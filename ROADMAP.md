# StarEngine Development Roadmap

## Vision
StarEngine is built for developers making games that run on anything.

Target aesthetic: Super Mario 64, PS1 Final Fantasy, low-poly, stylized, fast.
Target hardware: anything with a GPU from the last 20-30 years. Both editor and packaged games should run on a toaster.

---

## Status Key
x Done
/ Partially done or has known bugs
o Not started

---

## 1. Editor Layout and Panels

### 1.1 Core Panel Hierarchy System
x PanelManager replaced with DockArea managing a recursive DockNode split tree
x DockNode hierarchy: leaf (one or more tabbed panels) or split (horizontal/vertical, two children)
x Each panel has: name, visibility, bounds (set each frame by layout pass)
x Layout serialization to ~/.starengine/layout.cfg (key=value, no external deps)

### 1.2 Tabbed Panel Containers
x Tab bar rendering at top of each dock leaf with panel titles
x Active tab indicator with blue underline highlight
x Click tab to switch active panel
x Close button (X) on each tab hides the panel
x Drag threshold before tab detach begins (6px)

### 1.3 Detaching and Floating Windows
x Drag tab past threshold begins detach drag operation
x Drag ghost visual follows cursor showing panel name
x Release over empty space: panel becomes floating window within the editor
x Floating panels have their own header, close button and drag-to-move
x Release floating panel header over dock area: re-docks via drop zones

### 1.4 Docking Zones and Split Detection
x Five drop zones per leaf: Left, Right, Top, Bottom, Center (tab merging)
x Visual highlight of the target zone while dragging
x Drop on tab bar of another leaf: merges panel as a new tab (Center)
x Drop on content zone: creates a new split node in the hierarchy
x Split detection threshold: 25% from each edge triggers directional split

### 1.5 Resizing Docked Panels
x Resizable split bars between docked panels
x Mouse hover shows resize cursor (EW or NS)
x Drag split bar to resize adjacent panels proportionally
x Minimum size constraint (60px) prevents panels from disappearing
x Split ratios stored in layout and serialized to disk

### 1.6 Window Menu Integration
x Window menu in MenuBar lists all panels
x Click to toggle panel visibility
x Restored hidden panels attach to root leaf or float if tree is complex
x Reset Layout option restores UE5-style default arrangement
x Checked state indicator next to visible panels in the menu

### 1.7 Layout Persistence
x Save layout to ~/.starengine/layout.cfg on editor shutdown
x Layout includes: dock hierarchy, split ratios, active tabs
x Load layout on editor startup
x Reset layout to default via Window menu

### 1.8 Editor Tab System (Two-tier, UE5 parity)
x Major editor tabs in top bar: Level Editor (permanent) and asset editors
x Level Editor tab cannot be closed or detached
x Opening a .staractor/.stargraph opens a new named editor tab beside Level Editor
x Switching editor tabs rebuilds the menu bar with tab-relevant menus
x Closing an asset editor tab removes it; Level Editor always remains
x Asset editors that are already open are focused rather than reopened

### 1.9 Shared Property Field Widgets
x PropertyFields slots (src/Editor/PropertyFields.h/.cpp): FloatBox, IntBox, StringBox, BoolBox, Vec3Row
x Fix raygui quirk where global cursor-index shared across text boxes; force-activates fields so cursor appears at text end
x Click-drag-to-scrub on FloatBox/IntBox
o Array-of-fields wrapper - deferred until array-typed variables exist; just loops existing fields with an incrementing id

---

## 2. Engine Architecture

### 2.1 Object Hierarchy
x Object base class (name, id)
x Actor (transform, BeginPlay/Tick/EndPlay)
x Property reflection system (PropertyList, PropertyHandle)

### 2.2 Coordinate System and Units
x Unit scale: 1 unit = 1 centimeter
x Rotation Unit being Angle in degrees
x Scale Unit being multiplier for base scale (2 makes it 2x bigger, 0.5 makes it smaller by half)

