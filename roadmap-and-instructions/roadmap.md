ass# VoxelGame3D Status & Road Map

## What We Have Done (Working) ✅

### 1. Rendering Engine Core

- **OpenGL 3.3 Backbone:** Modern shader-based pipeline using VBOs, VAOs, and a clustered Chunk Render logic.
- **Texture Atlas Packing:** Procedurally loads standalone `.png` files from a Texture Pack `.zip` archive, merging strictly square block images automatically into a 3D Texture Array (for hardware-seamless filtering), while preserving GUI/Skin/Sky textures into 2D standalone slots.
- **Basic Texturing and Transparency:** Fully maps UVs out of the array with blending enabled for water/glass blocks.
- **Volumetric 3D Clouds:** Custom raymarched FBM clouds using real volumetric step algorithms with sunlight scattering and self-shadowing rather than 2D sprites.
- **Day / Night Cycle:** Configurable celestial angle tracking with dramatic Sky Color (Rayleigh/Mie approximation) atmospheric rendering based on sun/moon elevation.

### 2. World Generation & Physics

- **Infinite Terrain Pipeline Structure:** Chunks (16x256x16) generate using a multi-threaded asynchronous Queue Manager structure `ChunkManager`. `FastNoiseLite` builds varied heightmaps.
- **Physics & Raycast Collisions:** Solid AABB Axis-separated collision prevents clipping through geometry (now accurately pinned from feet-up). First-person and Third-person camera block intersection checks push cameras away from obstructed walls.
- **Chunk Hysteresis Optimization:** Solved border chunk spasms by instituting a distance-squared buffer preventing back-and-forth chunk load stuttering.

### 3. The Player Model System

- **Minecraft "Steve" Skin UV Parser:** Custom code maps standard 64x64 pixel layout skins mathematically onto 6 floating-point boxes (Head, Body, Arms, Legs).
- **Smooth Input Momentum:** Decoupled input logic from strict rigid increments and added exponential sliding factors (Hysteresis) to fly-mode velocities. Walk swinging speed caps realistically so limbs don't flail at Mach speeds.
- **Perspectives (F5 Toggle):** Functional toggling states for First-Person View, Third-Person Backwards View, and Third-Person Front View mode.
- **First-Person Immersion:** Camera rests in front of the center "Eyes", rendering swinging arms and visible chest/feet when looking down.

---

## What Needs Fixing / Improving As Of Now 🛠️

### 1. The Rendering & Textures

- **Shadow Buffer Clipping:** Shadow mapping relies on an untested or naive Orthographic projection `lightSpaceMatrix` that likely cuts off edges or breaks entirely when looking straight down or depending heavily on directional angles. Needs tighter dynamic Cascaded Shadow Maps (CSM).
- **Chunk Generation Bottlenecks:** Chunks queueing on the edge of distance spawn synchronously if too many are forced through. Needs better prioritization metric (e.g., raytraced cone visibility generation).
- **Block Textures Orientation:** Need standardized rotational states (e.g. Logs facing horizontally; Stairs connecting logically based on placement orientation). Right now, most blocks just span standard repetitive identical XYZ mappings.

### 2. Gameplay & Interactivity Defaults

- **Lack of Block Outliner / Context Focus:** We need a solid thin black bounding box rendering wireframe highlighting precisely which physical voxel face the raycast is currently pointing at so players know what they are about to mine.
- **Lighting Model:** A lot of the ambient light does not care about indoor caves or caves getting dark. The game is rendering uniform "Sky Light". We need full Voxel Block/Sun Light Flood-fill BFS calculation so mineshafts become completely pitch black!
- **Mouse Input Drift / Sensitivities:** Handling UI layer interactions vs raw uncaptured mouse modes.

### 3. The Block Placer Logic

- **No Valid Inventory to Logic Bridge:** The current system uses hotkeys (1-9) to swap raw block IDs but is completely missing the full scale player inventory `GUI` syncing down to what the engine's `place_block()` code grabs.
- **Entity Placement & Block Reach:** Ensure Raycast limits the maximum interaction range accurately to `5.0f` blocks in Survival!

---

## Future Goals (The Roadmap) 🚀

### 1. Robust Engine Additions

- **Multiplayer Threading / Server Split:** Detach `World` completely into a Headless architecture. The client `Main.cpp` only sends `Input` packets and visually interpolates `Voxel` packets back.
- **Audio Engine:** Integration of a spatial audio library (`miniaudio` or `FMOD`) for footsteps, block breaks, ambient wind/caves (with parameterised dampening inside solid blocks).
- **Water & Fluid Mechanics:** Blocks updates via tick scheduling. Water needs an active flow queue to spread dynamically. Transparent overlay UI blue screen when the player sinks underwater with fog modifiers.

### 2. Game World Entities

- **Entity System:** Add a proper Entity tracking manager class (rather than just drawing the current user Player class). Allow spawning Pigs, Zombies, etc., running on an A\* pathfinding navmesh built across voxel edges.
- **Dropped Items:** Floating 3D miniature spinning items over blocks when broken, running physics logic.

### 3. Polish & Progression (The "Idle-Game" Element vs Sandbox)

- **Custom Config Menus:** Connect the ImGui pause-menu "Settings" directly to an INI or JSON configuration serialization so changes persist permanently over restarts.
- **Biomes & Complex Flora:** Add trees, tall grass, 3D cross-mesh generation (`PlantRenderer` models), caves, and multiple overlapping Noise maps for varying Temperature/Humidity zones.
- **Dynamic Fog:** Realistic scattering fog that ties into the `cloud.frag` and masks the harsh clipping plane cutoffs at the edge of the world generation border perfectly.
