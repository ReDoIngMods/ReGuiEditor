# `rend/` - D3D11 rendering layer

Path: `ReGuiEditor/rend/` | Namespace: `rend` | Include root: `<rend/...>`

Thin RAII/ref-counted wrapper over Direct3D 11.1. Everything the editor draws (ImGui, MyGui render targets, blur, fullscreen effects) goes through it.

## Layout

| Path | Purpose |
|---|---|
| `Renderer.*` | Entry point singleton. Owns `ResourceManager`, `D3D11Device`, `ShaderManager` |
| `D3D11Device.*` | Creates device + immediate context (`ID3D11Device1` / `ID3D11DeviceContext1`) |
| `ResourceManager.*` | Factory + registry for every `rend::Resource` |
| `ShaderManager.*` | HLSL compile + on-disk `.cso` cache |
| `FullscreenPass.*` | Run a pixel shader over a viewport (post-fx, blits, procedural bg) |
| `Unknown.hpp` | COM-like base (`AddRef/Release/GetRefCount`) |
| `Common/` | Enums/structs mirroring D3D11/DXGI (no D3D headers needed by users) |
| `Resources/` | One class per GPU object (see below) |
| `Resources/Buffers/` | `BufferResource`, `Vertex/Index/ConstantBuffer` |
| `Resources/Shaders/` | `ShaderResource`, `Vertex/PixelShader`, `ShaderBlob`, `InputLayout` |
| `ImGui/` | `RGE_ImGui_*` backends (DX11, Win32, FreeType) |

## Lifecycle

```
Renderer::init(HWND)                      // note: HWND param currently unused
 ├─ ResourceManager::createInstance()
 └─ D3D11Device::createInstance()
     ├─ setRenderCallback  -> Application::getInstance().render()
     ├─ setInitCallback    -> Renderer::initRenderObjects()  (creates ShaderManager)
     ├─ setShutdownCallback-> Renderer::shutdownRenderObjects()
     └─ init()  -> createDevice() -> initCallback
```

- Feature levels: 11_1, 11_0, 10_1, 10_0 (retries without 11_1 on `E_INVALIDARG`).
- Flags: `BGRA_SUPPORT`; `DEBUG` layer + `ReportLiveDeviceObjects` on shutdown in debug builds.
- Teardown (`~Renderer`): `D3D11Device` first (runs shutdown callback), then `ResourceManager`.
- Singletons via `UTILS::Singleton<T>` (`createInstance` / `getInstance` / `destroyInstance`).

## Ref counting (read this)

- `rend::Unknown` -> `rend::Resource` (adds type/debug name, auto-registers in `ResourceManager`).
- `rend::RefCounter<T>` implements the count; **starts at 1**.
- `rend::RefCountPtr<T>` = ComPtr-like smart pointer. Works for both `rend::` objects and D3D COM objects.
- Every class has a `XxxHandle` typedef (`Texture2DHandle`, `PixelShaderHandle`, ...).
- **Gotcha:** `RefCountPtr(U*)` calls `AddRef`. Wrapping a fresh `new` leaks one ref. Use `makeRefCountPtr(ptr)` / `RefCountPtr<T>::create(ptr)` (attaches without AddRef). The `ResourceManager::create*` functions already return handles, so just use them.
- `operator&` releases then returns the address (ComPtr semantics). `getAddressOf()` does not release.
- Typo: `RefCountPtr::gtAddressOf()` (const overload) - missing `e`.
- `ResourceManager::getResources()` AddRefs every returned resource; caller must release.

## ResourceManager factories

