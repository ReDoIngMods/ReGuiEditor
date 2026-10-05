#include "ShaderManager.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include "Resources/Shaders/ShaderBlob.hpp"
#include "Resources/Shaders/ShaderResource.hpp"

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>
#include <UTILS/Strings.hpp>

#include <Windows.h>
#include <d3dcommon.h>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr char SHADER_CACHE_MAGIC[4] = { 'R', 'S', 'H', 'C' }; // RGE Shader Cache
constexpr uint32_t SHADER_CACHE_VERSION = 1;

constexpr uint32_t SHADER_CACHE_MAX_DEPENDENCIES = 4096;

template<typename DependencyVector>
inline static bool dependenciesUpToDate(const DependencyVector& dependencies) {
	if (dependencies.empty())
		return false;

	FilesystemManager& filesystemManager = FilesystemManager::getInstance();
	for (const auto& dependency : dependencies) {
		time_t currentWriteTime = 0;
		if (!filesystemManager.getFileWriteTimeRaw(dependency.path, currentWriteTime))
			return false;

		if (currentWriteTime != dependency.writeTime)
			return false;
	}

	return true;
}

template<typename DependencyVector>
class ShaderIncludeHandler : public ID3DInclude {
	std::filesystem::path rootDirectory_;

	DependencyVector& dependencies_;
	std::unordered_map<const void*, std::filesystem::path> openDirectories_;
public:
	ShaderIncludeHandler(std::filesystem::path rootDirectory, DependencyVector& dependencies) : rootDirectory_(std::move(rootDirectory)), dependencies_(dependencies) {}

	HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID parentData, LPCVOID* outputData, UINT* outputBytes) override {
		std::filesystem::path baseDirectory = this->rootDirectory_;
		if (const auto parent = this->openDirectories_.find(parentData); parentData && parent != this->openDirectories_.end())
			baseDirectory = parent->second;

		const std::filesystem::path includePath = (baseDirectory / fileName).lexically_normal();

		FilesystemManager& filesystemManager = FilesystemManager::getInstance();

		time_t writeTime = 0;
		if (!filesystemManager.getFileWriteTimeRaw(includePath, writeTime))
			return E_FAIL;

		std::vector<char> content;
		if (!filesystemManager.readFileRaw(includePath, content))
			return E_FAIL;

		char* buffer = new char[std::max<size_t>(content.size(), 1)];
		if (!content.empty())
			memcpy(buffer, content.data(), content.size());

		this->openDirectories_[buffer] = includePath.parent_path();

		const std::string includePathString = includePath.string();
		const bool known = std::any_of(this->dependencies_.begin(), this->dependencies_.end(), [&](const auto& dependency) { return dependency.path == includePathString; });
		if (!known)
			this->dependencies_.push_back({ includePathString, writeTime });
		
		*outputData = buffer;
		*outputBytes = static_cast<UINT>(content.size());

		return S_OK;
	}

	HRESULT __stdcall Close(LPCVOID data) override {
		this->openDirectories_.erase(data);
		delete[] static_cast<const char*>(data);
		
		return S_OK;
	}
};

uint64_t rend::ShaderManager::computeShaderHash(const std::string& shaderPath, const std::string& entrypoint, const std::vector<std::string>& macros, rend::ShaderType shaderType) {
	std::hash<std::string> hasher;

	uint64_t hash = hasher(shaderPath);
	hash ^= hasher(entrypoint) + 0x9E3779B97F4A7C15ULL + (hash << 6) + (hash >> 2);
	hash ^= static_cast<uint64_t>(shaderType) + 0x9E3779B97F4A7C15ULL + (hash << 6) + (hash >> 2);

	for (const std::string& macro : macros)
		hash ^= hasher(macro) + 0x9E3779B97F4A7C15ULL + (hash << 6) + (hash >> 2);

	return hash;
}

