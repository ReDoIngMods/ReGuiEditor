# ReGuiEditor - Internal Docs

Quick-reference notes for navigating the codebase. Source root: `ReGuiEditor/`.

| Doc | Folder | Status |
|---|---|---|
| [Core.md](Core.md) | `Main`, `Application`, `ConfigManager/`, `FilesystemManager/` | done |
| [rend.md](rend.md) | `rend/` - D3D11 renderer wrapper | done |
| [MyGuiRenderer.md](MyGuiRenderer.md) | `MyGuiRenderer/` - MyGUI platform, blur skin | done |
| ... | `GUI/` (views, UIManager, fonts, notifications) | todo |
| ... | `LuaManager/` (VM, `sm.*` wrappers) | todo |
| ... | `LayoutManager`, `UGCManager`, `JSON`, `UTILS`, `Window`, `ImGuiCustom` | todo |

Conventions: `this->` for members, trailing `_` on private members, `[[nodiscard]] bool init(...)` for fallible setup, `XxxHandle` = `RefCountPtr<Xxx>`.
Virtual paths: `$GAME_DATA`, `$SURVIVAL_DATA`, `$RGE_DATA`, `$RGE_CACHE_DATA`, `$RGE_USER_DATA`, `$CONTENT_<id>` (see Core.md).