| Call | Returns |
|---|---|
| `createSwapChain(void* hwnd)` | `SwapChainHandle` |
| `createTexture2D(w, h, Format, Texture2DFlags)` | `Texture2DHandle` |
| `createTexture2DNative(ID3D11Texture2D*)` | wrap an existing native texture |
| `createRenderView(Texture2D*)` | `RenderViewHandle` (RTV + SRV) |
| `createVertexShader / createPixelShader(path, entry, macros)` | shader handles (`createShader<T>` template picks one) |
| `createInputLayout(InputLayoutDesc, VertexShader*)` | `InputLayoutHandle` |
| `createVertexBuffer(stride, count, data)` | `VertexBufferHandle` |
| `createIndexBuffer(IndexBufferFormat, count, data)` | `IndexBufferHandle` |
| `createConstantBuffer(size, data)` | `ConstantBufferHandle` |
| `createRasterizerState(desc)` / `...DefaultRasterizerState()` | |
| `createBlendState(desc)` / `...DefaultBlendState()` | |
| `createDepthStencilState(desc)` / `...DefaultDepthStencilState()` | |
| `createSamplerState(desc)` / `createLinear/Point/AnisotropicSamplerState(addr[, maxAniso=16])` | |
| `createPipelineContext(name)` | `PipelineContextHandle` |

`getResourceCount()` is atomic (separate from the mutex to avoid deadlock).

## Resources

| Class | Notes |
|---|---|
| `Texture2D` | `init(w,h,Format,flags)` / `initFromNative`. Flags: `ShaderResource`, `RenderTarget`, `StagingRead`, `DynamicWrite`. `readData/writeData` (+`Region` variants), `copyResource`. Is a `ShaderResourceView`. |
| `RenderView` | RTV over a `Texture2D`; also an SRV (inherits `ShaderResourceView`). |
| `ShaderResourceView` | Base. `getShaderResourceView()` logs an error and returns null if unsupported. |
| `SwapChain` | `init(hwnd)`, `resize`, `present(vsync)`, `isOccluded`, `clearRenderTarget(Color)`. Tracks tearing support. |
| `BufferResource` | `mapBuffer(void**, BufferMapType)`, `unmapBuffer`, `readRawBuffer/writeRawBuffer`. Map types: `Read, Write, ReadWrite, WriteDiscard, WriteNoOverwrite`. |
| `VertexBuffer` | `init(stride, count, data, allowRead=false, allowWrite=true)`, `resizeBuffer`. |
| `IndexBuffer` | `UInt16` / `UInt32`; `resizeBuffer`, `changeFormat`. |
| `ConstantBuffer` | `init(size, data)`. Size must be a multiple of 16. |
| `BlendState` | `BlendDesc` (8 RT slots, `independentBlend`, `alphaToCoverage`), `ColorWriteMask`; holds blend factor + sample mask. |
| `DepthStencilState` | `DepthStencilDesc` (depth on by default, `Less`), front/back stencil ops; holds stencil ref. |
| `RasterizerState` | `RasterizerDesc` (cull Back, depthClip on) + `RasterizerDescEx` (frontCCW, scissor, depth bias). |
| `SamplerState` | `SamplerDesc` (Linear, Wrap, LOD +-FLT_MAX). Filters: `Point`, `Linear`, `Anisotropic` only. |
| `PipelineContext` | See below. |
| `InputLayout` | Build with `InputLayoutDesc::addElement(semantic, index, Format, slot, offset=REND_APPEND_ALIGNED_ELEMENT, class, stepRate)`. |
| `VertexShader` / `PixelShader` | `init(path, entry, macros)`; keep `ShaderBlob` (bytecode + hash). |

## PipelineContext

Isolated pipeline state (wraps `ID3DDeviceContextState`; `begin()` activates, `end()` restores the previous one). Lets a pass run safely inside another context, e.g. from an ImGui draw callback.

- State setters: `setRasterizerState`, `setBlendState`, `setDepthStencilState`, `setInputLayout`, `setVertexShader`, `setPixelShader`, `setIndexBuffer`, `setPrimitiveTopology`.
- Bindings (templated on `ShaderStage::Vertex|Pixel`): `setConstantBuffers`, `setShaderResourceViews`, `setSamplerStates`, each with variadic / `std::vector` overloads and `clear*`.
- Non-stage: `setVertexBuffers`, `setRenderViews`, `setScissorRects`, `setViewports` (+ `clear*`).
- Draws: `draw`, `drawIndexed`, `drawInstanced`, `drawIndexedInstanced`, `drawAuto`, `draw(Indexed)InstancedIndirect`.

```cpp
ctx->begin();
ctx->setRenderViews(target);
ctx->setViewports(vp);
ctx->setVertexShader(vs); ctx->setPixelShader(ps);
ctx->setShaderResourceViews<rend::ShaderStage::Pixel>(srv);
ctx->draw(3, 0);
ctx->end();
```

