#include "RGE_ImGui_Impl_DX11.hpp" // VCIC-Excluded

#include <ImGui/imgui.h>

#include <rend/Common/Color.hpp>
#include <rend/Common/CullingMode.hpp>
#include <rend/Common/Format.hpp>
#include <rend/Common/PrimitiveTopology.hpp>
#include <rend/Common/Rectangle.hpp>
#include <rend/Common/RefCounter.hpp>
#include <rend/Common/SamplerAddressMode.hpp>
#include <rend/Common/Viewport.hpp>

#include <rend/ResourceManager.hpp>

#include <rend/Resources/SwapChain.hpp>

#include <rend/Resources/Buffers/BufferResource.hpp>
#include <rend/Resources/Buffers/ConstantBuffer.hpp>
#include <rend/Resources/Buffers/IndexBuffer.hpp>
#include <rend/Resources/Buffers/VertexBuffer.hpp>

#include <rend/Resources/Resource.hpp>

#include <rend/Resources/PipelineContext.hpp>

#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/ShaderResourceView.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <rend/Resources/Shaders/InputLayout.hpp>
#include <rend/Resources/Shaders/PixelShader.hpp>
#include <rend/Resources/Shaders/VertexShader.hpp>

#include <rend/Resources/BlendState.hpp>
#include <rend/Resources/DepthStencilState.hpp>
#include <rend/Resources/RasterizerState.hpp>
#include <rend/Resources/SamplerState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

struct alignas(16) RGE_ImGui_ImplDX11_ConstantBuffer {
    float position[2];
    float resolution[2];
};

struct RGE_ImGui_ImplDX11_Data {
    rend::PipelineContextHandle pipelineContext;

    rend::VertexShaderHandle vertexShader;
    rend::PixelShaderHandle pixelShader;

    rend::InputLayoutHandle inputLayout;

    rend::ConstantBufferHandle constantBuffer;
    rend::VertexBufferHandle vertexBuffer;
    rend::IndexBufferHandle indexBuffer;

    rend::RasterizerStateHandle rasterizerState;
    rend::BlendStateHandle blendState;
    rend::SamplerStateHandle samplerState;
    rend::DepthStencilStateHandle depthStencilState;

    RGE_ImGui_ImplDX11_FrameInfo frameInfo;
};

struct RGE_ImGui_ImplDX11_ViewportData {
	rend::SwapChainHandle swapChain;
};

inline static RGE_ImGui_ImplDX11_Data* getBackendDX11Data() {
    return ImGui::GetCurrentContext() ? reinterpret_cast<RGE_ImGui_ImplDX11_Data*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
}

inline static void DestroyTexture(ImTextureData* textureData) {
    if (!textureData)
        return;

    rend::Texture2D* texture = static_cast<rend::Texture2D*>(textureData->BackendUserData);
    if (!texture) {
        CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ImGui texture with ID %p has no associated rend::Texture2D resource. This may indicate a resource leak.", textureData->TexID);
        return;
    }

    textureData->BackendUserData = nullptr;
    textureData->SetTexID(ImTextureID_Invalid);
    textureData->SetStatus(ImTextureStatus_Destroyed);

    texture->Release();
}

static void RGE_ImGui_ImplDX11_InitPlatformInterface();
static void RGE_ImGui_ImplDX11_ShutdownPlatformInterface();

bool RGE_ImGui_ImplDX11_Init() {
    CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing ImGui DX11 backend...");

    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData != nullptr || io.BackendRendererName != nullptr) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "A existing renderer backend is already initialized! Please shutdown the current one first!");
        return false;
    }

    RGE_ImGui_ImplDX11_Data* backendData = IM_NEW(RGE_ImGui_ImplDX11_Data)();
    io.BackendRendererName = "RGE_ImGui_ImplDX11";
    io.BackendRendererUserData = backendData;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    backendData->pipelineContext = rend::ResourceManager::getInstance().createPipelineContext("RGE_ImGui_ImplDX11_PipelineContext");
    if (!backendData->pipelineContext) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create rend::PipelineContext for ImGui DX11 backend!");

        RGE_ImGui_ImplDX11_Shutdown();
        return false;
    }

    if (!RGE_ImGui_ImplDX11_CreateDeviceObjects()) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create device objects for ImGui DX11 backend!");

        RGE_ImGui_ImplDX11_Shutdown();
        return false;
    }

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        RGE_ImGui_ImplDX11_InitPlatformInterface();

    return true;
}