### 2.3 World / Scene
x World singleton (SpawnActor, DestroyActor, BeginPlay, Tick, EndPlay)
x World drives actor lifecycle from Application main loop
x Actor component system (Component.h, SceneComponent.h/cpp, Actor holds component list)
x Runtime spawn/destroy actor via graph nodes
x Full component system (mesh, collision, audio components)
x Components declared on a StarActor class are instantiated as runtime Component
  objects when the class is spawned into the World (PIE), not just the ad hoc mesh path

### 2.4 Module System
x Module base class and ModuleManager
x StargraphModule registers via ModuleManager
x Module Browser panel (enable/disable per project)
x Third-party module import
x Module manifest format (.starmodule)

### 2.5 Module-Gated Registration and Rebuild-on-Import
x NodeRegistry entries tagged with their owning module, filtered by IsEnabled() when populating the Add Node menu
x AssetTypeRegistry entries tagged with their owning module, filtered by IsEnabled() in content browser create menu and double-click-to-open
x ModuleManager::InitializeAll skips Initialize() for modules disabled at startup (currently runs regardless of enabled state)
x Disabling a module hides/blocks its asset types and nodes everywhere in the editor, not just its Update() tick
x Trigger an engine rebuild (cmake --build) from the Module Browser after an import, instead of requiring a manual rebuild
o On successful rebuild, relaunch the new editor binary with the current project path so the freshly imported module is live without the user manually restarting
o Project/layout state preserved across the relaunch (open editor tabs, active map, dock layout)
o Rebuild failure surfaces the compiler error in the Log panel instead of silently leaving the old binary running

### 2.6 Component editing at runtime
x Add references of components nodes in the graph editor, similar to variable get and set. To get a reference of a component.
o Add certain nodes to all components, like "Set Relative Location" and such. Also World location using actor scale as offset.
o Add "Target" to existing nodes: "Get Actor Location" and similar take "Target" Object input pin, with self as default.
o Add a new camera component with a couple simple properties like float-FOV, and whatever else raylib 3d Cameras expose.
x Actors with camera components automatically view the first camera, but are able to change view target through nodes.
o Add nodes to other existing components: "Set Static Mesh" for Mesh Component, etc.

---

## 3. Asset Architecture

### 3.1 Actor Classes as Primary Asset (UE5 Blueprint parity)
x Replace "New Stargraph" with "New Actor" in content browser right-click menu
x StarActor asset (.staractor) combines: parent class, variables, and event graph
x Double-clicking a StarActor opens its graph editor
x StarActor graph editor exposes "Self" reference (the actor instance at runtime)
x StarActor classes drag-droppable from content browser into viewport to place instances
x Dropping a StarActor into the viewport creates an ActorInstance in the StarMap referencing that asset
x StarMap instance records reference StarActor by asset path (no embedded graph data per instance)
x Content browser icon for .staractor files

### 3.2 Asset Registry
x AssetInfo, AssetTypeRegistry
o Asset registry persistence (scan and cache on project load)
o Asset reimport on source file change
o Asset cooking pipeline (for eventual game packaging)

### 3.3 Scene Format (StarMap)
x StarMap file format (.starmap) with actor instance records
x Instance records: name, class path, transform
x Save/load round-trip
x StarMap set as project default map in ProjectSettings
o Load/unload levels at runtime via graph nodes

---

## 4. Project System

### 4.1 Project Browser
x New project creation, existing project listing and open
x Project settings file (.starproject)
x Project settings panel (target resolution, startup map)
o Recent projects list persisted between sessions

### 4.2 Project Save / Load
x Project::Load/Save, triggered from File menu
o Auto-save on interval
o Unsaved changes indicator in title bar

---

## 5. Graph Editor (Stargraph)

### 5.1 Graph Rendering
x Dark canvas with dot grid (fixed screen size regardless of zoom)
x Node header colors: red=event, blue=function
x UE5 pin colors: exec=white, bool=red, int=cyan, float=green, string=magenta, vec3=orange
x Bezier wire rendering with Alt+click to delete
x Zoom with scroll wheel
x Pan with middle mouse drag
x Node display names: underscores replaced with spaces