## ShaderManager

- `loadBlob(path, entry, macros, ShaderType, ShaderBlob** out)`.
- Cache key = hash(path, entry, macros, type). Cache file: `$RGE_CACHE_DATA/Shaders/shader_<vs|ps>_<name>_<hash16>.cso`; `#include`s are recorded so the cache can be invalidated.
- Macros are defined as `"1"`; a stage define (`shaderDefine`) is always added.
- Shader paths use env-style prefixes, e.g. `$RGE_DATA/Shaders/...`.

## FullscreenPass

One oversized triangle from `SV_VertexID` (default VS `$RGE_DATA/Shaders/gui_fullscreen_triangle.hlsl`, `vsMain`).

```cpp
rend::FullscreenPass pass;
rend::FullscreenPassDesc d;
d.pixelShader = "$RGE_DATA/Shaders/my_effect.hlsl";
d.constantBufferSize = sizeof(MyConsts);   // alignas(16), bound at PS b0
pass.init(d);

rend::FullscreenDrawParams p;
p.target = renderView;
p.viewport = rend::Viewport(0, 0, w, h, 0, 1);
p.constants = &consts;
p.shaderResourceViews = { srv };           // PS t0..
p.samplerStates = { sampler };             // PS s0..
p.extraConstantBuffers = { cb };           // PS b1.. (b0 is the pass's own)
p.blendState = nullptr;                    // nullptr = opaque overwrite
pass.draw(p);
```

- `SV_Position` is in *target* pixels, not viewport-relative.
- Non-copyable; `cleanup()` runs in the destructor.

## Common/

| File | Content |
|---|---|
| `Color.hpp` | `union Color {r,g,b,a / components[4]}`, ops, `toRGBA/fromRGBA` (ABGR packed), `lerp`, named colors |
| `Rectangle.hpp` | `{left,top,right,bottom}`, `contains`, `intersects`, `width/height/area` |
| `Viewport.hpp` | `{x,y,width,height,minDepth,maxDepth}` |
| `Format.hpp` | `Format` = DXGI_FORMAT mirror, `getFormatSize`, `getFormatName` |
| `Alignment.hpp` | `alignTo(value, alignment)` (power-of-2 alignment), `alignPower2(value)` |
| `BlendMode/BlendOperand/ComparisonFunction/CullingMode/PrimitiveTopology/SamplerAddressMode/SamplerFilter/StencilOperand` | D3D11 enum mirrors, each with `xxxToString()`; values are cast directly to D3D11 enums |

## ImGui backends (`rend/ImGui/`)

- `RGE_ImGui_ImplDX11`: `Init`, `Shutdown`, `RenderDrawData(drawData, RenderView*)`, device-object create/invalidate, `UpdateTexture(ImTextureData*)` (new ImGui texture API), `GetFrameInfo()`, `GetCallbackViewport(drawCmd, outViewport)` for draw callbacks.
- `RGE_ImGui_ImplWin32`: `Init(hwnd)`, `Shutdown`, `NewFrame`, `WndProcHandler`.
- `RGE_ImGui_FreeType`: cleaned-up copy of the ImGui FreeType loader; `RGE_ImGui_FreeType_GetFontLoader()`, flags `RGE_ImGui_FreeType_LoaderFlags_*`.

## Known issues / watch-outs

- `Format.hpp` `getFormatName`: the `B8G8R8X8_UNORM_SRGB` string contains full-width digits (`８`) - copy/paste bug.
- `Texture2D::getRowPitch()` = `width * bytesPerPixel`; wrong for block-compressed (BC*) formats.
- `getFormatSize` returns bytes/pixel, but for BC formats it is bytes/**block** (8 or 16); `R1_UNORM` returns 1.
- `ShaderResourceView.hpp` uses `CONSOLE_ERROR` without including `Console.hpp` (relies on PCH / force-include).
- `Renderer::init(HWND window)` ignores `window`.
- `DepthStencilDesc::depthWriteMask` is a `bool` (not a mask enum).
- `ConstantBuffer` sizes must be 16-byte aligned (use `rend::alignTo(size, 16u)`).
