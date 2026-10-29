#pragma once

#include <cassert>
#include <memory>

namespace UTILS {
    template <typename T>
    class Singleton {
        friend T;

        inline static T* instance_ = nullptr;
    protected:
        Singleton() = default;
    public:
        virtual ~Singleton() { instance_ = nullptr; }

        [[nodiscard]] static std::shared_ptr<T> createInstance() {
            assert(!instance_ && "Singleton instance already exists.");

            std::shared_ptr<T> instance = std::make_shared<T>();
            instance_ = instance.get();

            return instance;
        }

        static T& getInstance() {
            assert(instance_ && "Singleton instance does not exist.");
            return *instance_;
        }

        static T* getInstancePtr() {
            return instance_;
        }

        Singleton(Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&) = delete;
        Singleton& operator=(Singleton&&) = delete;
    };
}
