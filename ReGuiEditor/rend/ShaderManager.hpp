#pragma once
#include "Resources/Shaders/ShaderBlob.hpp"
#include "Resources/Shaders/ShaderResource.hpp"

#include "Common/RefCounter.hpp"

#include <Modules/Math/Hashing.hpp>
#include <Modules/Misc/Strings.hpp>

#include <UTILS/Singleton.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <d3dcommon.h>

namespace rend {
	class ShaderManager : public UTILS::Singleton<ShaderManager> {
		struct ShaderData {
			std::string shaderPath;
			std::string entrypoint;
			std::vector<std::string> macros;

			rend::ShaderType shaderType = rend::ShaderType::Vertex;
			rend::ShaderBlob** shaderBlob = nullptr;

			uint64_t hash = 0;

			void computeHash() {
				this->hash = 0;

				ReGuiEditor::hashCombine(this->hash, this->shaderPath);
				ReGuiEditor::hashCombine(this->hash, this->entrypoint);
				for (const auto& macro : this->macros)
					ReGuiEditor::hashCombine(this->hash, macro);
				
				ReGuiEditor::hashCombine(this->hash, static_cast<uint64_t>(this->shaderType));
			}

			std::vector<D3D_SHADER_MACRO> getD3DMacros(const char* shaderDefine) const {
				std::vector<D3D_SHADER_MACRO> d3dMacros;
				d3dMacros.reserve(this->macros.size() + 2);
				d3dMacros.emplace_back(shaderDefine, "1");

				for (const std::string& macro : this->macros)
					d3dMacros.emplace_back(macro.c_str(), "1");

				d3dMacros.emplace_back(nullptr, nullptr);
				return d3dMacros;
			}

			std::string getShaderFilename() const {
				std::string filename = this->shaderPath;
				size_t lastSlashPos = filename.find_last_of("/\\");
				if (lastSlashPos != std::string::npos)
					filename = filename.substr(lastSlashPos + 1);

				size_t lastDotPos = filename.find_last_of('.');
				if (lastDotPos != std::string::npos)
					filename = filename.substr(0, lastDotPos);

				return filename;
			}

			std::string generateCacheFilepath(const char* shaderTypeName) const {
				const std::string shaderFilename = this->getShaderFilename();
				return ReGuiEditor::formatString("$RGE_CACHE_DATA/Shaders/shader_%s_%s_%016llx.cso", shaderTypeName, shaderFilename.c_str(), this->hash);
			}
		};

		[[nodiscard]] bool loadFromCache(const ShaderData& shaderData);
		[[nodiscard]] bool storeToCache(const ShaderData& shaderData, ID3DBlob* shaderBlob, const std::vector<std::string>& includes);

		[[nodiscard]] bool createBlob(const ShaderData& shaderData, rend::RefCountPtr<ID3DBlob>& outputShaderBlob, std::vector<std::string>& outputIncludes);
	public:
		[[nodiscard]] bool loadBlob(std::string shaderPath, std::string entrypoint, std::vector<std::string> macros, rend::ShaderType shaderType, rend::ShaderBlob** outputBlob);
	};
}