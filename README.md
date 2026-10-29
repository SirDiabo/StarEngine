# <img src="Assets/logo.png" width="24"/> StarEngine

> **StarEngine is very early in development!** APIs, file formats and editor behavior are subject to change, and projects made with one version may need migrating for the next.

StarEngine is an indie-first game engine for games in the style of the fifth console generation.

It is built on [raylib](https://github.com/raysan5/raylib) for windowing, input, audio and rendering.


## Features

- Level editor with a viewport, gizmos, grid snapping, an outliner and a place-actors panel
- Actors built from components: mesh, camera, light, audio, collision, physics, input and widget
- Two programming interfaces, a visual one and a text one, that you pick per actor and per widget
- Widget system for UI, with its own editor tab and hierarchy
- Skeletal animation, plus OBJ and GLB model import
- Collision with named collision layers
- Save slots and input mappings, configured once per project
- Actor replication for multiplayer
- Packaging to a standalone game, with compression, integrity checks and optional encryption
- In-editor profiler
- Modules: rendering, scripting and more can be added, replaced or turned off without touching the core

## Building

StarEngine uses CMake, and raylib is included as a git submodule, so clone recursively:

```
git clone --recursive https://github.com/SirDiabo/StarEngine.git
cd StarEngine
cmake -S . -B build
cmake --build build --config Release
```

If you already cloned without `--recursive`, run `git submodule update --init` first.

This builds the editor as `StarEngine`. A runtime-only build, used for packaged games, is available with `-DSTARENGINE_BUILD_GAME=ON`.

Windows and Linux are tested. macOS has not been tried yet.

## Projects

Projects are stored as `.starproject` files in `Documents/StarProjects/`.

Each project contains:
- `Content/` - Assets managed by the content browser
- `Config/` - Project configuration files
- `Saved/` - User settings, logs, and temporary data

## Programming Interfaces

Actor and widget logic is written through a programming interface. Each one is a module, you can use several in the same project, and the core engine does not depend on any of them.

### Stargraph

A node based visual scripting system.
- **Nodes** - Building blocks with input/output pins
- **Links** - Connections between node pins
- **Variables** - Graph-local and object properties
- **Functions** - Your own nodes, edited as their own graphs

### Starscript

A small, Python-like text language with its own code editor, including completion, diagnostics and undo.

```
# My Player Actor
var speed = 250.0
var jumps: int = 0
var grounded = true

event BeginPlay():
    print("Hello from Starscript")

event Input("Jump"):
    if grounded:
        Physics.add_impulse(Vec3(0, 5, 0))
        jumps += 1

event Tick(dt: float):
    if input_pressed("W"):
        self.position += self.forward * speed * dt
    if input_pressed("S"):
        self.position += self.forward * speed * -dt
```

Widgets can be scripted the same way, reacting to element events such as `event StartButton.Clicked():`.

## Modules

Core functionality is separated into loadable modules. Each module extends the engine:

- Stargraph - Visual scripting system
- Starscript - Text scripting language
- BlobShadows - Simple projected blob shadows
- ShadowMap - Real-time shadow mapping

Modules can be browsed and toggled from inside the editor.

## Project Structure

```
src/
  Core/             - Core application framework
  Editor/           - Editor framework and panels
  Modules/          - Loadable engine modules
  Runtime/          - Runtime components
```

## Design Principles

- All asset formats are plain text, git-diffable and mergeable, unless there's a hard technical reason otherwise.
- Project-level named registries (Input Mappings, Save Files, Languages) share one pattern: configured once at the project level, referenced by dropdown wherever needed. New systems needing this shape should reuse it.
- The core engine knows nothing about specific modules. Modules plug in through registries and hooks.
- raylib is the only third-party dependency of the engine itself. Anything that needs another library becomes an external module.

## Editor Layout

The main editor window consists of tabbed panels:
- Viewport - Scene preview and editing
- Details - Selected object properties
- Content Browser - Asset management
- Logs - Output console
- Actor and widget editors - Scripting, components and UI design

## Community

Questions, feedback, bug reports and ideas are all welcome in [Discussions](https://github.com/SirDiabo/StarEngine/discussions). Issues and pull requests are turned off for now.

## Ownership

Games and other content created with StarEngine are the sole property of their respective creators.
StarEngine is a tool, the output you create with it is not a derivative work of the engine, and you retain full ownership and rights over anything made with it.

## Support

If you find StarEngine useful, consider [supporting me on Ko-fi](https://ko-fi.com/sirdiabo).

## License

StarEngine is released under the [MIT License](LICENSE).

See [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES) for licenses of included third-party components (raylib).
