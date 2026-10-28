#include "ShaderManager.hpp"

#include "Common/RefCounter.hpp"
#include "Resources/Shaders/ShaderBlob.hpp"
#include "Resources/Shaders/ShaderResource.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <UTILS/ByteStreamWriter.hpp>

#include <LZ4/lz4.h>

#include <Windows.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

struct ShaderCacheHeader {
	static constexpr uint32_t Magic = 0x52474543;
	static constexpr uint32_t Version = 4;

	uint32_t magic = Magic;
	uint32_t version = Version;
	int64_t lastWriteTime = 0;

	uint32_t uncompressedSize = 0;
	uint32_t compressedSize = 0;

	uint64_t hash = 0;

	[[nodiscard]] bool isValid() const {
		return this->magic == Magic && this->version == Version;
	}
};

class CustomIncludeHandler : public ID3DInclude {
	std::vector<std::string> discoveredFiles_;
	std::string baseDirectory_;
public:
	CustomIncludeHandler(const std::string& baseDirectory) : baseDirectory_(baseDirectory) {}

	const std::vector<std::string>& getDiscoveredFiles() const {
		return this->discoveredFiles_;
	}

	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override {
		const std::filesystem::path path = std::filesystem::path(this->baseDirectory_) / pFileName;
		
		std::vector<uint8_t> fileContent;
		if (!FilesystemManager::getInstance().readFile(path.string(), fileContent)) {
			CONSOLE_ERROR("Failed to read included shader file '%s'.", path.string().c_str());
			return E_FAIL;
		}

		const std::string pathString = path.string();
		if (std::find(this->discoveredFiles_.begin(), this->discoveredFiles_.end(), pathString) == this->discoveredFiles_.end())
			this->discoveredFiles_.push_back(pathString);

		uint8_t* buffer = new uint8_t[fileContent.size()];
		memcpy(buffer, fileContent.data(), fileContent.size());

		*ppData = buffer;
		*pBytes = static_cast<UINT>(fileContent.size());
		
		return S_OK;
	}

	HRESULT Close(LPCVOID pData) override {
		delete[] reinterpret_cast<const uint8_t*>(pData);
		return S_OK;
	}
};

struct ShaderInfo {
	const char* define = nullptr;
	const char* targetProfile = nullptr;
	const char* type = nullptr;

	[[nodiscard]] bool isValid() const {
		return this->define != nullptr && this->targetProfile != nullptr && this->type != nullptr;
	}
};

inline static const ShaderInfo& getShaderInfo(rend::ShaderType shaderType) {
	static const std::array<ShaderInfo, 2> shaderInfos = { {
		{ "VERTEX_SHADER", "vs_5_0", "vs" },
		{ "PIXEL_SHADER",  "ps_5_0", "ps" }
	} };

	if (static_cast<size_t>(shaderType) >= shaderInfos.size()) {
		static const ShaderInfo invalidShaderInfo;
		return invalidShaderInfo;
	}

	return shaderInfos[static_cast<size_t>(shaderType)];
}