bool rend::ShaderManager::loadBlobFromCache(uint64_t shaderHash, const char* shaderTypename, rend::ShaderBlob** outputBlob) {
	const std::string cacheFilePath = UTILS::formatString("$RGE_CACHE_DATA/Shaders/shader_%s_%llx.cso", shaderTypename, shaderHash);

	std::lock_guard<std::mutex> lock(this->shaderCacheMutex_);

	const auto iterator = this->shaderCache_.find(shaderHash);
	if (iterator != this->shaderCache_.end()) {
		if (dependenciesUpToDate(iterator->second.dependencies)) {
			iterator->second.blob->AddRef();

			*outputBlob = iterator->second.blob;
			return true;
		}

		CONSOLE_LOG(LEVEL_CACHE, COLOR_YELLOW, "Shader source or include modified, evicting loaded shader: %s", cacheFilePath.c_str());

		iterator->second.blob->Release();
		this->shaderCache_.erase(iterator);
	}

	std::vector<char> cacheContent;
	if (!FilesystemManager::getInstance().readFile(cacheFilePath, cacheContent))
		return false;

	UTILS::ByteStreamReader reader(cacheContent.data(), cacheContent.size());

	char magic[4] = {};
	if (!reader.readBytes(magic, sizeof(magic)) || memcmp(magic, SHADER_CACHE_MAGIC, sizeof(magic)) != 0 || reader.read<uint32_t>() != SHADER_CACHE_VERSION) {
		// Also catches cache files from before dependency tracking existed, which were bare bytecode.
		CONSOLE_LOG(LEVEL_CACHE, COLOR_YELLOW, "Shader cache file has an unknown format, ignoring: %s", cacheFilePath.c_str());
		return false;
	}

	const uint32_t dependencyCount = reader.read<uint32_t>();
	if (dependencyCount == 0 || dependencyCount > SHADER_CACHE_MAX_DEPENDENCIES) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Shader cache file has an invalid dependency list: %s", cacheFilePath.c_str());
		return false;
	}

	std::vector<ShaderDependency> dependencies;
	dependencies.reserve(dependencyCount);

	for (uint32_t i = 0; i < dependencyCount; i++) {
		ShaderDependency dependency;
		dependency.path = reader.readString();
		dependency.writeTime = reader.read<int64_t>();

		if (dependency.path.empty()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Shader cache file is corrupt: %s", cacheFilePath.c_str());
			return false;
		}

		dependencies.push_back(std::move(dependency));
	}

	if (!dependenciesUpToDate(dependencies)) {
		CONSOLE_LOG(LEVEL_CACHE, COLOR_YELLOW, "Shader source or include modified, invalidating cache: %s", dependencies.front().path.c_str());
		return false;
	}

	const uint32_t bytecodeSize = reader.read<uint32_t>();
	if (bytecodeSize == 0 || bytecodeSize != reader.getRemaining()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Shader cache file has an invalid bytecode size: %s", cacheFilePath.c_str());
		return false;
	}

	ID3DBlob* compiledBlob = nullptr;
	HRESULT result = D3DCreateBlob(bytecodeSize, &compiledBlob);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ID3DBlob for cached shader: %s", cacheFilePath.c_str());
		return false;
	}

	if (!reader.readBytes(static_cast<char*>(compiledBlob->GetBufferPointer()), bytecodeSize)) {
		compiledBlob->Release();
		return false;
	}

	ID3DBlob* signatureBlob = nullptr;
	result = D3DGetInputSignatureBlob(compiledBlob->GetBufferPointer(), compiledBlob->GetBufferSize(), &signatureBlob);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid/corrupt shader bytecode: %s", cacheFilePath.c_str());

		compiledBlob->Release();
		return false;
	}

	signatureBlob->Release();

	rend::ShaderBlob* shaderBlob = new rend::ShaderBlob(compiledBlob, shaderHash);
	compiledBlob->Release();

	this->shaderCache_.emplace(shaderHash, CachedShader{ shaderBlob, std::move(dependencies) });

	shaderBlob->AddRef();
	*outputBlob = shaderBlob;
	return true;
}

rend::ShaderManager::~ShaderManager() {
	CONSOLE_LOG(LEVEL_SHADER, COLOR_GRAY, "Shutting down ShaderManager...");

	std::lock_guard<std::mutex> lock(this->shaderCacheMutex_);
	for (auto& pair : this->shaderCache_)
		pair.second.blob->Release();

	this->shaderCache_.clear();
}

