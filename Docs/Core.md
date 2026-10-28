# Core: `Main`, `Application`, `ConfigManager`, `FilesystemManager`

Path: `ReGuiEditor/` (root files) + `ConfigManager/` + `FilesystemManager/`. All managers are `UTILS::Singleton<T>` (`createInstance` / `getInstance` / `destroyInstance`).

## Main.cpp

`wWinMain`:
1. Per-monitor-v2 DPI awareness (thread + process).
2. Env `SteamAppId=387990` (Scrap Mechanic), `SteamNoOverlayUIDrawing=1`; `SteamAPI_InitEx` (needs Steam running + game owned).
3. `FreeImage_Initialise()`.
4. `Application::createInstance()` -> `initialize()`; on failure `destroyInstance()` and `return 1`.
5. Loop: `SteamAPI_RunCallbacks(); update(); render();` until `isRunning` is false.
6. `destroyInstance()`, `FreeImage_DeInitialise()`, `SteamAPI_Shutdown()`.

## Application

Owns the window, swap chain and every manager (public members, raw pointers).

Init order (`initialize()`, each step `return false` on failure; splash shows status text):

| # | Step |
|---|---|
| 1 | QPC timer, window callbacks (`onCloseRequest`, `onWindowResize`, `windowProc`), `window.create(L"ReGui Layout Editor", 1920, 1080)` (hidden until first frame) |
| 2 | `FilesystemManager::initialize()` |
| 3 | Splashscreen from `$RGE_DATA/Images/splash.png` (optional) |
| 4 | `rend::Renderer::init(hwnd)` + `createSwapChain(hwnd)` |
| 5 | `UGCManager::initialize()` (workshop content) |
| 6 | `ConfigManager::createInstance()` (loads config in ctor) |
| 7 | `UIManager::initialize()` (ImGui + views) |
| 8 | `GuiSystemManager::initialize()` (MyGUI, see [MyGuiRenderer.md](MyGuiRenderer.md)) |
| 9 | `LuaManager::initialize()` |

Frame:

```
update():   exit check -> deltaTime (QPC) -> window.update() -> viewsManager->update(dt)
            -> fixed step @ 40 Hz: luaManager->update(dt), viewsManager->tick()
render(vsync=true):
            luaManager->tick()
            -> clear swapchain RT (darkGray)
            -> guiSystemManager->render()              // MyGUI -> offscreen RT
            -> ImGui NewFrame -> viewsManager->render() -> ImGui::Render
            -> UpdatePlatformWindows / RenderPlatformWindowsDefault
            -> RGE_ImGui_ImplDX11_RenderDrawData(.., swapChain RTV)
            -> present(vsync)
            -> first frame only: window.show(), splashscreen.close()
```

- Resize: `onWindowResize` resizes the swap chain then calls `render(false)` (live redraw while dragging, no vsync). `onResize` only re-renders.
- `windowProc`: ImGui handler first (returns `true` if consumed), else `DefWindowProcW`.
- `onCloseRequest` always returns `true` (no unsaved-changes prompt yet).
- Teardown order (`~Application`): Lua -> GuiSystem -> UI -> Config -> UGC -> swapChain -> Renderer -> Filesystem -> window.
- Note: `Renderer` registers `Application::render` as the `D3D11Device` render callback, but the loop calls `render()` directly.

## FilesystemManager

Virtual paths: `"$KEY/relative/path"` -> `directories_[KEY] / rest`. Absolute native paths pass through unchanged (`lexically_normal`).

Keys registered in `initialize()`:

| Key | Resolves to |
|---|---|
| `$GAME_DATA` | `<install>/Data` |
| `$SURVIVAL_DATA` | `<install>/Survival` |
| `$CHALLENGE_DATA` | `<install>/ChallengeData` |
| `$PARTICLE_EDITOR_DATA` | `<install>/ParticleEditorData` |
| `$RGE_DATA` | `../Data` (relative to working dir, made absolute) |
| `$RGE_CACHE_DATA` | `../Cache` |
| `$RGE_USER_DATA` | `%APPDATA%\Axolot Games\Scrap Mechanic\User\User_<steamid64>` |

`<install>` comes from `SteamApps()->GetAppInstallDir`. `$CONTENT_<localId>` keys (workshop/mod content) are expected to be added via `addDirectory` (by UGCManager - not verified).

API:
- `addDirectory(name, path)` (no overwrite - `try_emplace`), `removeDirectory(name)`.
- `resolveDirectory(path, out)`, `exists(path)`, `getFileWriteTime(path, time_t&)`.
- `resolveContentData(path&, localId)`: rewrites a `$CONTENT_DATA/...` prefix to `$CONTENT_<localId>/...`; other keys are untouched.
- `readFile(path, vector<uint8_t> | string | vector<char> | ByteStreamReader&)` (memory-mapped, then copied).
- `writeFile(path, vector<uint8_t> | string | vector<char> | ByteStreamWriter)` (creates parent dirs, pre-sizes file, memory-mapped write).
- A path with no slash (`"$GAME_DATA"`) resolves to the directory itself.

## ConfigManager

- Binary file `$RGE_USER_DATA/ReGuiEditor/config.bin`.
- Format: `"RGEC"` magic, `u8 version=1`, `u16 count`, then `count x { string key, u8 type, value }`. Types: `0 bool, 1 int, 2 float, 3 string`.
- Values: `std::variant<bool, int, float, std::string>`; typed access must match the stored type exactly (`int` vs `float` mismatch = miss).
- Constructor: `loadConfig()` -> `createConfig(...)` defaults -> `saveConfig()` (only writes when `hasChanges_`).
- Current keys: `mygui.canvas.width` (1920), `mygui.canvas.height` (1080).
- API: `createConfig(key, default)` (registers default, inserts if missing), `getConfigVariable(key, out)`, `getConfigVariableWithDefault(key, def)`, `setConfigVariable(key, value)`, `resetConfig()`, `loadConfig()`, `saveConfig()`.
- Destructor does not save; call `saveConfig()` yourself after `set`/`reset`.

## Known issues / watch-outs

- **Bug:** `ConfigManager::setConfigVariable` has `if (iterator->second == value)` - should be `!=`. As written the value never changes and `hasChanges_` is set only when nothing changed.
- `FilesystemManager::readFile` / `writeFile` fail on **empty** files / empty data (`CreateFileMapping` rejects zero-length files).
- File I/O uses `CreateFileA` with `path.string()` (ANSI): non-ASCII characters in the install path or Windows username can break it. Prefer `CreateFileW` with `path.c_str()`.
- `$RGE_DATA` / `$RGE_CACHE_DATA` depend on the working directory at `initialize()` time (`../Data`).
- `resolveKey` does not sanitize `..` in the relative part.
- `getFileWriteTime` returns raw `file_clock` ticks (not a Unix `time_t`).
- Fixed-step loop has no accumulator clamp: after a long stall (e.g. window drag) it runs many `fixedUpdate` calls back-to-back.
- `Main.cpp`: Steam init failure does `return false` (= exit code 0); the failure path after `Application::initialize()` skips `FreeImage_DeInitialise` / `SteamAPI_Shutdown`.
- `ConfigManager::createConfig` has a redundant `iterator->second = value` after `emplace`.