void RGE_ImGui_ImplDX11_Shutdown() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData == nullptr && io.BackendRendererName == nullptr)
        return;

    CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Shutting down ImGui DX11 backend...");

    RGE_ImGui_ImplDX11_ShutdownPlatformInterface();

    ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

    for (ImTextureData* textureData : platformIO.Textures) {
        if (textureData->RefCount != 1) {
            CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "ImGui texture with ID %p has a reference count of %d, expected 1. This may indicate a resource leak.", textureData->TexID, textureData->RefCount);
            continue;
        }

        DestroyTexture(textureData);
    }

    RGE_ImGui_ImplDX11_InvalidateDeviceObjects();

    IM_DELETE(getBackendDX11Data());

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasTextures;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasViewports;
}

void RGE_ImGui_ImplDX11_RenderDrawData(ImDrawData* drawData, rend::RenderView* renderView) {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplDX11_Init()?");
    IM_ASSERT(io.BackendRendererName != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplDX11_Init()?");

    if (!drawData || drawData->CmdLists.Size == 0)
        return;

    RGE_ImGui_ImplDX11_Data* backendData = getBackendDX11Data();
    if (!backendData)
        return;

    if (drawData->Textures != nullptr) {
        for (ImTextureData* textureData : *drawData->Textures) {
            if (textureData->Status == ImTextureStatus_OK)
                continue;

            RGE_ImGui_ImplDX11_UpdateTexture(textureData);
        }
    }

    RGE_ImGui_ImplDX11_ConstantBuffer constantBufferData;
    memset(&constantBufferData, 0, sizeof(constantBufferData));
    constantBufferData.position[0] = drawData->DisplayPos.x;
    constantBufferData.position[1] = drawData->DisplayPos.y;
    constantBufferData.resolution[0] = drawData->DisplaySize.x;
    constantBufferData.resolution[1] = drawData->DisplaySize.y;
    if (!backendData->constantBuffer->writeRawBuffer(&constantBufferData, sizeof(constantBufferData))) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to update ImGui constant buffer!");
        return;
    }

    if (!backendData->indexBuffer->resizeBuffer(drawData->TotalIdxCount)) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resize ImGui vertex or index buffer!");
        return;
    }

    if (!backendData->vertexBuffer->resizeBuffer(drawData->TotalVtxCount)) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resize ImGui vertex or index buffer!");
        return;
    }

    ImDrawVert* vertexDestination = nullptr;
    if (!backendData->vertexBuffer->mapBuffer<ImDrawVert>(&vertexDestination, rend::BufferMapType::WriteDiscard)) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map ImGui vertex buffer!");
        return;
    }

    ImDrawIdx* indexDestination = nullptr;
    if (!backendData->indexBuffer->mapBuffer<ImDrawIdx>(&indexDestination, rend::BufferMapType::WriteDiscard)) {
        (void)backendData->vertexBuffer->unmapBuffer();

        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map ImGui index buffer!");
        return;
    }

    for (int cmdListIndex = 0; cmdListIndex < drawData->CmdLists.Size; cmdListIndex++) {
        const ImDrawList* cmdList = drawData->CmdLists[cmdListIndex];
        memcpy(vertexDestination, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(indexDestination, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        vertexDestination += cmdList->VtxBuffer.Size;
        indexDestination += cmdList->IdxBuffer.Size;
    }

    (void)backendData->vertexBuffer->unmapBuffer();
    (void)backendData->indexBuffer->unmapBuffer();

    rend::PipelineContext* pipelineContext = backendData->pipelineContext;
    pipelineContext->begin();

    pipelineContext->setRenderViews(renderView);

    pipelineContext->setPrimitiveTopology(rend::PrimitiveTopology::TriangleList);

    pipelineContext->setRasterizerState(backendData->rasterizerState);
    pipelineContext->setBlendState(backendData->blendState);
    pipelineContext->setDepthStencilState(backendData->depthStencilState);
    pipelineContext->setInputLayout(backendData->inputLayout);

    pipelineContext->setVertexShader(backendData->vertexShader);
    pipelineContext->setPixelShader(backendData->pixelShader);

    pipelineContext->setConstantBuffers<rend::ShaderStage::Vertex>(0, backendData->constantBuffer);
    pipelineContext->setSamplerStates<rend::ShaderStage::Pixel>(0, backendData->samplerState);

    pipelineContext->setVertexBuffers(backendData->vertexBuffer);
    pipelineContext->setIndexBuffer(backendData->indexBuffer);

    rend::Viewport viewport;
    viewport.width = drawData->DisplaySize.x * drawData->FramebufferScale.x;
    viewport.height = drawData->DisplaySize.y * drawData->FramebufferScale.y;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    pipelineContext->setViewports(viewport);

    backendData->frameInfo.renderView = renderView;
    backendData->frameInfo.displayPos = drawData->DisplayPos;
    backendData->frameInfo.framebufferScale = drawData->FramebufferScale;

    uint32_t globalIndexOffset = 0;
    uint32_t globalVertexOffset = 0;
    for (const ImDrawList* drawList : drawData->CmdLists) {
        for (const ImDrawCmd& drawCmd : drawList->CmdBuffer) {
            if (drawCmd.UserCallback) {
                drawCmd.UserCallback(drawList, &drawCmd);
                continue;
            }

            ImVec2 clipMin = {
                (drawCmd.ClipRect.x - drawData->DisplayPos.x) * drawData->FramebufferScale.x,
                (drawCmd.ClipRect.y - drawData->DisplayPos.y) * drawData->FramebufferScale.y
            };

            ImVec2 clipMax = {
                (drawCmd.ClipRect.z - drawData->DisplayPos.x) * drawData->FramebufferScale.x,
                (drawCmd.ClipRect.w - drawData->DisplayPos.y) * drawData->FramebufferScale.y
            };

            if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                continue;

            rend::Rectangle scissorRect;
            scissorRect.left = clipMin.x;
            scissorRect.top = clipMin.y;
            scissorRect.right = clipMax.x;
            scissorRect.bottom = clipMax.y;

            pipelineContext->setScissorRects(scissorRect);
            pipelineContext->setShaderResourceViews<rend::ShaderStage::Pixel>(0, reinterpret_cast<const rend::ShaderResourceView*>(drawCmd.GetTexID()));
            pipelineContext->drawIndexed(drawCmd.ElemCount, drawCmd.IdxOffset + globalIndexOffset, drawCmd.VtxOffset + globalVertexOffset);
        }

        globalIndexOffset += drawList->IdxBuffer.Size;
        globalVertexOffset += drawList->VtxBuffer.Size;
    }

    backendData->frameInfo = RGE_ImGui_ImplDX11_FrameInfo();

    pipelineContext->clearRenderViews();

    pipelineContext->end();
}

bool RGE_ImGui_ImplDX11_CreateDeviceObjects() {
    RGE_ImGui_ImplDX11_Data* backendData = getBackendDX11Data();
    if (!backendData)
        return false;

    rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
    backendData->vertexShader = resourceManager.createVertexShader("$RGE_DATA/shaders/gui_imgui.hlsl", "vsMain");
    if (!backendData->vertexShader) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui vertex shader!");
        return false;
    }
    backendData->vertexShader->setDebugName("RGE_ImGui_ImplDX11_VertexShader");

    backendData->pixelShader = resourceManager.createPixelShader("$RGE_DATA/shaders/gui_imgui.hlsl", "psMain");
    if (!backendData->pixelShader) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui pixel shader!");
        return false;
    }
    backendData->pixelShader->setDebugName("RGE_ImGui_ImplDX11_PixelShader");

    rend::InputLayoutDesc inputLayoutDesc;
    inputLayoutDesc.addElement("POSITION", 0, rend::Format::R32G32_FLOAT);
    inputLayoutDesc.addElement("TEXCOORD", 0, rend::Format::R32G32_FLOAT);
    inputLayoutDesc.addElement("COLOR", 0, rend::Format::R8G8B8A8_UNORM);

    backendData->inputLayout = resourceManager.createInputLayout(inputLayoutDesc, backendData->vertexShader);
    if (!backendData->inputLayout) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui input layout!");
        return false;
    }
    backendData->inputLayout->setDebugName("RGE_ImGui_ImplDX11_InputLayout");

    backendData->constantBuffer = resourceManager.createConstantBuffer(sizeof(RGE_ImGui_ImplDX11_ConstantBuffer), nullptr);
    if (!backendData->constantBuffer) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui constant buffer!");
        return false;
    }
    backendData->constantBuffer->setDebugName("RGE_ImGui_ImplDX11_ConstantBuffer");

    backendData->vertexBuffer = resourceManager.createVertexBuffer(sizeof(ImDrawVert));
    if (!backendData->vertexBuffer) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui vertex buffer!");
        return false;
    }
    backendData->vertexBuffer->setDebugName("RGE_ImGui_ImplDX11_VertexBuffer");

    backendData->indexBuffer = resourceManager.createIndexBuffer(sizeof(ImDrawIdx) == 2 ? rend::IndexBufferFormat::UInt16 : rend::IndexBufferFormat::UInt32);
    if (!backendData->indexBuffer) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui index buffer!");
        return false;
    }
    backendData->indexBuffer->setDebugName("RGE_ImGui_ImplDX11_IndexBuffer");

    rend::RasterizerDesc rasterizerDesc;
    rasterizerDesc.cullMode = rend::CullingMode::None;
    rasterizerDesc.depthClip = true;
    rasterizerDesc.advanced.scissor = true;

    backendData->rasterizerState = resourceManager.createRasterizerState(rasterizerDesc);
    if (!backendData->rasterizerState) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui rasterizer state!");
        return false;
    }
    backendData->rasterizerState->setDebugName("RGE_ImGui_ImplDX11_RasterizerState");

    backendData->blendState = resourceManager.createDefaultBlendState();
    if (!backendData->blendState) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui blend state!");
        return false;
    }
    backendData->blendState->setDebugName("RGE_ImGui_ImplDX11_BlendState");

    backendData->samplerState = resourceManager.createLinearSamplerState(rend::SamplerAddressMode::Clamp);
    if (!backendData->samplerState) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui sampler state!");
        return false;
    }
    backendData->samplerState->setDebugName("RGE_ImGui_ImplDX11_SamplerState");

    backendData->depthStencilState = resourceManager.createDefaultDepthStencilState();
    if (!backendData->depthStencilState) {
        CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui depth stencil state!");
        return false;
    }
    backendData->depthStencilState->setDebugName("RGE_ImGui_ImplDX11_DepthStencilState");

    return true;
}