### 5.2 Node Interaction
x Drag nodes by header with 16-unit grid snapping
x Single node selection, deselect on empty click
x Delete selected node(s) with Delete key
x Box/drag selection with visual rectangle
x Multi-node move together
x Ctrl+click node to add/remove from selection
o Ctrl+D to duplicate selected node(s)
o Ctrl+C and Ctrl+V to copy selected nodes and paste to cursor position

### 5.3 Pin and Wire Interaction
x Click pin to start wire, release on compatible pin to connect
x Release on empty space opens Add Node menu filtered to compatible types
x Alt+click pin to delete all links on that pin
x Ctrl+drag pin to detach and re-route
x Alt+hover wire turns red, alt+click deletes it
o Right-click a wire: popup with "Break Link" / "Add Reroute Node"
o Double-click a wire inserts a reroute node

### 5.4 Add Node Context Menu
x Searchable, scrollable, grouped by category
x Filters to compatible types when opened from pin drag
x Node names display with spaces, scroll does not bleed to graph zoom
x Menu repositions to stay within graph area bounds

### 5.5 Left Panel (My Blueprint)
x Graphs, Variables, Functions, Macros sections
x Variable creation, inline rename, type-colored indicators
x Delete variable with Delete key, right-click rename/delete popup
x Drag variable to canvas: drop=Get, Alt+drop=Set
o Functions section: user-defined functions, double-click opens sub-graph
x Components Section
x Component selection is exclusive with node/variable selection
x Components carry a relative transform and per-type data (mesh/material refs,
  collision shape/radius/height, sound path/autoplay/looping/volume), not just a name
o Collapse/expand sections

### 5.6 Right Panel (Details)
x Selected node name/type display
x Selected variable details (name, type, default value, bool checkbox)
x Component details: Position/Rotation/Scale and type-specific fields (mesh + material
  picker, collision shape/radius/height, audio path/volume/autoplay/looping)
x Active field tracking prevents simultaneous text input
o Node input pin defaults editable in details panel

### 5.7 Toolbar
x Asset name, unsaved indicator, Save (Ctrl+S)
x Play button executes Event_BeginPlay on graph
x Variable values snapshot before play, restored after stop
o Compile button (validate graph, report errors to log)
o Graph breadcrumb trail for sub-graphs

### 5.8 Text Input Quality of Life
o All inline text fields open with text pre-selected
o Typing immediately replaces pre-selected text
o Ctrl+A selects all in active field
o Ctrl+C/V/X in text fields

---

## 6. Variables and Pin Types

### 6.1 Current Types
x Exec, Boolean, Integer, Float, String, Vec3, Object

### 6.2 Wildcard Type
x PinType::Wildcard - hollow diamond, light gray unresolved
x Adopts color/type of first connected typed pin
o All wildcard pins on same node resolve together
o Disconnecting resets to unresolved
x Executor resolves wildcards before evaluating

### 6.3 Node Consolidation (post-wildcard)
x Replace Float/Int/Vec3 math nodes with single wildcard Add/Subtract/Multiply/Divide

---

## 7. Built-in Nodes

### 7.1 Events
x Event_BeginPlay, Event_Tick (Delta Seconds), Event_EndPlay
o Custom Event nodes (user-named, callable from other graphs)
x Input events: Event_Key_W/A/S/D/Space/etc. (fired while key held or on press/release)
x Get_Axis_Horizontal, Get_Axis_Vertical (returns -1..1 float, WASD + arrows)
o Input events: Event_Mouse_Button (left, right, middle)

### 7.2 Flow Control
x Branch, For_Loop, While_Loop, Sequence
o Do_Once, Gate, Flip_Flop
o Delay (requires timer system)

