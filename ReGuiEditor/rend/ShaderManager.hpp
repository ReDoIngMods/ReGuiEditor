#pragma once
#include <UTILS/Singleton.hpp>

#include <mutex>
#include <unordered_map>

#include "Resources/Shaders/ShaderBlob.hpp"
#include "Resources/Shaders/ShaderResource.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rend {
	class ShaderManager : public UTILS::Singleton<ShaderManager> {
		struct ShaderDependency {
			std::filesystem::path path;
			time_t writeTime = 0;
		};

		struct CachedShader {
			rend::ShaderBlob* blob = nullptr;
			std::vector<ShaderDependency> dependencies;
		};

		std::unordered_map<uint64_t, CachedShader> shaderCache_;
		std::mutex shaderCacheMutex_;

		[[nodiscard]] uint64_t computeShaderHash(const std::string& shaderPath, const std::string& entrypoint, const std::vector<std::string>& macros, rend::ShaderType shaderType);
		[[nodiscard]] bool loadBlobFromCache(uint64_t shaderHash, const char* shaderTypename, rend::ShaderBlob** outputBlob);
	public:
		~ShaderManager() override;

		[[nodiscard]] bool loadBlob(const std::string& shaderPath, const std::string& entrypoint, std::vector<std::string> macros, rend::ShaderType shaderType, rend::ShaderBlob** outputBlob);
	};
}