void RGE_ImGui_ImplDX11_InvalidateDeviceObjects() {
    RGE_ImGui_ImplDX11_Data* backendData = getBackendDX11Data();
    if (!backendData)
        return;

    backendData->depthStencilState.reset();
    backendData->samplerState.reset();
    backendData->blendState.reset();
    backendData->rasterizerState.reset();

    backendData->indexBuffer.reset();
    backendData->vertexBuffer.reset();
    backendData->constantBuffer.reset();

    backendData->inputLayout.reset();
    backendData->pixelShader.reset();
    backendData->vertexShader.reset();
}

void RGE_ImGui_ImplDX11_UpdateTexture(ImTextureData* textureData) {
    if (!textureData || textureData->Status == ImTextureStatus_OK)
        return;

    if (textureData->Status == ImTextureStatus_WantCreate) {
        IM_ASSERT(textureData->TexID == ImTextureID_Invalid && "Texture ID should be invalid when creating a new texture!");
        IM_ASSERT(textureData->Format == ImTextureFormat_RGBA32 && "Only RGBA32 format is supported for texture creation in this backend!");

        uint8_t* pixels = static_cast<uint8_t*>(textureData->GetPixels());

        rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
        rend::Texture2DHandle texture = resourceManager.createTexture2D(textureData->Width, textureData->Height, rend::Format::R8G8B8A8_UNORM, rend::Texture2DFlags::ShaderResource);
        if (!texture) {
            CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui texture!");

            textureData->SetTexID(ImTextureID_Invalid);
            textureData->SetStatus(ImTextureStatus_Destroyed);
            return;
        }

        if (!texture->writeData(pixels, textureData->Width * textureData->Height * textureData->BytesPerPixel)) {
            CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to write data to ImGui texture!");

            textureData->SetTexID(ImTextureID_Invalid);
            textureData->SetStatus(ImTextureStatus_Destroyed);
            return;
        }

        texture->setDebugName("RGE_ImGui_ImplDX11_Texture");

        rend::Texture2D* ownedTexture = texture.detach();
        textureData->BackendUserData = ownedTexture;
        textureData->SetTexID(reinterpret_cast<ImTextureID>(static_cast<rend::ShaderResourceView*>(ownedTexture)));
        textureData->SetStatus(ImTextureStatus_OK);
        return;
    }
    else if (textureData->Status == ImTextureStatus_WantUpdates) {
        rend::Texture2D* texture = static_cast<rend::Texture2D*>(textureData->BackendUserData);
        if (!texture) {
            CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "ImGui texture update requested but no backend texture found!");

            textureData->SetTexID(ImTextureID_Invalid);
            textureData->SetStatus(ImTextureStatus_Destroyed);
            return;
        }

        for (const ImTextureRect& updateRect : textureData->Updates) {
            uint8_t* pixels = static_cast<uint8_t*>(textureData->GetPixelsAt(updateRect.x, updateRect.y));
            size_t updateSize = static_cast<size_t>(updateRect.w) * updateRect.h * textureData->BytesPerPixel;
            size_t srcPitch = static_cast<size_t>(textureData->Width) * textureData->BytesPerPixel;

            if (!texture->writeDataRegion(pixels, updateSize, updateRect.x, updateRect.y, updateRect.w, updateRect.h, static_cast<uint32_t>(srcPitch)))
                CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to update ImGui texture region (%d, %d, %d, %d)!", updateRect.x, updateRect.y, updateRect.w, updateRect.h);
        }

        textureData->SetStatus(ImTextureStatus_OK);
        return;
    } else if (textureData->Status == ImTextureStatus_WantDestroy) {
        if (textureData->UnusedFrames > 0)
            DestroyTexture(textureData);

        return;
    }

    textureData->SetTexID(ImTextureID_Invalid);
    textureData->SetStatus(ImTextureStatus_OK);
}

