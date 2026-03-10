# Project Analysis & Performance Audit

## 1. Deep Scan Performance Analysis

### Current Strengths
- **Multi-Draw Indirect (MDI)**: Excellent use of `glMultiDrawElementsIndirect` to minimize draw calls.
- **Worker Thread Pool**: Offloads heavy noise generation and meshing from the main thread successfully.
- **Greedy Meshing**: Significant reduction in triangle count for solid blocks.
- **LOD System**: Distance-based LOD rendering for a larger perceived world.
- **Memory Efficiency**: Voxels are only allocated if they aren't air, saving ~90% RAM in sparse areas.

### Potential Bottlenecks & Missing Optimizations
1. **Fragment Shader Complexity (High Priority)**:
   - **Volumetric Sunrays**: Currently implemented per-fragment with 8 samples. This is extremely heavy.
   - **PCSS (Percentage Closer Soft Shadows)**: 16 Poisson Disk samples + blocker search is expensive.
   - **Strategy**: Move volumetric rays to a lower-resolution pass or use temporal accumulation. Simplify shadows for medium/low settings.

2. **Meshing & Multi-Threading**:
   - **Greedy Meshing Limitations**: Currently excludes water and transparent blocks to allow for specific effects (waves). While beneficial for visuals, it increases draw call complexity for those layers.
   - **VRAM Fragmentation**: `defragmentVRAM` is present but triggered only when the buffer is full. A more proactive slot-management system could reduce the need for full defrags.

3. **World Serialization (I/O)**:
   - **Uncompressed Binary**: Chunks are saved as raw byte arrays. For large worlds, this will lead to massive disk usage and slower I/O.
   - **Strategy**: Implement LZ4 or Zstandard compression. These are fast and perfect for repetitive voxel data.

4. **Frustum Culling**:
   - Currently uses a basic AABB-frustum test. For highly vertical worlds, many invisible chunks might still be processed by the MDI rebuild logic.

---

## 2. Future Strategy & Questions

### Directory Strategy (AppData)
For Windows, it is best practice to store game data in `%AppData%`.
- **Proposed Path**: `C:\Users\<User>\AppData\Roaming\VoxelGame3D`
- **Why?**: This keeps the user's `C:\Code\` or `C:\Program Files\` clean and allows for easy backup of save files.
- **Implementation**: I can help you implement a `Config` check that dynamically resolves this path using `SHGetKnownFolderPath` or `getenv("APPDATA")`.

### Game Installer & Dependencies
- **C++ Components**: You usually need the **Microsoft Visual C++ Redistributable**. If you compile with static linking (`/MT` in MSVC or the static flags you have in `CMakeLists.txt` for MinGW), you can often avoid this, but it increases executable size.
- **Bundling Assets**: An installer will package the `.exe`, `assets/`, `shaders/`, and any required `.dll` files (like `glew32.dll` if not statically linked).

### Recommended Installer Tools
1. **Inno Setup (Recommended)**: Free, extremely simple to script, and very professional. It's the gold standard for indie C++ games on Windows.
2. **CPack (CMake Built-in)**: Since you already use CMake, CPack can generate `.zip`, `NSIS`, or even `WIX` installers directly from your build configuration.
3. **NSIS**: More powerful but has a steeper learning curve than Inno Setup.

---

## 3. Recommended Roadmap
1. **Short Term**: Optimize `basic.frag` by adding toggles or lower-quality paths for Volumetric Rays.
2. **Medium Term**: Implement Chunk Compression and move Save Data to `%AppData%`.
3. **Long Term**: Integrate CPack or Inno Setup for distribution.
