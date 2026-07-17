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
x Array-of-fields wrapper (ArrayHeader/ArrayRowControls/ArrayValueRect in PropertyFields.h/.cpp)
x Newly activated field seeds its buffer from the source value on the activation frame itself
x Registry-reference dropdown field (RegistryPickerPopup + NamedListRegistry), project-level
  named lists (Save Files, Languages, etc.) scoped by category string, with inline create-new

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

### 2.4.1 Project-Scoped External Modules
x Project only Modules. ModuleManager scans both the engine's built-in src/Modules and the current project's Modules/ directory
x .starmodule manifest optional repository field (owner/repo) for catalog/repo-imported modules
x A module folder with its own CMakeLists.txt is add_subdirectory()'d and registers its link target via starengine_register_module(), instead of being globbed as loose source
x Networking module split out of the engine source tree into its own standalone repo folder
x Module Browser's single Import button replaced with three import paths: from disk, from a typed "owner/repo" repository, and from a curated plain-text catalog index
o Blocking "this runs native code" trust warning before importing a non-catalog repository (currently just listed as "(external)" on the row)
o "Update module" action (re-clone/diff against the pinned repository). Using release tab tags or latest commit date of main branch to detect updates.

### 2.5 Module-Gated Registration and Rebuild-on-Import
x NodeRegistry entries tagged with their owning module, filtered by IsEnabled() when populating the Add Node menu
x AssetTypeRegistry entries tagged with their owning module, filtered by IsEnabled() in content browser create menu and double-click-to-open
x ModuleManager::InitializeAll skips Initialize() for modules disabled at startup
x Disabling a module hides/blocks its asset types and nodes everywhere in the editor, not just its Update() tick
x Trigger an engine rebuild (cmake --build) from the Module Browser after an import, instead of requiring a manual rebuild
x On successful rebuild, relaunch the new editor binary with the current project path so the freshly imported module is live without the user manually restarting
x Project/layout state preserved across the relaunch (open editor tabs, active map, dock layout)
x Rebuild failure surfaces the compiler error in the Log panel instead of silently leaving the old binary running
x CMake auto-reconfigures on module import via CONFIGURE_DEPENDS on the module-discovery globs, instead of requiring a manual cmake reconfigure before the rebuild

### 2.6 Component editing at runtime
x Add references of components nodes in the graph editor, similar to variable get and set. To get a reference of a component.
x Add certain nodes to all components, like "Set Relative Location" and such. Also World location using actor scale as offset.
x Add "Target" to existing nodes: "Get Actor Location" and similar take "Target" Object input pin, with self as default.
x Add a new camera component with a couple simple properties like float-FOV, and whatever else raylib 3d Cameras expose.
x Actors with camera components automatically view the first camera, but are able to change view target through nodes.
x Add nodes to other existing components: "Set Static Mesh" for Mesh Component, etc.

### 2.7 Input Focus and View Target
x Get/Set Input Focus nodes exist; Input_Key only processes for the actor holding input focus
x Input Focus is per-machine and unreplicated; it decides which actor reads local input, independent of Has Authority
x Get/Set View Target node. Actor's CameraComponent becomes the active render camera
x Only one actor holds input focus / view target at a time by default. Switching is explicit via nodes

### 2.8 Replication Core
x ReplicationMode enum on variables and custom events: Local (default) / Replicated / Server Only
x Local = never leaves the machine, Replicated = client-authoritative, syncs up then out, Server Only = write-gated to host, syncs out
x Property-diff snapshot path for variables, separate fire-once event queue for custom events
x Has Authority node (Host / Client branches). Always-true when no networking module is active
x Abstract NetworkBackend interface a networking module registers against, module-gated same as 2.5

### 2.9 Player Actor Nodes
x Add Movement Input nodes - accumulate, normalize, move actor directly
x Add Control Yaw / Add Control Pitch / Add Control Roll accumulate into normalized rotation delta
x Get/Set Control Rotation - to be used with camera rotation or actor yaw
x Pitch clamp to prevent flip 89°
x Add Use Control Control Yaw/Pitch/Roll (seperate) to both Actor and Camera Component. Actor Yaw true by default, Camera Pitch true by default.