bool rend::ShaderManager::loadFromCache(const ShaderData& shaderData) {
	const ShaderInfo& shaderInfo = getShaderInfo(shaderData.shaderType);
	if (!shaderInfo.isValid()) {
		CONSOLE_ERROR("Invalid shader type specified for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	const std::string cacheFilepath = shaderData.generateCacheFilepath(shaderInfo.type);
	const FilesystemManager& filesystemManager = FilesystemManager::getInstance();
	if (!filesystemManager.exists(cacheFilepath))
		return false;

	std::vector<uint8_t> cachedData;
	if (!filesystemManager.readFile(cacheFilepath, cachedData))
		return false;

	if (cachedData.size() < sizeof(ShaderCacheHeader)) {
		CONSOLE_WARNING("Shader cache file '%s' has an incomplete header.", cacheFilepath.c_str());
		return false;
	}

	size_t offset = 0;
	const auto readRaw = [&](void* destination, size_t size) {
		if (cachedData.size() - offset < size)
			return false;

		std::memcpy(destination, cachedData.data() + offset, size);
		offset += size;
		return true;
	};

	ShaderCacheHeader header;
	readRaw(&header, sizeof(header));
	if (!header.isValid()) {
		CONSOLE_WARNING("Invalid shader cache header in file '%s'.", cacheFilepath.c_str());
		return false;
	}

	if (header.hash != shaderData.hash) {
		CONSOLE_WARNING("Shader file '%s' has a different hash than the cache. Cache is invalid.", shaderData.shaderPath.c_str());
		return false;
	}

	time_t shaderFileWriteTime = 0;
	if (!filesystemManager.getFileWriteTime(shaderData.shaderPath, shaderFileWriteTime))
		return false;

	if (static_cast<int64_t>(shaderFileWriteTime) != header.lastWriteTime) {
		CONSOLE_WARNING("Shader file '%s' has been modified since the cache was created. Cache is invalid.", shaderData.shaderPath.c_str());
		return false;
	}

	if (header.uncompressedSize == 0 || header.compressedSize == 0 || header.uncompressedSize > static_cast<uint32_t>(INT_MAX) || header.compressedSize > static_cast<uint32_t>(INT_MAX)) {
		CONSOLE_WARNING("Invalid shader cache sizes in file '%s'.", cacheFilepath.c_str());
		return false;
	}

	uint32_t includeCount = 0;
	if (!readRaw(&includeCount, sizeof(includeCount))) {
		CONSOLE_WARNING("Shader cache file '%s' has no include table.", cacheFilepath.c_str());
		return false;
	}

	for (uint32_t index = 0; index < includeCount; ++index) {
		int64_t includeWriteTime = 0;
		uint32_t pathLength = 0;
		if (!readRaw(&includeWriteTime, sizeof(includeWriteTime)) || !readRaw(&pathLength, sizeof(pathLength)) || cachedData.size() - offset < pathLength) {
			CONSOLE_WARNING("Corrupt include table in shader cache file '%s'.", cacheFilepath.c_str());
			return false;
		}

		const std::string includePath(reinterpret_cast<const char*>(cachedData.data()) + offset, pathLength);
		offset += pathLength;

		time_t currentWriteTime = 0;
		if (!filesystemManager.getFileWriteTime(includePath, currentWriteTime) || static_cast<int64_t>(currentWriteTime) != includeWriteTime) {
			CONSOLE_WARNING("Included file '%s' of shader '%s' is missing or modified. Cache is invalid.", includePath.c_str(), shaderData.shaderPath.c_str());
			return false;
		}
	}

	if (cachedData.size() - offset != header.compressedSize) {
		CONSOLE_WARNING("Invalid compressed shader data size in cache file '%s'.", cacheFilepath.c_str());
		return false;
	}

	const char* compressedData = reinterpret_cast<const char*>(cachedData.data()) + offset;

	std::vector<char> uncompressedData(header.uncompressedSize);
	const int decompressedSize = LZ4_decompress_safe(compressedData, uncompressedData.data(), static_cast<int>(header.compressedSize), static_cast<int>(header.uncompressedSize));
	if (decompressedSize != static_cast<int>(header.uncompressedSize)) {
		CONSOLE_WARNING("Failed to decompress shader data from cache file '%s'.", cacheFilepath.c_str());
		return false;
	}

	ID3DBlob* shaderBlob = nullptr;
	const HRESULT result = D3DCreateBlob(static_cast<SIZE_T>(header.uncompressedSize), &shaderBlob);
	if (FAILED(result) || !shaderBlob) {
		CONSOLE_WARNING("Failed to create shader blob for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	memcpy(shaderBlob->GetBufferPointer(), uncompressedData.data(), static_cast<size_t>(header.uncompressedSize));

	*shaderData.shaderBlob = new rend::ShaderBlob(shaderBlob, shaderData.hash);
	return true;
}

bool rend::ShaderManager::storeToCache(const ShaderData& shaderData, ID3DBlob* shaderBlob, const std::vector<std::string>& includes) {
	const ShaderInfo& shaderInfo = getShaderInfo(shaderData.shaderType);
	if (!shaderInfo.isValid()) {
		CONSOLE_ERROR("Invalid shader type specified for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	if (shaderBlob == nullptr || shaderBlob->GetBufferPointer() == nullptr)
		return false;

	const SIZE_T blobSize = shaderBlob->GetBufferSize();
	if (blobSize == 0 || blobSize > static_cast<SIZE_T>(INT_MAX) || blobSize > std::numeric_limits<uint32_t>::max()) {
		CONSOLE_ERROR("Invalid compiled shader size for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	const int compressedBound = LZ4_compressBound(static_cast<int>(blobSize));
	if (compressedBound <= 0) {
		CONSOLE_ERROR("Failed to compute compressed size for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	std::vector<char> compressedData(static_cast<size_t>(compressedBound));

	const int actualCompressedSize = LZ4_compress_default(static_cast<const char*>(shaderBlob->GetBufferPointer()), compressedData.data(), static_cast<int>(blobSize), compressedBound);
	if (actualCompressedSize <= 0) {
		CONSOLE_ERROR("Failed to compress compiled shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	compressedData.resize(static_cast<size_t>(actualCompressedSize));

	time_t shaderFileWriteTime = 0;
	const FilesystemManager& filesystemManager = FilesystemManager::getInstance();
	if (!filesystemManager.getFileWriteTime(shaderData.shaderPath, shaderFileWriteTime)) {
		CONSOLE_ERROR("Failed to get last write time for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	ShaderCacheHeader header;
	header.lastWriteTime = static_cast<int64_t>(shaderFileWriteTime);
	header.uncompressedSize = static_cast<uint32_t>(blobSize);
	header.compressedSize = static_cast<uint32_t>(actualCompressedSize);
	header.hash = shaderData.hash;

	UTILS::ByteStreamWriter writer;
	writer.write(header);
	writer.write(static_cast<uint32_t>(includes.size()));
	for (const std::string& includePath : includes) {
		time_t includeWriteTime = 0;
		if (!filesystemManager.getFileWriteTime(includePath, includeWriteTime)) {
			CONSOLE_ERROR("Failed to get last write time for included file '%s'.", includePath.c_str());
			return false;
		}

		writer.write(static_cast<int64_t>(includeWriteTime));
		writer.write(static_cast<uint32_t>(includePath.size()));
		writer.writeBytes(includePath.data(), includePath.size());
	}

	writer.writeBytes(compressedData.data(), compressedData.size());

	const std::string cacheFilepath = shaderData.generateCacheFilepath(shaderInfo.type);
	const std::vector<char> cacheData = writer.toVector();

	if (!filesystemManager.writeFile(cacheFilepath, cacheData)) {
		CONSOLE_WARNING("Failed to write shader cache file '%s'.", cacheFilepath.c_str());
		return false;
	}

	return true;
}

bool rend::ShaderManager::createBlob(const ShaderData& shaderData, rend::RefCountPtr<ID3DBlob>& outputShaderBlob, std::vector<std::string>& outputIncludes) {
	const ShaderInfo& shaderInfo = getShaderInfo(shaderData.shaderType);
	if (!shaderInfo.isValid()) {
		CONSOLE_ERROR("Invalid shader type specified for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	std::string shaderSource;
	if (!FilesystemManager::getInstance().readFile(shaderData.shaderPath, shaderSource)) {
		CONSOLE_ERROR("Failed to read shader file '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	const std::vector<D3D_SHADER_MACRO> d3dMacros = shaderData.getD3DMacros(shaderInfo.define);

	CustomIncludeHandler includeHandler(std::filesystem::path(shaderData.shaderPath).parent_path().string());

	rend::RefCountPtr<ID3DBlob> shaderBlob = nullptr;
	rend::RefCountPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT result = D3DCompile(shaderSource.c_str(), shaderSource.size(), shaderData.shaderPath.c_str(), d3dMacros.data(), &includeHandler, shaderData.entrypoint.c_str(), shaderInfo.targetProfile, 0, 0, &shaderBlob, &errorBlob);
	if (FAILED(result)) {
		if (errorBlob)
			CONSOLE_ERROR("Failed to compile shader '%s'\r\n%s", shaderData.shaderPath.c_str(), static_cast<const char*>(errorBlob->GetBufferPointer()));
		else
			CONSOLE_ERROR("Failed to compile shader '%s'\r\nunknown error", shaderData.shaderPath.c_str());

		return false;
	}

	if (errorBlob)
		CONSOLE_WARNING("Shader compilation for '%s' produced warnings\r\n%s", shaderData.shaderPath.c_str(), static_cast<const char*>(errorBlob->GetBufferPointer()));

	if (!shaderBlob) {
		CONSOLE_ERROR("Shader compilation succeeded but no shader blob was produced for shader '%s'.", shaderData.shaderPath.c_str());
		return false;
	}

	outputShaderBlob = shaderBlob;
	outputIncludes = includeHandler.getDiscoveredFiles();
	*shaderData.shaderBlob = new rend::ShaderBlob(shaderBlob, shaderData.hash);
	return true;
}

bool rend::ShaderManager::loadBlob(std::string shaderPath, std::string entrypoint, std::vector<std::string> macros, rend::ShaderType shaderType, rend::ShaderBlob** outputBlob) {
	if (outputBlob == nullptr)
		return false;

	ShaderData shaderData;
	shaderData.shaderPath = std::move(shaderPath);
	shaderData.entrypoint = std::move(entrypoint);
	shaderData.macros = std::move(macros);
	shaderData.shaderType = shaderType;
	shaderData.shaderBlob = outputBlob;

	shaderData.computeHash();

	if (this->loadFromCache(shaderData))
		return true;

	std::string definesList;
	definesList.reserve(shaderData.macros.size() * 32);

	for (size_t index = 0; index < shaderData.macros.size(); ++index) {
		definesList += "\"";
		definesList += shaderData.macros[index];
		definesList += "\"";

		if (index < shaderData.macros.size() - 1)
			definesList += ", ";
	}

	CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "CACHE MISS! shaderPath='%s', entrypoint='%s', shaderType=%d, defines=[%s], hash=%016llx", shaderData.shaderPath.c_str(), shaderData.entrypoint.c_str(), static_cast<int>(shaderData.shaderType), definesList.c_str(), shaderData.hash);

	rend::RefCountPtr<ID3DBlob> shaderBlob = nullptr;
	std::vector<std::string> includes;
	if (!this->createBlob(shaderData, shaderBlob, includes))
		return false;

	if (!this->storeToCache(shaderData, shaderBlob, includes))
		CONSOLE_WARNING("Failed to store shader '%s' to cache.", shaderData.shaderPath.c_str());

	return true;
}