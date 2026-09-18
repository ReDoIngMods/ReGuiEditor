#pragma once
#include <UTILS/Singleton.hpp>

#include <MyGui/MyGUI_Types.h>

#include <d3d11.h>

class BlurrySubSkinRenderer : public UTILS::Singleton<BlurrySubSkinRenderer> {
	struct alignas(16) ConstantBufferData {
		float backdropUV[4];
		float atlasUV[4];
	};

	ID3D11VertexShader* vertexShader_ = nullptr;
	ID3D11PixelShader* pixelShader_ = nullptr;
	ID3D11SamplerState* samplerState_ = nullptr;
	ID3D11Buffer* paramsBuffer_ = nullptr;

	bool initialized_ = false;
public:
	BlurrySubSkinRenderer() = default;
	~BlurrySubSkinRenderer() { this->cleanup(); }

	bool initialize();
	void cleanup();

	void composite(ID3D11ShaderResourceView* backdropView, const MyGUI::FloatRect& backdropUV, ID3D11ShaderResourceView* atlasView, const MyGUI::FloatRect& atlasUV, ID3D11RenderTargetView* destinationView, int destinationWidth, int destinationHeight);
};