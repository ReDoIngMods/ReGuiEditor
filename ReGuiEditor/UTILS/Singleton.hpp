#pragma once
#include <cassert>

namespace UTILS {
	template <typename T>
	class Singleton {
		friend T;

		inline static T* instance_ = nullptr;
	protected:
		Singleton() = default;
	public:
		virtual ~Singleton() = default;

		static T* createInstance() {
			assert(instance_ == nullptr && "Singleton instance already exists.");
			instance_ = new T();
			return instance_;
		}

		static T& getInstance() {
			assert(instance_ != nullptr && "Singleton instance does not exist.");
			return *instance_;
		}

		static T* getInstancePtr() {
			return instance_;
		}

		static void destroyInstance() {
			assert(instance_ != nullptr && "Singleton instance does not exist.");
			delete instance_;
			instance_ = nullptr;
		}

		Singleton(const Singleton&) = delete;
		Singleton& operator=(const Singleton&) = delete;
		Singleton(Singleton&&) = delete;
		Singleton& operator=(Singleton&&) = delete;
	};
}