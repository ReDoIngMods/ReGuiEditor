#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ReGuiEditor {
    struct ExpectedError {
        std::string message_ = "Unknown error";
    public:
        ExpectedError() = default;

        ExpectedError(std::string message) : message_(std::move(message)) {}

		std::string_view message() const { return this->message_; }
    };

    template<typename T>
    class Expected {
        std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
        ReGuiEditor::ExpectedError errorMessage_;
    public:
        Expected() = default;

        Expected(T value) {
            new (&this->storage_) T(std::move(value));
            this->errorMessage_.message_.clear();
        }

		Expected(ReGuiEditor::ExpectedError error) : errorMessage_(std::move(error)) {}

        Expected(const ReGuiEditor::Expected<T>& other) {
            if (other.hasValue())
                new (&this->storage_) T(other.value());
            else
                this->errorMessage_ = other.errorMessage_;
        }

        Expected(ReGuiEditor::Expected<T>&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
            if (other.hasValue())
                new (&this->storage_) T(std::move(other.value()));
            else
                this->errorMessage_ = std::move(other.errorMessage_);
        }

        ReGuiEditor::Expected<T>& operator=(const ReGuiEditor::Expected<T>& other) {
            if (this == &other)
                return *this;

            if (this->hasValue())
                reinterpret_cast<T*>(&this->storage_)->~T();

            if (other.hasValue()) {
                new (&this->storage_) T(other.value());
                this->errorMessage_.clear();
            } else {
                this->errorMessage_ = other.errorMessage_;
            }

            return *this;
        }

        ReGuiEditor::Expected<T>& operator=(ReGuiEditor::Expected<T>&& other) noexcept(std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_move_assignable_v<T>) {
			if (this == &other)
				return *this;

			if (this->hasValue())
				reinterpret_cast<T*>(&this->storage_)->~T();

			if (other.hasValue()) {
				new (&this->storage_) T(std::move(other.value()));
				this->errorMessage_.clear();
            } else {
                this->errorMessage_ = std::move(other.errorMessage_);
            }

            return *this;
        }

        ~Expected() {
            if (this->hasValue())
                reinterpret_cast<T*>(&this->storage_)->~T();
        }

        const bool hasValue() const { return this->errorMessage_.message_.empty(); }
        std::string_view getErrorMessage() const { return this->errorMessage_.message_; }
		const ReGuiEditor::ExpectedError& getError() const { return this->errorMessage_; }

        T& value() {
            if (!this->hasValue())
                throw std::runtime_error("Expected<T> has no value: " + this->errorMessage_.message_);

            return *reinterpret_cast<T*>(&this->storage_);
        }

        const T& value() const {
            if (!this->hasValue())
                throw std::runtime_error("Expected<T> has no value: " + this->errorMessage_.message_);

            return *reinterpret_cast<const T*>(&this->storage_);
        }

        explicit operator bool() const { return this->hasValue(); }
        explicit operator T& () { return this->value(); }
        explicit operator const T& () const { return this->value(); }

        T* operator->() { return &this->value(); }
        T& operator*() { return this->value(); }

        const T* operator->() const { return &this->value(); }
        const T& operator*() const { return this->value(); }
    };
}