const RGE_ImGui_ImplDX11_FrameInfo* RGE_ImGui_ImplDX11_GetFrameInfo() {
    RGE_ImGui_ImplDX11_Data* backendData = getBackendDX11Data();
    return backendData ? &backendData->frameInfo : nullptr;
}

bool RGE_ImGui_ImplDX11_GetCallbackViewport(const ImDrawCmd* drawCmd, rend::Viewport& outputViewport) {
    RGE_ImGui_ImplDX11_Data* backendData = getBackendDX11Data();
    if (!drawCmd || !backendData)
        return false;

    const float left = std::max(0.0f, std::trunc((drawCmd->ClipRect.x - backendData->frameInfo.displayPos.x) * backendData->frameInfo.framebufferScale.x));
    const float top = std::max(0.0f, std::trunc((drawCmd->ClipRect.y - backendData->frameInfo.displayPos.y) * backendData->frameInfo.framebufferScale.y));
    const float right = std::trunc((drawCmd->ClipRect.z - backendData->frameInfo.displayPos.x) * backendData->frameInfo.framebufferScale.x);
    const float bottom = std::trunc((drawCmd->ClipRect.w - backendData->frameInfo.displayPos.y) * backendData->frameInfo.framebufferScale.y);

    if (right <= left || bottom <= top)
        return false;

    outputViewport.x = left;
    outputViewport.y = top;
    outputViewport.width = right - left;
    outputViewport.height = bottom - top;
    outputViewport.minDepth = 0.0f;
    outputViewport.maxDepth = 1.0f;
    return true;
}