### 7.3 Math
x Float/Int/Vec3 Add/Subtract/Multiply/Divide
x Conversions: Float/Int/Bool to String, Float to Int, Int to Float
o Float: Clamp, Lerp, Abs, Min, Max, Round, Floor, Ceil
o Int: Clamp, Abs, Min, Max, Modulo
o Vec3: Length, Normalize, Dot, Cross, Distance, Scale
o Vec3: Make (X, Y, Z -> Vec3), Break (Vec3 -> X, Y, Z)

### 7.4 Actor Self-Reference Nodes
x Get_Actor_Location (pure, reads ctx.actor->position)
x Set_Actor_Location (exec, writes to ctx.actor->position)
x Get_Actor_Forward_Vector (pure, derived from actor rotation)
x Get/Set Actor Rotation
x Get/Set Actor Scale
o Get Actor Right Vector, Get Actor Up Vector
o Destroy Self
o Spawn Actor (class, location, rotation -> actor reference)

### 7.5 Utility
x Print_String (inline text field, outputs to log panel)
o Print_Warning, Print_Error (yellow/red in log)
o Get_Game_Time (returns elapsed seconds as float)
o Get_Delta_Time (returns last frame delta, usable outside Event_Tick)

### 7.6 Variables
x Get_Variable / Set_Variable via left panel drag
/ Get/Set functional at runtime (needs actor context integration)
x Variable_Name internal pin hidden from node face

---

## 8. Graph Executor
x Exec pin chain walking with safety limit
x Data pin evaluation with recursive upstream propagation
x Flow control: Branch, For_Loop, Sequence handled specially
x ExecutionContext struct (Actor*, float deltaTime) threaded through all nodes
x Event_Tick writes deltaTime into Delta Seconds output pin before execution
x StargraphBehavior passes actor pointer and deltaTime from World::Tick
x Variable snapshot/restore around play so runtime writes don't persist to file
o Wildcard type resolution at runtime
o Async nodes (Delay, latent actions)

---

## 9. Play-In-Editor (PIE)
x Play button in viewport spawns all StarMap actor instances into World
x Each instance loads its .staractor graph as StargraphBehavior
x World::BeginPlay fires for all actors
x World::Tick fires every frame, actor Event_Tick graphs run with delta time
x Input nodes read from raylib keyboard state during tick
x Stop button cleanly destroys all runtime actors and restores editor state
x Camera returns to editor view on stop
x Log panel shows runtime output during play
o Editor only box that shows warnings, errors and prints on the top left of the viewport 

---

