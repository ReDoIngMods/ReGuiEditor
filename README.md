<img src="https://placehold.co/1920x1080" alt="ReGui Editor Banner" width="100%" height="auto">

<div style="display: flex; align-items: center; gap: 20px;">
    <img src=".github/Icon.png" alt="ReGui Editor Icon" width="100" height="100">
    <div style="width: 100%;">
        <img src=".github/stupid-title-bypass.svg" alt="ReGui Editor Title" width="100%" height="auto">
        <p>ReGui is a advanced GUI library built on top of <a href="https://store.steampowered.com/app/387990/">Scrap Mechanic</a>'s JsonGui feature. This is the GUI editor for ReGui</p>
    </div>
</div>

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

- Download the latest Release
- Extract it anywhere you want
- Run `ReGuiEditor.exe` and start creating your GUI layouts!

> [!NOTE]
> You probably also want to make a shortcut to the executable and place it somewhere on ur desktop and/or taskbar for easy access.

## Frequently Asked Questions

There's obviously a lot of questions about ReGui and its editor, so here are some of the most common ones:

### Why a entire dedicated GUI editor?

MyGui Layout Editor is a fine tool for creating GUI layouts, however it doesn't support the new features of JsonGui but just only GuiInterface (which is quite limiting unlike JsonGui)

It also doesn't have any **Quality-of-Life** features that ReGui has, like custom widgets, Copy & paste and more. We also wanted to add widgets that MyGui itself doesn't have.

And internally we wanted if not near-100% accuracy with rendering and so we also have to emulate `sm.regui` in the editor, which is not possible with MyGui Layout Editor.

Because of that, ReGui has to get its own dedicated GUI editor, which is what this project is.

### What if i don't want to use the benefits of ReGui and just want to use JsonGui?

ReGui Editor supports both ReGui and JsonGui, and some custom widgets can even be used in JsonGui, such as **UIPadding** and more. So even if you don't want to rely on ReGui, you can still use the editor to create JsonGui layouts and still get additional benefits to make your life easier.

The only downside is that there are widgets are exclusive to ReGui, so if you use them, you won't be able to use them in JsonGui. But if you stick to normal widgets avaliable in JsonGui & widgets that dont require special behaviour with rendering, you can still use the editor to create JsonGui layouts.

## Contributing

If you want to contribute to the project, you can do so by creating a pull request on GitHub. We welcome contributions of all kinds, including bug fixes, new features, and documentation improvements.

## Thanks

We would like to thank the following people for their contributions to the project:
- **Uch/Comeone** for initially helping with the structures for ReGui **V3**
- **Fagiano** for suggestions and recommendations on the project.
- **Donut05** for suggestions, reccommendations aswel as creating the ReGui Icon.
- **The Red Builder** for originally inventing the technology, without it ReGui wouldn't even be thought of.

## License

This project is licensed under the MPL-2.0 License. See the [LICENSE](LICENSE) file for more information.