static void RGE_ImGui_ImplDX11_CreateWindow(ImGuiViewport* viewport) {
    RGE_ImGui_ImplDX11_ViewportData* viewportData = IM_NEW(RGE_ImGui_ImplDX11_ViewportData)();
    viewport->RendererUserData = viewportData;

    void* hwnd = viewport->PlatformHandleRaw ? viewport->PlatformHandleRaw : viewport->PlatformHandle;
	if (!hwnd) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create ImGui viewport window: Invalid HWND!");
		return;
	}

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
	viewportData->swapChain = resourceManager.createSwapChain(hwnd);
	if (!viewportData->swapChain) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create rend::SwapChain for ImGui viewport window!");
		return;
	}
}

static void RGE_ImGui_ImplDX11_DestroyWindow(ImGuiViewport* viewport) {
    if (RGE_ImGui_ImplDX11_ViewportData* viewportData = static_cast<RGE_ImGui_ImplDX11_ViewportData*>(viewport->RendererUserData)) {
        viewportData->swapChain.reset();

        IM_DELETE(viewportData);
    }
    viewport->RendererUserData = nullptr;
}

static void RGE_ImGui_ImplDX11_SetWindowSize(ImGuiViewport* viewport, ImVec2 size) {
	RGE_ImGui_ImplDX11_ViewportData* viewportData = static_cast<RGE_ImGui_ImplDX11_ViewportData*>(viewport->RendererUserData);
	if (!viewportData || !viewportData->swapChain)
		return;

	(void)viewportData->swapChain->resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
}