### 2.10 Save / Load System
o Save flag on variables and component fields (similar to 2.8)
o Project-level Save Files registry (name, encrypted, portable) - per-variable field picks a name, doesn't restate file settings
o PIE Save files live in the project's existing Saved/ directory, Packaged in common OS save directories (AppData, .local, etc.)
o Save All / Save Variable (dropdown-picked) nodes (Self Scoped. Each Actor saves its own stuff)
o Load File / Load All nodes

### 2.11 Input Component and Mapping
o InputComponent: authored list of named mappings (name, default key) in Details panel
o Mapping types: Bool (button), Axis (scalar), Axis2D (reuses Vec3, Z unused)
o Auto-generates graph nodes per mapping (e.g. "Key Jump"), built on existing Input_Key polling
o Generated nodes scoped to graphs of actor classes that own the relevant InputComponent (extends 5.4 Add Node filtering)
o Get InputComponent + Override Input node for runtime rebinding
o Rebind overrides are Save-flaggable (2.10) for persistence across sessions
o Input Device concept: Keyboard/Mouse always active by default, gamepads added as separate devices once claimed
o Device claim flow: unclaimed devices listen for any input, first press assigns that device to the next open player slot, only active once an Input Component requests multiple devices
o Rebind overrides stored per device, giving each local player their own control scheme
o Mapped input event nodes pick their target Input Component from a details panel field instead of an input pin
o Multiple Input Components per actor, one per device, once multi-component-per-type support exists (13.5)

