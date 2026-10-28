# `MyGuiRenderer/` - MyGUI platform on top of `rend`

Path: `ReGuiEditor/MyGuiRenderer/` | No namespace (global classes) | Depends on: [rend.md](rend.md), `MyGui/` headers, `ConfigManager`

Implements everything MyGUI needs to run inside the editor (render manager, textures, vertex buffers, data/file access, fonts, logging, resolution handling) plus the game's "blurry" skin. The GUI is drawn into an offscreen `RenderView` that ImGui then shows (`UIRenderView`).

## Layout

| Path | Purpose |
|---|---|
| `GuiSystemManager.*` | Top-level singleton: init/cleanup/render of the whole MyGUI stack |
| `ResolutionManager.*` | Picks the game's per-resolution resource folder, loads/unloads its resources |
| `DataManager.*` / `DataStream.*` | MyGUI file access (search dirs, memory-mapped streams) |
| `FontManager.*` | Loads TrueType font XML, caches generated fonts |
| `LogListener.hpp` | Routes MyGUI log output to the editor console |
| `BackgroundImageHelper.*` | Draws a background image under the GUI (`FullscreenPass`) |
| `BlurrySubSkin/` | Backdrop-blur skin used by the game's glassy panels |
| `RenderPlatform/` | `RP*` classes = MyGUI `RenderManager`/`ITexture`/`IVertexBuffer`/`IRenderTarget` on `rend` |

## GuiSystemManager

`initialize()` order (every step returns `false` on failure):

1. `MyGUI::LogManager` + `LogSource` + `LogListener` (stdout disabled)
2. `DataManager`
3. `RPRenderManager::initialise()`
4. Render target: `R8G8B8A8_UNORM`, `ShaderResource | RenderTarget`, size from config `mygui.canvas.width/height` (default **1920x1080**)
5. `MyGUI::Gui::initialise("")`, register `ResourceTrueTypeFont` factory
6. Load resource XMLs: `$GAME_DATA/Gui/GuiConfig.xml`, `$SURVIVAL_DATA/Gui/GuiConfigSurvival.xml`, `$CHALLENGE_DATA/Gui/GuiConfigChallenge.xml`, `$GAME_DATA/Gui/GuiSettings.xml` (Customization config is commented out)
7. `FontManager::loadFontXML("$GAME_DATA/Gui/Language/English/Fonts.xml")` (English hard-coded)
8. `ResolutionManager::initialize()`
9. `GuiBlurManager::initialize(w, h)` -> `BlurrySubSkinRenderer::initialize()` -> `BlurrySubSkinRegistry` (registers the skin factory)
10. `ResolutionManager::setResolution(w, h)`
11. `BackgroundImageHelper` + default background `gui_loadingscreen_creative.png`

`render()` per frame:

```
Clear RT (Aquamarine)
 -> BackgroundImageHelper::render
 -> GuiBlurManager::captureAndBlur(renderView)        // blur of what is on the RT *so far*
 -> BlurrySubSkinRegistry::updateAllInstances(blurSRV) // each skin composites its panel
 -> RPRenderManager::drawOneFrame()                    // MyGUI draws all layers
```

- `changeResolution(w, h)`: no-op if unchanged; recreates RT, then `ResolutionManager::setResolution`, then `GuiBlurManager::resize`.
- Getters: `getRenderShaderResourceView()` (the GUI output), `getBackgroundImageHelper()`, `getGuiBlurManager()`, `getRenderTargetWidth/Height()`.
- `cleanup()` order: `Gui` -> background -> blur registry/renderer/manager -> resolution -> fonts -> RT -> render manager -> data manager -> log manager/source.

## RenderPlatform/ (MyGUI -> rend)

| Class | MyGUI base | Notes |
|---|---|---|
| `RPRenderManager` | `RenderManager`, `IRenderTarget` | Owns sampler/blend/rasterizer/depth states, one `PipelineContext`, two shader sets (`shaderPlain`, `shaderTextured` = VS+PS+InputLayout), texture map by name. Vertex format `ColourABGR`; only `R8G8B8A8` supported. |
| `RPTexture` | `ITexture` | Backed by `rend::Texture2D` (GPU size can be power-of-two, `getTextureWidth/Height`) vs logical `getWidth/Height`. CPU `pixels_` staging for `lock/unlock`; on-disk cache (`loadCache/storeCache`). Can expose a render target (`RPRTTexture`). |
| `RPRTTexture` | `IRenderTarget` | Render-to-texture for an `RPTexture`; own `PipelineContext` + `RenderView`. |
| `RPVertexBuffer` | `IVertexBuffer` | Wraps `rend::VertexBuffer`; `lock()` maps, `unlock()` unmaps. |

