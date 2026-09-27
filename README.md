# StarEngine

StarEngine is a indie-first game engine for games in the style of the fifth console generation.

## Overview

Built on [raylib](https://github.com/raysan5/raylib) for windowing, input, audio, and rendering.

## Projects

Projects are stored as `.starproject` files in `Documents/StarProjects/`.

Each project contains:
- `Content/` - Assets managed by the content browser
- `Config/` - Project configuration files
- `Saved/` - User settings, logs, and temporary data

## Modules

Core functionality is separated into loadable modules. Each module extends the engine:

- Stargraph - Visual Scripting System
- BlobShadows - Simple Lighting System.
- ShadowMap - Realtime ShadowMap Lighting System. 

## Project Structure

```
src/
  Core/                         - Core application framework
  Editor/                       - Editor framework and panels
  Modules/                      - Loadable engine modules
  Runtime/                      - Runtime components
```

## Design Principles

- All asset formats are plain text, git-diffable and mergeable, unless there's a hard technical reason otherwise.
- Project-level named registries (Input Mappings, Save Files, Languages) share one pattern: configured once at the project level, referenced by dropdown wherever needed. New systems needing this shape should reuse it.

## Stargraph System

The Stargraph visual scripting system consists of:
- **Nodes** - Building blocks with input/output pins
- **Links** - Connections between node pins
- **Variables** - Graph-local and object properties
- **Node Registry** - Central registry for all node types

Node types are loaded from separate files, enabling extensibility. Function nodes can themselves be Stargraph assets for meta-programming.

## Editor Layout

The main editor window consists of tabbed panels:
- Viewport - Scene preview and editing
- Details - Selected object properties
- Content Browser - Asset management
- Logs - Output console
- Stargraph Editor - Visual script editing

## Ownership

Games and other content created with StarEngine are the sole property of their respective creators.
StarEngine is a tool, the output you create with it is not a derivative work of the engine, and you retain full ownership and rights over your games.

## Support

If you find StarEngine useful, consider [supporting me on Ko-fi](https://ko-fi.com/sirdiabo).

## License

StarEngine is released under the [MIT License](LICENSE).

See [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES) for licenses of included third-party components (raylib).
