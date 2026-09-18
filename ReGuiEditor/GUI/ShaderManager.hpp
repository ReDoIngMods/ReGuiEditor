#pragma once
#include <UTILS/Singleton.hpp>
#include <UTILS/Strings.hpp>

#include <d3d11.h>
#include <d3dcommon.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

class ShaderManager : public UTILS::Singleton<ShaderManager> {
	enum class ShaderType {
		Unknown,
		Vertex,
		Pixel
	};

	struct ShaderParameters {
		std::string filePath = "";
		std::string entryPoint = "";
		std::vector<std::string> defines = {};
		ShaderManager::ShaderType type = ShaderManager::ShaderType::Unknown;

		std::vector<D3D_SHADER_MACRO> generateMacroDefines() const {
			std::vector<D3D_SHADER_MACRO> macroDefines;

			switch (type) {
			case ShaderType::Vertex:
				macroDefines.push_back({ "VERTEX_SHADER", "1" });
				break;
			case ShaderType::Pixel:
				macroDefines.push_back({ "PIXEL_SHADER", "1" });
				break;
			case ShaderType::Unknown:
				CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderParameters::generateMacroDefines: ShaderType is Unknown. This may lead to unexpected behavior.");
				
				macroDefines.push_back({ "UNKNOWN_SHADER", "1" });
				break;
			default:
				CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ShaderParameters::generateMacroDefines: ShaderType is invalid. This may lead to unexpected behavior.");
				
				macroDefines.push_back({ "UNKNOWN_SHADER", "1" });
				break;
			}

			for (const std::string& define : this->defines)
				macroDefines.push_back({ define.c_str(), "1" });

			macroDefines.push_back({ nullptr, nullptr });
			return macroDefines;
		}

		uint64_t generateHash() const {
			std::hash<std::string> hasher;

			uint64_t hash = hasher(this->filePath);
			hash ^= hasher(this->entryPoint) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= static_cast<uint64_t>(this->type) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

			for (const std::string& define : this->defines)
				hash ^= hasher(define) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

			return hash;
		}

		std::string generateCacheFileName() const {
			const uint64_t shaderHash = this->generateHash();
			const std::string fileName = std::filesystem::path(filePath).stem().string();

			return UTILS::formatString("%016llx_%s.cso", shaderHash, fileName.c_str());
		}

		std::string generateCacheFilePath() const {
			return "$RGE_CACHE_DATA/Shaders/" + this->generateCacheFileName();
		}

		const char* getShaderModel() const {
			switch (this->type) {
			case ShaderManager::ShaderType::Vertex: return "vs_5_0";
			case ShaderManager::ShaderType::Pixel: return "ps_5_0";
			default: return "unknown";
			}
		}
	};
	
	bool compileShaderFromFile(const ShaderManager::ShaderParameters& params, ID3DBlob** blobOutput);

	bool compileVertexShader(const ShaderManager::ShaderParameters& params, ID3D11VertexShader** shaderOutput, std::string* shaderBytecode = nullptr);
	bool compilePixelShader(const ShaderManager::ShaderParameters& params, ID3D11PixelShader** shaderOutput, std::string* shaderBytecode = nullptr);

	bool loadFromCache(const std::string& filePath, ID3DBlob** blobOutput);
	bool saveToCache(const std::string& filePath, ID3DBlob* blob);

	std::unordered_map<uint64_t, ID3D11DeviceChild*> cache_;
public:
	ShaderManager() = default;
	~ShaderManager() { this->cleanup(); }

	void cleanup();

	template<typename T>
	bool getShader(const std::string& filePath, const std::string& entryPoint, const std::vector<std::string>& defines, T*& shaderOutput, std::string* shaderBytecode = nullptr) {
		static_assert(std::is_same_v<T, ID3D11VertexShader> || std::is_same_v<T, ID3D11PixelShader>, "getShader: T must be ID3D11VertexShader or ID3D11PixelShader");

		ShaderManager::ShaderParameters params;
		params.filePath = filePath;
		params.entryPoint = entryPoint;
		params.defines = defines;

		const uint64_t hash = params.generateHash();
		auto iterator = this->cache_.find(hash);
		if (iterator != this->cache_.end()) {
			shaderOutput = static_cast<T*>(iterator->second);
			shaderOutput->AddRef();
			return true;
		}

		if constexpr (std::is_same_v<T, ID3D11VertexShader>) {
			params.type = ShaderManager::ShaderType::Vertex;

			if (!this->compileVertexShader(params, &shaderOutput, shaderBytecode))
				return false;
		} else if constexpr (std::is_same_v<T, ID3D11PixelShader>) {
			params.type = ShaderManager::ShaderType::Pixel;

			if (!this->compilePixelShader(params, &shaderOutput, shaderBytecode))
				return false;
		} else {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ShaderManager::getShader: T must be ID3D11VertexShader or ID3D11PixelShader.");
			return false;
		}

		shaderOutput->AddRef();

		this->cache_[hash] = shaderOutput;
		return true;
	}

	template<typename T>
	bool getShader(const std::string& filePath, const std::string& entryPoint, T*& shaderOutput, std::string* shaderBytecode = nullptr) {
		return this->getShader(filePath, entryPoint, {}, shaderOutput, shaderBytecode);
	}
};