bool rend::ShaderManager::loadBlob(const std::string& shaderPath, const std::string& entrypoint, std::vector<std::string> macros, rend::ShaderType shaderType, rend::ShaderBlob** outputBlob) {
	const char* shaderTypename = nullptr;
	const char* targetProfile = nullptr;

	switch (shaderType) {
	case rend::ShaderType::Vertex:
		shaderTypename = "vs";
		targetProfile = "vs_5_0";
		macros.push_back("VERTEX_SHADER");
		break;
	case rend::ShaderType::Pixel:
		shaderTypename = "ps";
		targetProfile = "ps_5_0";
		macros.push_back("PIXEL_SHADER");
		break;
	default:
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unsupported shader type: %d", static_cast<int>(shaderType));
		return false;
	}

	const uint64_t shaderHash = this->computeShaderHash(shaderPath, entrypoint, macros, shaderType);
	if (this->loadBlobFromCache(shaderHash, shaderTypename, outputBlob))
		return true;

	CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "Cache miss! ShaderPath=%s, entrypoint=%s, macros=<count %zu>, shaderType=%d, shaderHash=%llx", shaderPath.c_str(), entrypoint.c_str(), macros.size(), static_cast<int>(shaderType), shaderHash);

	std::vector<D3D_SHADER_MACRO> d3dMacros;
	for (const std::string& macro : macros) {
		D3D_SHADER_MACRO d3dMacro;
		ZeroMemory(&d3dMacro, sizeof(D3D_SHADER_MACRO));
		d3dMacro.Name = macro.c_str();
		d3dMacro.Definition = "1";

		d3dMacros.push_back(d3dMacro);
	}

	d3dMacros.push_back({ nullptr, nullptr });

	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	std::filesystem::path resolvedPath;
	if (!filesystemManager.resolveDirectory(shaderPath, resolvedPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resolve shader path: %s", shaderPath.c_str());
		return false;
	}

	std::vector<ShaderDependency> dependencies;

	ShaderDependency sourceDependency;
	sourceDependency.path = resolvedPath.lexically_normal().string();

	if (!filesystemManager.getFileWriteTimeRaw(resolvedPath, sourceDependency.writeTime)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get write time of shader source: %s", resolvedPath.string().c_str());
		return false;
	}

	dependencies.push_back(std::move(sourceDependency));

	std::vector<char> sourceCode;
	if (!FilesystemManager::getInstance().readFileRaw(resolvedPath, sourceCode)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read shader source: %s", resolvedPath.string().c_str());
		return false;
	}

	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG) || defined(DEBUG)
	compileFlags |= D3DCOMPILE_DEBUG;
#else
	compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	ID3DBlob* compiledBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	ShaderIncludeHandler<std::vector<ShaderDependency>> includeHandler(resolvedPath.parent_path(), dependencies);

	const std::string sourceName = resolvedPath.string();
	const HRESULT result = D3DCompile(sourceCode.data(), sourceCode.size(), sourceName.c_str(), d3dMacros.data(), &includeHandler, entrypoint.c_str(), targetProfile, compileFlags, 0, &compiledBlob, &errorBlob);
	if (FAILED(result)) {
		if (errorBlob) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Shader compilation error: %s", static_cast<const char*>(errorBlob->GetBufferPointer()));
			errorBlob->Release();
		}
		else {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Shader compilation failed with HRESULT: 0x%08X", result);
		}

		return false;
	}

	if (errorBlob) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Shader compilation warning: %s", static_cast<const char*>(errorBlob->GetBufferPointer()));
		errorBlob->Release();
	}

	rend::ShaderBlob* shaderBlob = new rend::ShaderBlob(compiledBlob, shaderHash);
	compiledBlob->Release();

	{
		std::lock_guard<std::mutex> lock(this->shaderCacheMutex_);
		auto [iterator, inserted] = this->shaderCache_.emplace(shaderHash, CachedShader{ shaderBlob, dependencies });
		
		if (!inserted) {
			if (dependenciesUpToDate(iterator->second.dependencies)) {
				shaderBlob->Release();
				iterator->second.blob->AddRef();

				*outputBlob = iterator->second.blob;
				return true;
			}

			iterator->second.blob->Release();
			iterator->second = CachedShader{ shaderBlob, dependencies };
		}

		shaderBlob->AddRef();
	}

	UTILS::ByteStreamWriter writer;
	writer.writeBytes(SHADER_CACHE_MAGIC, sizeof(SHADER_CACHE_MAGIC));
	writer.write<uint32_t>(SHADER_CACHE_VERSION);
	writer.write<uint32_t>(static_cast<uint32_t>(dependencies.size()));

	for (const ShaderDependency& dependency : dependencies) {
		writer.writeString(dependency.path.string());
		writer.write<int64_t>(dependency.writeTime);
	}

	const std::vector<char> shaderBytecode = shaderBlob->getBytecode();
	writer.write<uint32_t>(static_cast<uint32_t>(shaderBytecode.size()));
	writer.writeBytes(shaderBytecode.data(), shaderBytecode.size());

	const std::string cacheFilePath = UTILS::formatString("$RGE_CACHE_DATA/Shaders/shader_%s_%llx.cso", shaderTypename, shaderHash);
	if (!FilesystemManager::getInstance().writeFile(cacheFilePath, writer.toVector()))
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to write shader cache file: %s", cacheFilePath.c_str());

	*outputBlob = shaderBlob;
	return true;
}