static void RGE_ImGui_ImplDX11_RenderWindow(ImGuiViewport* viewport, void* renderArg) {
    RGE_ImGui_ImplDX11_ViewportData* viewportData = static_cast<RGE_ImGui_ImplDX11_ViewportData*>(viewport->RendererUserData);
    if (!viewportData || !viewportData->swapChain)
        return;

    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
        (void)viewportData->swapChain->clearRenderTarget(rend::Color::black());

    RGE_ImGui_ImplDX11_RenderDrawData(viewport->DrawData, viewportData->swapChain->getRenderTargetView());
}

static void RGE_ImGui_ImplDX11_SwapBuffers(ImGuiViewport* viewport, void* renderArg) {
    RGE_ImGui_ImplDX11_ViewportData* viewportData = static_cast<RGE_ImGui_ImplDX11_ViewportData*>(viewport->RendererUserData);
	if (!viewportData || !viewportData->swapChain)
		return;

    (void)viewportData->swapChain->present();
}

static void RGE_ImGui_ImplDX11_InitPlatformInterface() {
    ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
    platformIO.Renderer_CreateWindow = RGE_ImGui_ImplDX11_CreateWindow;
    platformIO.Renderer_DestroyWindow = RGE_ImGui_ImplDX11_DestroyWindow;
    platformIO.Renderer_SetWindowSize = RGE_ImGui_ImplDX11_SetWindowSize;
    platformIO.Renderer_RenderWindow = RGE_ImGui_ImplDX11_RenderWindow;
    platformIO.Renderer_SwapBuffers = RGE_ImGui_ImplDX11_SwapBuffers;
}

static void RGE_ImGui_ImplDX11_ShutdownPlatformInterface() {
    ImGui::DestroyPlatformWindows();
}