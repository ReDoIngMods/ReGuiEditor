#pragma once
#include <UTILS/Singleton.hpp>

#include <rend/Common/RefCounter.hpp>

#include <d3d11_1.h>

#include <functional>
#include <utility>

namespace rend {
	class D3D11Device : public UTILS::Singleton<D3D11Device> {
		rend::RefCountPtr<ID3D11Device1> device1_;
		rend::RefCountPtr<ID3D11DeviceContext1> deviceContext1_;

		bool isInitialized_ = false;

		std::function<void()> renderCallback_;
		std::function<bool()> initCallback_;
		std::function<void()> shutdownCallback_;

		bool createDevice();
	public:
		~D3D11Device() override;

		bool init();

		rend::RefCountPtr<ID3D11Device1> getDevice1() const { return this->device1_; }
		rend::RefCountPtr<ID3D11DeviceContext1> getDeviceContext1() const { return this->deviceContext1_; }

		void setRenderCallback(std::function<void()> callback) { this->renderCallback_ = std::move(callback); }
		void setInitCallback(std::function<bool()> callback) { this->initCallback_ = std::move(callback); }
		void setShutdownCallback(std::function<void()> callback) { this->shutdownCallback_ = std::move(callback); }
	};
}