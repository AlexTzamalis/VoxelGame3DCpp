# Release Notes - v0.1.0-alpha

## 🚀 Welcome to the first release of the VoxelGame3D Engine!

VoxelGame3D is a standalone, high-performance sandbox voxel engine built from the ground up using **C++17** and **OpenGL 3.3+**. This initial alpha release (v0.1.0) establishes the core rendering pipeline, procedural world generation, and atmospheric systems required for a modern voxel experience.

---

### ✨ Key Features in v0.1.0-alpha

#### 🌍 Advanced World Generation & Biomes

- **Continental Noise System**: The world is divided into large landmasses, oceans, and specific mountain ranges, preventing "noise walls" and creating realistic geography.
- **Diverse Biome Suite**:
  - **Plains**: Flat, grassy expanses for easy building.
  - **Forests & Jungles**: High-density vegetation with distinct tree types.
  - **Extreme Mountains**: Rare, vertical peaks reaching high altitudes.
  - **Oceans & Deep Oceans**: Vast aquatic environments with volumetric depth.
  - **Tundra & Taiga**: Snowy plains and dense pine forests.

#### ☁️ Cinematic Atmospheric Scattering (Photon-Inspired)

- **Ozone & Rayleigh Scattering**: A physically-accurate sky model that replicates the deep blues of noon and the rich oranges/purples of twilight.
- **Volumetric Clouds**: Multi-layered noise clouds at 150m altitude featuring **Henyey-Greenstein** forward scattering (Silver Linings).
- **Physical Water (Beer-Lambert)**: Water now absorbs light based on depth, creating a realistic murky teal-blue volume rather than flat darkness.

#### ⚡ Performance & Engine Architecture

- **MDI (Multi-Draw Indirect)**: Utilizing modern OpenGL features to render thousands of chunks in a single GPU command.
- **Radial LOD (Level of Detail)**: A circular horizon system that renders terrain up to **4km away** (256 chunks) with optimized low-poly geometry.
- **Multi-Threaded Pipeline**: Chunk generation, noise sampling, and meshing are entirely offloaded to background worker threads, eliminating frame stutters.
- **Frustum Culling**: Accelerated AABB checks ensure only visible geometry reaches the GPU.

#### 🎮 Player Systems & UI

- **Physics-Based Movement**: Smooth acceleration/deceleration in Creative/Spectator modes and standard voxel-feel movement in Survival.
- **Inventory & Hotbar**: Fully functional 36-slot inventory and 9-slot hotbar integrated with block placement logic.
- **Real-Time Settings**: In-game menu for adjusting FOV, Render Distance, Shadow Intensity, and Shader effects on the fly.

---

### 🛠️ Technical Stack

- **Graphics**: OpenGL 3.3+ Core Profile
- **Math**: GLM (OpenGL Mathematics)
- **Windowing/Input**: GLFW & GLEW
- **Noise**: FastNoiseLite (OpenSimplex2 FBm)
- **UI**: Dear ImGui
- **Build System**: CMake

### 📝 Project Vision

This release marks the transition from a prototype to a foundational engine. Future updates will focus on entity systems, complex lighting (Voxel GI), and enhanced modding capabilities.

---

**VoxelGame3D - A Standalone Voxel Engine**
_Released: March 8, 2026_