## 10. Game Packaging
x Runtime-only CMake build target (no Editor/ code, no StargraphEditorPanel)
x Packaged executable loads a StarMap directly, runs the world
x Asset path resolution relative to packaged executable
x All .staractor and .starmap files included in package output
x Platform targets: Windows, Linux (Windows via mingw-w64 cross toolchain, verified; macOS needs osxcross, wired but unverified; Web wired via emcmake, unverified, no single-executable output; Android not implemented, needs a full APK project template)
o option for single-file packaging (embed assets into binary)
x Content encryption at cook time (XOR, extensible to AES-128/256 later)
x Renderer choice per project (Hardware/OpenGL or Software/CPU via raylib's rlsw backend, packaged games only)

---

## 11. Viewport Camera
| Input | Behavior | Status |
|---|---|---|
| Right-click drag | Free-look FPS rotation | [x] |
| Right-click + W/S/A/D | Fly camera | [x] |
| Right-click + Q/E | Fly up/down | [x] |
| Right-click + Scroll | Adjust fly speed | [x] |
| Middle-click drag | Orbit | [x] |
| Scroll wheel | Dolly zoom | [x] |
| F | Focus on selected actor | [x] |
| Alt + Left drag | Orbit (alternative) | [ ] |

---

## 12. Assets
Place all in `StarEngine/Assets/`.

| File | Size | Status |
|---|---|---|
| `logo.png` | 256x256 RGBA | [x] |
| `icon_actor.png` | 32x32 RGBA | [x] |
| `icon_starmap.png` | 32x32 RGBA | [x] |
| `icon_folder.png` | 32x32 RGBA | [x] |
| `icon_asset.png` | 32x32 RGBA | [x] |

x Load and display icons in content browser tiles
o Display logo on top left of the editor
o Display logo on project browser screen

---

## 13. Future Modules

### 13.1 Audio Module
o WAV playback, MIDI parsing, SoundFont (SF2) synthesis
o Adaptive music layer system (N64 style)
o Audio graph nodes: Play Sound, Stop Sound, Set Volume, Set Pitch

### 13.2 Networking / Replication Module
o Replicated variable annotation, automatic variable sync
o Movement prediction
o RPC graph nodes, simple lobby/session system

### 13.3 Physics Module
o collision shapes, physics graph nodes

### 13.4 Rendering / Visual Style
o Simple light sources
o Generating shadow maps
o Runtime shadow maps (stencil, no tracing)
o PSX vertex snapping, affine texture mapping
o Render scale / pixel art mode, color palette post-process, vhs pixel smear and scan lines
o Skeletal mesh and animation import

### 13.5 Meshes Module
x Meshes module (src/Modules/Meshes) registers module-gated StaticMesh (.starasset),
  Texture (.startex), and Material (.starmat) asset types
x StarAsset format records Type + Source path; no file copies
x Single Content Browser "+" Import button handles mesh and texture, infers type from extension
x Asset picker dropdowns no longer close on open, render z-fight, or click-through (deferred draw)
x StarAsset stores import transform + default material in cooked file
x Import Options popup (ImportDetailsPanel) after cooking to set transform/material
x Static mesh import defaults to 100x scale (DCC tools export in meters)
x Dragging a mesh to viewport seeds instance transform/material from asset data
x Drag StaticMesh asset to viewport to place instance
x Edit-mode viewport renders actual model (raylib LoadModel/DrawModel) with fallback placeholder
x Edit-mode viewport renders declared components for StarActor instances
x PIE instantiates full declared component set (Mesh/Collision/Audio) on spawn
x Model cache keyed by source path, shared between edit-mode and PIE
x Texture cache keyed by .startex asset path
x StarMaterial: flat color + optional diffuse texture, unlit, edited inline in Content Browser
x MeshComponent carries Static Mesh + Material references; no direct .mtl edit needed
x Position/Rotation/Scale, Static Mesh, Material editable in Details panel for instances
x Component details editable in StargraphEditorPanel right panel
x Selection uses wireframe overlay so assigned material stays visible
x Actor-level scale is a uniform multiplier (default 1.0); placeholder is 50-unit nominal scaled
x Class-graph cache invalidated on save so edited components reflect immediately
/ FBX not supported (maybe find way to convert?)
o Reimport on source file change
o Mesh/texture/material thumbnail preview in content browser
o Content-browser drag-and-drop target inside StargraphEditorPanel
o Multiple components of same type on one actor; convenience accessors for most recently added
o Physics/collision response (CollisionComponent is authorable/visual but no movement/overlap)

---

## 14. CI / Build
x GitHub Actions: Windows, Linux, macOS
x Artifacts zipped per platform
x Release per push using CMake version + run number tag
x CONFIGURE_DEPENDS on GLOB_RECURSE
o Runtime-only build target (no editor code)
o Unit tests in CI
o CI build caching

---

## 15. Asset Preview Tab
Double-clicking a StaticMesh asset opens a dedicated tab: viewport with the mesh,
import details panel on the right, and Reimport button in the top bar. Not started.

o Extract reusable ViewportPanel pieces (orbit/pan camera, model/texture cache) for
  shared use between level and preview viewports
o New AssetPreviewTab (EditorTab subclass)
o Wire double-click on .starasset to open in AssetPreviewTab
o Center panel: single-mesh viewport using extracted camera/model code
o Right panel: reuse existing ImportDetailsPanel
o Top bar: Reimport button re-cooks from stored SourcePath without file browser
o No-scope: skeletal mesh/animation preview (depends on 13.4)