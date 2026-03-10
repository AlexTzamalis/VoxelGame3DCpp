# VoxelGame3D

A modern, high-performance, and open-source 3D voxel engine built from the ground up in C++.

This project is a foundational voxel rendering engine focused on efficient rendering, smooth chunk management, and solid architectural principles.

## 🚀 Quick Start

Ensure you have a C++17 compiler and CMake 3.16+ installed.

```bash
# Clone the repository
git clone https://github.com/AlexTzamalis/VoxelGame3DCpp.git
cd VoxelGame3DCpp

# Configure and Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run (Windows)
cd build/Release && ./VoxelGame3D.exe
# Run (Linux/macOS)
cd build && ./VoxelGame3D
```

## 🛠 Technology Stack

![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-%23FFFFFF.svg?style=for-the-badge&logo=opengl)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white)

- **C++17:** Core engine logic and multi-threading.
- **OpenGL 3.3+ (Core Profile):** Hardware-accelerated 3D graphics.
- **MDI (Multi-Draw Indirect):** High-efficiency rendering pipeline.
- **Dear ImGui:** Professional UI overlays and settings menus.
- **FastNoiseLite:** Procedural world generation.
- **CMake:** Automated dependency management (GLFW, GLM, GLEW, stb).

## 💻 IDE Setup (CLion)

If you are using CLion, follow these steps to ensure the game finds its shaders and assets:

1. **Open Project**: Point CLion to the root `CMakeLists.txt`.
2. **Set Working Directory**: 
   - Go to `Run` -> `Edit Configurations`.
   - Select the `VoxelGame3D` target.
   - Set **Working directory** to the folder containing the executable (usually `$ProjectFileDir$/cmake-build-debug` or `$ProjectFileDir$/cmake-build-release`).
   - *Note*: Shaders and assets are automatically copied to the build directory during the build process.

## ⚠️ Requirements & Compatibility

- **OS**: Windows (tested), Linux, macOS.
- **Graphics**: GPU supporting **OpenGL 3.3 Core Profile** or higher.
- **Compiler**: Any C++17 compatible compiler (MSVC 2019+, GCC 9+, Clang 10+).

## 🔍 Troubleshooting

- **Black Screen / No Textures**: Ensure `assets/` folder is present in your working directory.
- **Missing Shaders Error**: The program expects a `shaders/` directory in the same folder as the `.exe`. CMake handles this copy automatically on build.
- **Low Performance**: Check `Settings` in-game and ensure "Advanced Shaders" are balanced for your hardware. See [Performance Analysis](Docs/PerformanceAnalysis.md) for deeper details.

## 📚 Documentation

Detailed guides are located in the `Docs/` directory:
- [Build and Run Instructions](Docs/BUILD-AND-RUN.md)
- [Performance & Optimization Audit](Docs/PerformanceAnalysis.md)
- [Internal Architecture](Docs/PROJECT-INSTRUCTIONS.md)

## 📜 License

This project is released under the **MIT License**. See [LICENSE](LICENSE) for details.
