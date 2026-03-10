# Build and Run — Voxel Game 3D

## Requirements

- CMake 3.16+
- C++17 compiler (MSVC, GCC, Clang)
- OpenGL 3.3+ drivers

## Build

From the project root:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Or open the project in **CLion** (or another IDE); it will configure and build using the root `CMakeLists.txt`.

Dependencies (GLFW, GLEW, GLM, stb) are fetched automatically via CMake `FetchContent`.

## Run

- **From terminal:** Run the executable from the **build directory** so it finds `shaders/` and `assets/` (they are copied there by CMake).
  ```bash
  cd build
  ./VoxelGame3D   # or VoxelGame3D.exe on Windows
  ```
- **From CLion:** Set the run configuration **Working directory** to your build directory (e.g. `$ProjectFileDir$/cmake-build-debug`) so the same paths work.

## Controls

- **WASD** — Move
- **Space / Left Shift** — Up / Down
- **Mouse** — Look (pitch/yaw)
- **Escape** — Close window

## Assets

Place `textures-atlas.png` in `assets/`. The engine expects a 16×16 grid of tiles (one tile per block type); tile size is configurable in code (see `atlasTileSize` in the shader).
