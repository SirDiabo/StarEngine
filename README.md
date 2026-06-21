# StarEngine

StarEngine is a game engine for making lowpoly, low resolution games in the style of the fifth console generation (Nintendo 64 / PlayStation 1).

## Overview

Built on [raylib](https://github.com/raysan5/raylib) for windowing, input, audio, and rendering.

Features a visual scripting system called Stargraph, inspired by Unreal Engine's Blueprints.

## Projects

Projects are stored as `.starproject` files in `Documents/StarProjects/`.

Each project contains:
- `Content/` - Assets managed by the content browser
- `Config/` - Project configuration files
- `Saved/` - User settings, logs, and temporary data

### Asset System

The content browser imports external assets (PNG, WAV, FBX) without copying them. Instead, it creates cooked `.starasset` files that reference the original source paths. This enables:
- Fast reimports when source files change
- No duplication of original assets in the project
- Easy asset management within the engine

Supported asset types:
- `.stargraph` - Visual script graphs for actors
- `.starasset` - Cooked assets (textures, audio, models)
- `.starmap` - Scene/level files

## Modules

Core functionality is separated into loadable modules. Each module extends the engine:

- Core - Application framework, module system
- Audio - Sound and music handling
- Particles - Particle system (separate module)
- Renderer - Rendering utilities
- Input - Input mapping and handling
- Stargraph - Visual scripting system

## Project Structure

```
src/
  Core/                         - Core application framework
  Editor/                       - Editor framework and panels
  Modules/                      - Loadable engine modules
  Runtime/                      - Runtime components
```

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
