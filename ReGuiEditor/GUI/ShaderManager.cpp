#include "ShaderManager.hpp"

#include "../Application.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <UTILS/Strings.hpp> // VCIC-Excluded: Includer cleaner is being a dum dum

#include <Windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

class IncludeHandler : public ID3DInclude {
public:
	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override {
		if (IncludeType != D3D_INCLUDE_LOCAL) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "IncludeHandler::Open: Invalid IncludeType.");
			return E_INVALIDARG;
		}

		std::filesystem::path filePath = pFileName;
		if (filePath.string().find('$') == std::string::npos)
			filePath = "$RGE_DATA/Shaders/" / filePath;

		std::string fileContents;
		if (!FilesystemManager::getInstance().readFile(filePath.string(), fileContents)) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "IncludeHandler::Open: Failed to read included shader file %s.", filePath.string().c_str());
			return E_FAIL;
		}

		fileContents += "\n";

		char* buffer = new char[fileContents.size()];
		memcpy(buffer, fileContents.data(), fileContents.size());

		*ppData = buffer;
		*pBytes = static_cast<UINT>(fileContents.size());
		return S_OK;
	}

	HRESULT Close(LPCVOID pData) override {
		delete[] static_cast<const char*>(pData);
		return S_OK;
	}
};

static IncludeHandler gIncludeHandler;

inline static std::string fixD3DCompileMessage(std::string errorMessage) {
	size_t firstDollar = errorMessage.find('$');
	if (firstDollar != std::string::npos)
		errorMessage = errorMessage.substr(firstDollar);
	
	while (!errorMessage.empty() && (errorMessage.back() == '\0' || errorMessage.back() == '\n'))
		errorMessage.pop_back();

	return errorMessage;
}

bool ShaderManager::compileShaderFromFile(const ShaderManager::ShaderParameters& params, ID3DBlob** blobOutput) {
	const std::string cacheFilePath = params.generateCacheFilePath();
	if (!this->loadFromCache(cacheFilePath, blobOutput))
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderManager::compileShaderFromFile: Failed to load shader from cache: %s", cacheFilePath.c_str());

	std::string fileContents;
	if (!FilesystemManager::getInstance().readFile(params.filePath, fileContents)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ShaderManager::compileShaderFromFile: Failed to read shader file %s.", params.filePath.c_str());
		return false;
	}

	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
	compileFlags |= D3DCOMPILE_DEBUG;
#else
	compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	
	const std::vector<D3D_SHADER_MACRO> macroDefines = params.generateMacroDefines();

	ID3DBlob* errorBlob = nullptr;
	HRESULT result = D3DCompile(fileContents.c_str(), fileContents.size(), params.filePath.c_str(), macroDefines.data(), &gIncludeHandler, params.entryPoint.c_str(), params.getShaderModel(), compileFlags, 0, blobOutput, &errorBlob);
	if (FAILED(result)) {
		if (errorBlob) {
			std::string errorMessage(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
			errorMessage = fixD3DCompileMessage(errorMessage);
			
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ShaderManager::compileShaderFromFile: %s", errorMessage.c_str());
			errorBlob->Release();
		} else {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ShaderManager::compileShaderFromFile: %s: unknown error, hresult: 0x%X", params.filePath.c_str(), result);
		}

		return false;
	}

	if (errorBlob) {
		std::string warningMessage(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
		warningMessage = fixD3DCompileMessage(warningMessage);

		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderManager::compileShaderFromFile: %s", warningMessage.c_str());
		errorBlob->Release();
	}

	if (!this->saveToCache(cacheFilePath, *blobOutput))
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderManager::compileShaderFromFile: Failed to save shader to cache: %s", cacheFilePath.c_str());

	CONSOLE_LOG(LEVEL_SHADER, COLOR_GREEN, "ShaderManager::compileShaderFromFile: Compiled shader \"%s\" (%s, %s)", params.filePath.c_str(), params.entryPoint.c_str(), params.getShaderModel());
	return true;
}

bool ShaderManager::compileVertexShader(const ShaderManager::ShaderParameters& params, ID3D11VertexShader** shaderOutput, std::string* shaderBytecode) {
	ID3DBlob* vertexBlob = nullptr;
	if (!this->compileShaderFromFile(params, &vertexBlob))
		return false;

	if (shaderBytecode)
		shaderBytecode->assign(static_cast<const char*>(vertexBlob->GetBufferPointer()), vertexBlob->GetBufferSize());
		
	HRESULT result = Application::getInstance().d3d11Device.getDevice()->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr, shaderOutput);
	vertexBlob->Release();

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ShaderManager::compileVertexShader: Failed to create vertex shader. HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

bool ShaderManager::compilePixelShader(const ShaderManager::ShaderParameters& params, ID3D11PixelShader** shaderOutput, std::string* shaderBytecode) {
	ID3DBlob* pixelBlob = nullptr;
	if (!this->compileShaderFromFile(params, &pixelBlob))
		return false;

	if (shaderBytecode)
		shaderBytecode->assign(static_cast<const char*>(pixelBlob->GetBufferPointer()), pixelBlob->GetBufferSize());

	HRESULT result = Application::getInstance().d3d11Device.getDevice()->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr, shaderOutput);
	pixelBlob->Release();

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ShaderManager::compilePixelShader: Failed to create pixel shader. HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

bool ShaderManager::loadFromCache(const std::string& filePath, ID3DBlob** blobOutput) {
	std::string blobContent;
	if (!FilesystemManager::getInstance().readFile(filePath, blobContent)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderManager::loadFromCache: Failed to read shader blob file %s.", filePath.c_str());
		return false;
	}

	HRESULT result = D3DCreateBlob(blobContent.size(), blobOutput);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderManager::loadFromCache: Failed to create blob for shader %s. HRESULT: 0x%X", filePath.c_str(), result);
		return false;
	}

	memcpy((*blobOutput)->GetBufferPointer(), blobContent.data(), blobContent.size());
	return true;
}

bool ShaderManager::saveToCache(const std::string& filePath, ID3DBlob* blob) {
	if (!blob)
		return false;

	std::string blobContent(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
	if (!FilesystemManager::getInstance().writeFile(filePath, blobContent)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderManager::saveToCache: Failed to write shader blob file %s.", filePath.c_str());
		return false;
	}

	return true;
}

void ShaderManager::cleanup() {
	for (auto& [hash, shader] : this->cache_)
		shader->Release();
	
	this->cache_.clear();
}