#pragma once
#include <rend/Common/RefCounter.hpp>
#include <rend/Unknown.hpp>

#include <d3dcommon.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace rend {
	class ShaderBlob : public rend::RefCounter<rend::Unknown> {
		rend::RefCountPtr<ID3DBlob> blob_ = nullptr;
		uint64_t hash_ = 0;
	public:
		ShaderBlob(ID3DBlob* blob, uint64_t hash) : blob_(blob), hash_(hash) {}

		[[nodiscard]] std::vector<char> getBytecode() const {
			if (!this->blob_)
				return {};

			std::vector<char> bytecode(this->blob_->GetBufferSize());
			memcpy(bytecode.data(), this->blob_->GetBufferPointer(), this->blob_->GetBufferSize());

			return bytecode;
		}

		[[nodiscard]] rend::RefCountPtr<ID3DBlob> getBlob() const { return this->blob_; }
		[[nodiscard]] uint64_t getHash() const { return this->hash_; }
	};

	typedef rend::RefCountPtr<rend::ShaderBlob> ShaderBlobHandle;
}