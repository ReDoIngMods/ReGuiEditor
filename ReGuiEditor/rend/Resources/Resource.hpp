#pragma once
#include "../Unknown.hpp"

#include <string>
#include <typeinfo>

namespace rend {
	class ResourceManager;

	class Resource : public rend::Unknown {
		friend class rend::ResourceManager;

		const char* typeName_ = nullptr;
		std::string debugName_;
	public:
		Resource();
		~Resource();

		void updateTypeName() { this->typeName_ = typeid(*this).name(); }
		void setDebugName(const std::string& name) { this->debugName_ = name; }

		[[nodiscard]] const char* getTypeName() const noexcept {
			if (!this->typeName_)
				const_cast<rend::Resource*>(this)->updateTypeName(); // ignore the const cast plz :)

			return this->typeName_;
		}

		[[nodiscard]] const char* getDebugName() const noexcept {
			return this->debugName_.empty() ? this->getTypeName() : this->debugName_.c_str();
		}
	};
}
