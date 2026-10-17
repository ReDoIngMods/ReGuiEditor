#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>
#include <type_traits>

namespace rend {
	template <typename T>
	class RefCountPtr  {
	protected:
		T* pointer_;

		template <class U>
		friend class RefCountPtr;

		void internalAddRef() const noexcept {
			if (this->pointer_ != nullptr)
				this->pointer_->AddRef();
		}

		uint32_t internalRelease() noexcept {
			uint32_t ref = 0;
			T* temp = this->pointer_;

			if (temp != nullptr) {
				this->pointer_ = nullptr;
				ref = temp->Release();
			}

			return ref;
		}
	public:
		RefCountPtr() noexcept : pointer_(nullptr) { }
		RefCountPtr(std::nullptr_t) noexcept : pointer_(nullptr) { }

		template <class U>
		RefCountPtr(U* other) noexcept : pointer_(other) {
			this->internalAddRef();
		}

		RefCountPtr(const RefCountPtr& other) noexcept : pointer_(other.pointer_) {
			this->internalAddRef();
		}


		template <class U>
		RefCountPtr(const RefCountPtr<U>& other, typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = nullptr) noexcept : pointer_(other.pointer_) {
			this->internalAddRef();
		}

		RefCountPtr(RefCountPtr&& other) noexcept : pointer_(nullptr) {
			if (this != reinterpret_cast<RefCountPtr*>(&reinterpret_cast<unsigned char&>(other)))
				this->swap(other);
		}

		~RefCountPtr() noexcept {
			this->internalRelease();
		}

		RefCountPtr& operator=(std::nullptr_t) noexcept {
			this->internalRelease();
			return *this;
		}

		RefCountPtr& operator=(const RefCountPtr& other) noexcept {
			if (this != &other)
				RefCountPtr(other).swap(*this);
		
			return *this;
		}

		template <typename U>
		RefCountPtr& operator=(U* other) noexcept {
			RefCountPtr(other).swap(*this);
			return *this;
		}

		template <class U>
		RefCountPtr& operator=(const RefCountPtr<U>& other) noexcept {
			RefCountPtr(other).swap(*this);
			return *this;
		}

		RefCountPtr& operator=(RefCountPtr&& other) noexcept {
			RefCountPtr(static_cast<RefCountPtr&&>(other)).swap(*this);
			return *this;
		}

		template <class U>
		RefCountPtr& operator=(RefCountPtr<U>&& other) noexcept {
			RefCountPtr(static_cast<RefCountPtr<U>&&>(other)).swap(*this);
			return *this;
		}

		void swap(RefCountPtr&& other) noexcept {
			T* tmp = this->pointer_;
			this->pointer_ = other.pointer_;
			other.pointer_ = tmp;
		}

		void swap(RefCountPtr& other) noexcept {
			T* tmp = this->pointer_;
			this->pointer_ = other.pointer_;
			other.pointer_ = tmp;
		}

		[[nodiscard]] T* operator->() const noexcept {
			assert(this->pointer_ != nullptr && "Attempting to dereference a null pointer in RefCountPtr");
			return this->pointer_;
		}

		[[nodiscard]] T& operator*() const noexcept {
			assert(this->pointer_ != nullptr && "Attempting to dereference a null pointer in RefCountPtr");
			return *this->pointer_;
		}

		[[nodiscard]] operator T* () const noexcept {
			return this->pointer_;
		}

		[[nodiscard]] T** operator&() noexcept {
			return this->releaseAndGetAddressOf();
		}

		[[nodiscard]] T* get() const noexcept {
			return this->pointer_;
		}

		[[nodiscard]] explicit operator bool() const noexcept {
			return this->pointer_ != nullptr;
		}

		[[nodiscard]] T* const* gtAddressOf() const noexcept  {
			return &this->pointer_;
		}

		[[nodiscard]] T** getAddressOf() noexcept {
			return &this->pointer_;
		}

		[[nodiscard]] T** releaseAndGetAddressOf() noexcept {
			this->internalRelease();
			return &this->pointer_;
		}

		T* detach() noexcept {
			T* ptr = this->pointer_;
			this->pointer_ = nullptr;

			return ptr;
		}

		void attach(T* other) noexcept {
			if (this->pointer_ != nullptr) {
				auto referenceCount = this->pointer_->Release();
				(void)referenceCount;
				
				assert(referenceCount != 0 || this->pointer_ != other && "Cannot attach to the same object without first detaching or releasing the existing reference.");
			}

			this->pointer_ = other;
		}

		static RefCountPtr<T> create(T* other)  {
			RefCountPtr<T> Ptr;
			Ptr.attach(other);

			return Ptr;
		}

		uint32_t reset() {
			return this->internalRelease();
		}
	};

	template<class T>
	RefCountPtr<T> makeRefCountPtr(T* other) {
		return RefCountPtr<T>::create(other);
	}

	template<class T>
	class RefCounter : public T {
		std::atomic<uint32_t> refCount_ = { 1 };
	public:
		virtual uint32_t AddRef() noexcept override {
			return this->refCount_.fetch_add(1) + 1;
		}

		virtual uint32_t Release() override {
			uint32_t newRefCount = this->refCount_.fetch_sub(1) - 1;
			if (newRefCount == 0)
				delete this;

			return newRefCount;
		}

		[[nodiscard]] virtual uint32_t GetRefCount() const noexcept override {
			return this->refCount_.load();
		}
	};
}