Key points:
- `uvScaleBuffer_` (constant buffer) scales MyGUI's logical-size UVs down to the power-of-two GPU texture.
- `setupContext(ctx, renderView, w, h)` binds the shared state, `drawBatch(ctx, buffer, texture, count)` draws one batch; both are reused by `RPRTTexture`.
- `setRenderView()` sets the main GUI target; `setViewSize()` updates MyGUI's view.
- `getInstance()` casts MyGUI's singleton pointer (no own instance).

## BlurrySubSkin/

Backdrop blur in three parts:

| Class | Role |
|---|---|
| `GuiBlurManager` | Captures the RT into `captureView_`, downsamples (default **2x**) and runs separable Gaussian: `copyPass_` -> `blurHorizontalPass_` -> `blurVerticalPass_` over `ping/pong` views. Result = `pongView_`. `BlurParameters {texelSizeX, texelSizeY}` (16-aligned CB per pass). `resize()` recreates targets. |
| `BlurrySubSkin` | `MyGUI::SubSkin` subclass (`MYGUI_RTTI_DERIVED`). Owns a `GuiBlurSkinTexture` composite RT and the atlas `RPTexture`. `updateComposite(backdropSRV, canvasW, canvasH)` renders backdrop+atlas into its own texture; `doRender()` draws it. |
| `BlurrySubSkinRenderer` | `composite(backdropSRV, backdropUV, atlasSRV, atlasUV, destRV, w, h)` via `FullscreenPass`. |
| `BlurrySubSkinRegistry` | Registers `BlurrySubSkin` as `"BasisSkin"` / `"BasisSkin/State"` factory (name `BlurrySubSkin`), tracks live instances, `updateAllInstances()`. |
| `GuiBlurSkinTexture` | Named `RPTexture` + `RenderView`; `initialize(name, w, h)`, `resize`. |

## DataManager / DataStream

- Implements `MyGUI::DataManager`: `getData`, `isDataExist`, `getDataListNames(pattern)`, `getDataPath`.
- Default search dirs (non-recursive): `$GAME_DATA/Gui`, `.../Editor`, `Fonts`, `MyGUI_Media`, `Beacon`, `EnemyMarkers`, `Handbook`, `LostItem`, `QuestMarker`, `QuestPins`, `Waypoint`, plus `$CHALLENGE_DATA/Gui` and `$SURVIVAL_DATA/Gui`.
- `addSearchDirectory(path, recursive)` / `removeSearchDirectory(path)` - this is how mod content dirs get added.
- `DataStream`: Win32 file mapping (`CreateFileMapping`/`MapViewOfFile`), read-only; `eof/size/readline/read`.

## FontManager

- `loadFontXML(path)`, `clearFonts()`; keeps `loadedFonts_`.
- Cache key = hash of the font XML element tree; cache path from `getCacheFilepath(fontName, hash)`. `loadFromCache` / `generateCacheFile` avoid re-rasterizing TTFs.

## ResolutionManager

- Holds a list of `{width, height, folder}` plus per-resolution resource files; `setResolution(w, h)` selects the matching entry and (re)loads its resources (`loadedResources`). `getResolutionWidth/Height()` return 0 if none is selected.

## LogListener

Maps MyGUI levels to console: Info = dropped, Warning = yellow, Error/Critical = red, unknown = magenta. Debug builds hide the "non power of two size" texture warning; fixes MyGUI's `dublicate` typo to `duplicate`.

## BackgroundImageHelper

`setBackgroundTexture(name)` (loaded through MyGUI data dirs), `render(renderView, w, h)` draws it with a `FullscreenPass` + sampler. No explicit `getBackground...` API.

## Known issues / watch-outs

- `GuiSystemManager::initialize()` ends with **debug code**: creates a `PanelEmpty` root widget on layer `Main` and loads a hard-coded workshop layout (`$CONTENT_3660881a-.../Computer.layout`). Remove/guard before release.
- Blur is captured *before* MyGUI draws, so panels only blur the background image, never other widgets.
- English fonts and the four config XMLs are hard-coded.
- `GuiSystemManager::render()` clears with `Aquamarine` (debug-looking clear colour).
- `GuiSystemManager::initialize()` returns early on failure without cleanup; `cleanup()` is only run by the destructor.
- `BlurrySubSkinRegistry::removeInstance` uses `std::remove` without `<algorithm>` in its header (works via transitive include).