### 2.12 Component Hierarchy
x Component::parent consulted for world transform composition (Actor::GetComponentWorldTransform walks the parent chain, used everywhere a component's world position/rotation is computed)
x Editor field to set a component's parent among the actor's other components (Details panel Parent field, cycles through valid non-cyclic choices)

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

### 3.4 Actor Composition (Child Actor Component)
o ChildActorComponent: references a StarActor class, spawns as a real child Actor via existing World lifecycle
o Positioned via existing relative-transform hierarchy (Component parent/position/rotation/scale)
o No per-instance property overrides 'yet' (deferred, not forgotten)
o Cycle detection (prevent child actor to spawn infinite child actor loop)

### 3.5 Actor SubClasses
o Right clicking a .staractor in Content Browser lets you create a Sub Class
o Subclass Actors retain all of Parents logic, letting you build on top of it.
o "Create Character" content-browser template. Actor subclass pre-wired with Player, Collision and Mesh Components.

### 3.6 Asset Picker
o Single shared asset picker used everywhere instead of several separate implementations
o Play_Sound and other asset-path input fields get a picker instead of a raw text field
o Node input fields store asset references relative to the project instead of absolute source paths

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
/ Node title text size scales with zoom with no min/max clamp, grows past node bounds when fully zoomed out

### 5.2 Node Interaction
x Drag nodes by header with 16-unit grid snapping
x Single node selection, deselect on empty click
x Delete selected node(s) with Delete key
x Box/drag selection with visual rectangle
x Multi-node move together
x Ctrl+click node to add/remove from selection
x Ctrl+D to duplicate selected node(s)
x Ctrl+C and Ctrl+V to copy selected nodes and paste to cursor position

### 5.3 Pin and Wire Interaction
x Click pin to start wire, release on compatible pin to connect
x Release on empty space opens Add Node menu filtered to compatible types
x Alt+click pin to delete all links on that pin
x Ctrl+drag pin to detach and re-route; on pins with multiple links the whole bundle detaches together and reconnects to wherever the drag completes
x Alt+hover wire turns red, alt+click deletes it
o Right-click a wire: popup with "Break Link" / "Add Reroute Node"
o Double-click a wire inserts a reroute node

### 5.4 Add Node Context Menu
x Searchable, scrollable, grouped by category
x Filters to compatible types when opened from pin drag
x Node names display with spaces, scroll does not bleed to graph zoom
x Menu repositions to stay within graph area bounds
o Search accepts math symbols (+, -, *, /, %) as aliases for Add, Subtract, Multiply, Divide, Modulo

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

### 5.8 Undo / Redo
o Generic command/undo-stack abstraction, not Stargraph-specific internally
o First application: node move/create/delete, wire connect/disconnect
o Viewport and Details panel edits register against the same stack later, not a separate system

### 5.9 Editor Layout
o Stargraph Editor panels (Components, Variables, Details, Graph) use DockArea like the Level Editor instead of a fixed layout

### 5.10 Component Preview
o Viewport panel inside the Stargraph Editor showing the actor's declared components
o During Play in Graph, adds small default test environment (floor and skybox) for previewing an actor in isolation during graph editing
o Runtime pin value tooltips during Play for live graph debugging

---

## 6. Variables and Pin Types

### 6.1 Current Types
x Exec, Boolean, Integer, Float, String, Vec3, Object

### 6.2 Wildcard Type
x PinType::Wildcard - hollow diamond, light gray unresolved
x Adopts color/type of first connected typed pin
x All wildcard pins on same node resolve together
x Disconnecting resets to unresolved
x Executor resolves wildcards before evaluating

### 6.3 Node Consolidation (post-wildcard)
x Replace Float/Int/Vec3 math nodes with single wildcard Add/Subtract/Multiply/Divide

---

## 7. Built-in Nodes

### 7.1 Events
x Event_BeginPlay, Event_Tick (Delta Seconds), Event_EndPlay
x Input_Key node supporting keyboard keys via searchable key dropdown
x Mouse buttons (Left, Right, Middle) exposed through Input_Key node as boolean held
x Mouse wheel exposed through Input_Key node as axis float (-1..1)
x Mouse X and Mouse Y exposed through Input_Key node as axis floats (-1..1)
x Key input field renders searchable dropdown with all input names, matching Add Node menu UX
x Gamepad Inputs (Facebutton Up/Down/Left/Right, L/R Stick X/Y, Shoulder L/R, Trigger Axis L/R, DPad U/D/L/R, Special L/R, etc.)
/ Gamepad Inputs hardcoded to gamepad index 0, no multi-controller support yet

o Functions with inputs and outputs. Have their own graph with input and return node. (user-named, callable from other graphs)
x Custom Event nodes (Functionally, same as functions but without their own graph and without return node)
/ Call Custom Event node is exposed in the Add Node menu; only the named custom events it calls should be user-facing

### 7.2 Flow Control
x Branch, For_Loop, While_Loop, Sequence, Do_Once, Gate, Flip_Flop, Has_Authority walk their body through the shared exec-chain walker, so chains longer than one node continue and data pins propagate (8)
o Delay (requires timer system)

### 7.3 Math
x Float/Int/Vec3 Add/Subtract/Multiply/Divide
x Conversions: Float/Int/Bool to String, Float to Int, Int to Float
x Bool: Random Bool, Not, And, Or
x Float: Lerp, Truncate, Floor, Ceil, Round, Random Float, Random Float in Range
x Int: Random Int, Random Int in Range
x Wildcard: Abs, Min, Max, Clamp, Modulo
x Vec3: Length, Normalize, Dot, Cross, Distance, Scale
x Vec3: Make (X, Y, Z -> Vec3), Break (Vec3 -> X, Y, Z)
x Vec3: Normalized Distance, Reflect
x String: Concat, Length, Substring, Find, Replace, Contains
x Conversions: Vec3 to String, String to Vec3, Bool to Float, Bool to Int
x Comparisons: Wildcard_Equals, Greater Than, Less Than, Greater or Equal, Less or Equal

### 7.4 Actor Self-Reference Nodes
x Get_Actor_Location (pure, reads ctx.actor->position)
x Set_Actor_Location (exec, writes to ctx.actor->position)
x Get_Actor_Forward_Vector (pure, derived from actor rotation)
x Get/Set Actor Rotation
x Get/Set Actor Scale
x Get Actor Right Vector, Get Actor Up Vector
x Spawn Actor (class, graph path -> actor reference)

### 7.5 Utility
x Print_String (inline text field, outputs to log panel)
x Print_Warning, Print_Error (yellow/red in log)
x Get_Game_Time (returns elapsed seconds as float)
x Get_Delta_Time (returns last frame delta, usable outside Event_Tick)

### 7.6 Variables
x Get_Variable / Set_Variable via left panel drag
x Get/Set functional at runtime (needs actor context integration)
x Variable_Name internal pin hidden from node face

### 7.7 Drawing/Rendering Nodes
x Draw_Primitive (Rectangle, Circle, Line, Triangle) - Debug drawing in PIE (Don't show up in packaged games)
x Draw_Billboard - Sprite that always faces camera
x Draw_Billboard_Pro - Billboard with rotation
x Clear_Background - Change background color at runtime
x Draw_Texture - Draw texture at position with tint
x Draw_Texture_Pro - Draw textured rectangle with rotation/scale

### 7.8 Texture/Image Manipulation Nodes
x Load_Texture - Load texture from file path
x Unload_Texture - Free texture from memory
x Get_Texture_Size - Returns texture width/height
x Image_To_Texture - Create texture from image data
o Gen_Texture_Mipmaps - Generate mipmaps for texture

### 7.9 Audio Control Nodes Core (Not part of audio module)
x Play_Sound - Play a sound selectable in a drop down list. Returns Audio object property.
x Stop_Sound - Stop playing sound.
x Pause_Sound / Resume_Sound - Sound control
x Is_Sound_Playing - Check playback state
x Set_Master_Volume - Global volume control
x Load_Music - Load music stream
x Play_Music - Play music
x Set_Music_Volume - Music volume control
x Seek_Music - Seek to position in seconds

### 7.10 Camera Control Nodes
x Get_Camera_Matrix - Get view matrix for custom rendering
x Update_Camera - Manual camera mode updates
x Camera_To_Target - Set camera to look at actor

### 7.11 File I/O Nodes
x File_Exists - Check if file exists
x Directory_Exists - Check if directory exists
x Load_Text_File - Load text content from file
x Save_Text_File - Write text to file
x Get_Working_Directory - Get current directory
x Change_Directory - Change working directory

### 7.12 Utility Nodes
x Take_Screenshot - Capture screen to input file path
x Open_URL - Open URL in default browser
x Set_Window_Title - Change window title
x Get_Screen_Size - Get screen width/height
x Get_Mouse_Position - Get mouse X/Y as Vec2
x Get_FPS - Get current frames per second (1 / Deltatime)

### 7.13 Color Nodes
x Color_From_Hex - Create Color from hex value
x Color_To_Hex - Get hex value from Color
x Color_Lerp - Interpolate between two colors
x Color_Tint - Multiply color with tint
x Color_Alpha - Set color alpha

### 7.14 Collision Detection Nodes (For basic collision tracing. Not Part of any component or module.)
x Check_Collision_Point_Rec - Point in rectangle
x Check_Collision_Recs - Rectangle collision
x Check_Collision_Circles - Circle collision
x Get_Collision_Rec - Get collision rectangle

### 7.15 Input State Nodes
x Get_Key_Pressed - Get first queued key press
x Get_Char_Pressed - Unicode character pressed
x Get_Mouse_Delta - Mouse movement since last frame
x Get_Touch_Position - Multi-touch support

### 7.16 Gesture Nodes
x Is_Gesture_Detected - Check for tap/drag/swipe/pinch
x Get_Gesture_Drag_Vector - Drag direction
x Get_Gesture_Pinch_Angle - Pinch rotation
x Get_Gesture_Hold_Duration - Long press time

### 7.17 Window Management Nodes
x Toggle_Fullscreen - Switch fullscreen/windowed
x Maximize_Window / Minimize_Window
x Set_Window_Opacity - Window transparency
x Set_Window_Position - Move window on screen

### 7.18 Clipboard Nodes
x Get_Clipboard_Text - Read clipboard content
x Set_Clipboard_Text - Write to clipboard

### 7.19 Image Processing Nodes
o Image_Load - Load image from file
o Image_Resize - Resize with different algorithms
o Image_Flip_Vertical/Horizontal
o Image_Alpha_Crop - Remove transparent borders
o Image_Color_Tint / Invert / Grayscale

### 7.20 Compression/Hash Nodes
o Compute_MD5 / SHA256 - File hashing
o Compress_Data / Decompress_Data

### 7.21 Debug Drawing Nodes
x Draw_Grid - Draw reference grid (debug visualization)
x Draw_Axes - Draw XYZ axes at position (debug transform visualization)
x Draw_Ray - Visualize raycasts
x Draw_Cube - Debug cube visualization
x Draw_Circle_3D - Debug sphere visualization

---

## 8. Graph Executor
x Exec pin chain walking with safety limit
x Data pin evaluation with recursive upstream propagation
x Flow control (Branch, For_Loop, While_Loop, Sequence, Do_Once, Gate, Flip_Flop, Has_Authority) routes through ExecuteLinkedNodes, exposed as a public GraphExecutor method (7.2)
x ExecutionContext struct (Actor*, float deltaTime) threaded through all nodes
x Event_Tick writes deltaTime into Delta Seconds output pin before execution
x StargraphBehavior passes actor pointer and deltaTime from World::Tick
x Variable snapshot/restore around play so runtime writes don't persist to file
x Pin::defaultValue is a PinValue (string-like, drop-in compatible) with a cached typed interpretation (float/int/vec3); numeric round-trips (atof/atoi/sscanf) replaced with cached accessors app-wide; data propagation skips the copy (and cache invalidation) when a value is unchanged frame to frame
o Wildcard type resolution at runtime
o Async nodes (Delay, latent actions)
o Compile step validates the graph (stale references, unresolved wildcards, unreachable nodes) before anything else; nativizing to C++ is a later, separate step for hot graphs
o Nativization stays off during PIE so graphs stay live-editable while playing; it only applies to packaged builds
o Bytecode compilation as a lower-risk option if profiling shows Stargraph is an actual bottleneck

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
x Editor only box that shows warnings, errors and prints on the top left of the viewport 

---

## 10. Game Packaging
x Runtime-only CMake build target (no Editor/ code, no StargraphEditorPanel)
x Packaged executable loads a StarMap directly, runs the world
x Asset path resolution relative to packaged executable
x All .staractor and .starmap files included in package output
x Platform targets: Windows, Linux (Windows via mingw-w64 cross toolchain, verified; macOS needs osxcross, wired but unverified; Web wired via emcmake, unverified, no single-executable output; Android not implemented, needs a full APK project template)
x Content encryption at cook time (XOR, extensible to AES-128/256 later)
x Renderer choice per project (Hardware/OpenGL or Software/CPU via raylib's rlsw backend, packaged games only)
o move packaging out of the top menu and next to the play button on the viewport top bar thing

### 10.1 Packaging into a single executable (blob)
x Ship one .exe with no loose Content folder alongside it (single-file/pak packaging format, default)
x Define a pak archive format: index of [relative path, offset, size] plus concatenated cooked bytes, written by the cooker instead of a loose output folder
x Cooker writes a single .starpak file (index + blob) in place of current per-file Content/ tree
x Embed the .starpak into the compiled executable, appended after the linked binary, with a footer (magic + offset + size) found via argv[0]
x Runtime virtual filesystem: CookedFile reads directly from the embedded pak's byte range in memory
x Extract file-API-only formats (.obj, non-binary .gltf) to a per-run temp file sourced from the in-memory pak (gltf itself not implemented yet, obj path done)
x Resolve sibling files (mtl textures, gltf buffers) from the pak index by shared parent path instead of scanning a disk directory (mtl/textures done, gltf buffers pending gltf support)
x PackageRunner produces the single exe (with appended pak) as the only required output artifact
x Keep loose-folder packaging as a project setting for fast iteration/patching without a full rebuild
o Verify appended trailing data survives code signing/packaging tools on Windows and macOS before relying on this approach

### 10.2 Shipping prebuilt engine libraries per platform
o Package a game without the engine source tree on disk (mirrors UE's precompiled engine, compiles only project game code)
o Split engine into an internal library target (Core/Runtime/Modules, no GameMain.cpp) vs a thin per-platform entry point that links against it
o CI matrix builds the library for every platform/renderer combo and publishes the artifacts alongside each editor release
o Editor bundles prebuilt libs for locally-targetable platforms; missing ones fetched on demand, versioned to the running editor build
o PackageRunner compiles only GameMain.cpp plus generated module glue, then links against the matching prebuilt engine lib
o Restrict prebuilt-lib packaging to projects using only built-in modules; custom modules stay on the source-build path
o Version-tag prebuilt libs to the exact editor build/commit; refuse to link a mismatched header/lib pair to avoid silent ABI breakage
o Fall back to build-from-source packaging with a clear message when no matching prebuilt lib is available, keeping it additive not breaking

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
| Alt + Left drag | Orbit (alternative) | [x] |

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
x Display logo on top left of the editor
x Display logo on project browser screen
x Application Icon
x Packaged Game Default Icon

---

## 13. Modules

### 13.1 Audio Module
o Move existing Audio Component to Module
/ WAV import and playback
o MIDI import and parsing with Instrument Selection (SoundFont or Sample)
o SoundFont (SF2) import and synthesis
o Audio graph nodes: Play Sound, Stop Sound, Set Volume, Set Pitch
o Adaptive music layer system that allows you to play multiple tracks to be controlled individually, add loop regions and exit events for easy transitioning

### 13.2 Networking / Replication Module
x Host/Join/Disconnect nodes with password and max player slots
x Transport via libdatachannel (MPL 2.0, wraps libjuice) with DTLS encryption and SCTP channels
x Direct P2P via TCP rendezvous, then ICE/DTLS/SCTP over UDP with optional TURN fallback
x Star topology: all clients connect to host only, host-authoritative with broadcast semantics
x Signaling via direct TCP (no third-party relay), non-trickle ICE gathering before sending
x NetworkBackend interface extended with sourceConnectionId and spawn-replication surface
x Runtime actor spawn replication: host broadcasts net id, class path, transform; clients apply idempotently
x Connection lifecycle events (OnPlayerConnected/Disconnected/ConnectedToHost/JoinFailed) as Custom Events
x Security: constant-time password comparison, SIGPIPE handling, handshake thread caps, path validation
o Listen-host authority for Variables/Events/spawn, defers continuous transform replication to 13.3+
o Destroy replication for runtime-spawned actors; late-joiner catch-up broadcasts all actors (needs per-connection addressing)
o Server/Multicast nodes available but not default

### 13.3 Physics Module
o collision shapes component for overlap events
o PhysicsComponent: Velocity, gravity scale, mass, drag, linear/angular dampening, etc.

### 13.4 Rendering Module 
o Simple light sources (Point, Rectangle, Spot, Sun, Global)
o Generating shadow maps option
o Runtime shadow maps (stencil, no tracing)
o PSX vertex snapping, affine texture mapping options
o Render scale, color palette post-process, vhs pixel smear and scan lines

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

### 13.6 Skeletal Mesh and Animations
o Skeletal mesh import with .StarRig
o Animation import with .StarAnim
o Animation Preview Tab
o Animation Graph File to be assigned to Skeletal Meshes inside actors

### 13.7 UI / Widget Module (StarWidget)
o Create Widget in Content Browser
o Widget Editor Tab
o Default Widget Components Buttons, Text, Vertical Box, Horizontal Box, Progress Bar, etc.
o Create/Add/Remove widgets from viewport at runtime via graph nodes

### 13.8 Localization Module
o Project-level Languages registry
o String Table asset (Key -> text per language), text fields reference a key rather than storing translations inline
o JSON/CSV export/import of the String Table for translators
o Runtime Set Language node listing registered language options

### 13.9 StarScript Module
o Simplified text-based scripting language, alternative to Stargraph

### 13.10 VR Module (Mostly adds nodes)
o Begin_VR_Mode / End_VR_Mode - VR rendering
o Load_VR_Stereo_Config - VR configuration
o Update_VR_Config - VR device parameters

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
o Extend the same pattern to Texture, Material, Sound, and other asset types

---

## 16. Log Panel
o Logs saved to a log file on disk (Project/Saved/{date-time.log})
o Log panel text is selectable and copyable