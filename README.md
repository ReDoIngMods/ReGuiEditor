<img src="https://placehold.co/1920x1080" alt="ReGui Editor Banner" width="100%" height="auto">

<img align="left" src=".github/regui-icon.png" alt="ReGui Editor Icon" width="100" height="100">

<img src=".github/regui-title.svg" alt="ReGui Editor">

<p>ReGui is an advanced GUI library built on top of <a href="https://store.steampowered.com/app/387990/">Scrap Mechanic</a>'s JsonGui feature, providing Quality-of-life features aswel as making complex UI a lot easier to create and manage.</p>

<br clear="left">

> [!WARNING]
> ReGui is **still in development** and its unrecommended to use it in production. Expect bugs and missing features!

## Features
 
The ReGui Editor has a lot more features unlike the MyGui Layout Editor and the built-in JsonGui Editior in the game. Such as:
- **Copy & Paste**: Copy and paste widgets, including their children and properties.
- **Custom Widgets**: Make your own custom widgets and use them in the editor, allowing you to quickly make detailed layouts in a fraction of the time.
- **Addon Support**: ReGui Editor automaticly supports all `sm.regui` addons without any extra work by emulating the game's lua enviroment.
- **Easy to use**: The editor is designed to be easy to use, with a simple and intuitive interface that allows you to quickly create and edit your GUI layouts.
- **JsonGui Support**: The editor supports both ReGui and JsonGui, allowing you to create layouts incase you don't want to use ReGui.
- **Near 1:1 accurate rendering**: The editor uses MyGui to render the layouts and copies the same structure that the game uses, so what you see in the editor is what you see in the game.
## Installation
 
1. Download the latest [release](../../releases/latest).
2. Extract the archive to any folder.
3. Run `ReGuiEditor.exe`.
> [!TIP]
> Pin a shortcut to your desktop or taskbar for quick access.
 
## FAQ
 
### Why a dedicated GUI editor?
 
MyGui Layout Editor is a fine tool, but it only supports GuiInterface, not the newer JsonGui, which is far more flexible. It also lacks ReGui's quality-of-life features (custom widgets, copy & paste, ...) and can't render ReGui-only widgets.
 
Near-100% rendering accuracy additionally requires emulating `sm.regui` inside the editor, which MyGui Layout Editor can't do. So ReGui got its own editor.
 
### Can I use the editor for plain JsonGui, without ReGui?
 
Yes. The editor supports both. Some custom widgets, such as **UIPadding**, also work in plain JsonGui.
 
The catch: widgets that rely on ReGui-specific behavior or rendering are ReGui-only. If you stick to standard JsonGui widgets, your layouts stay fully JsonGui-compatible.
 
## Contributing
 
Contributions of all kinds are welcome: bug fixes, new features and documentation improvements. To contribute:
 
1. Fork the repository and create a branch for your change.
2. Make your changes and test them in the editor.
3. Open a pull request describing what you changed and why.
Found a bug or have an idea? [Open an issue](../../issues).
 
## Thanks
 
Thanks to everyone who helped shape ReGui:
 
- **Uch/Comeone**: helped with the structures for ReGui **V3**.
- **Fagiano**: suggestions and recommendations.
- **Donut05**: suggestions, recommendations, and the ReGui icon.
- **The Red Builder**: invented the technology ReGui is built on. Without it, ReGui wouldn't exist.

## License
 
Licensed under the [MPL-2.0](LICENSE) license.
