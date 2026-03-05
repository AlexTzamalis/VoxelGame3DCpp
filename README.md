# VoxelGame3D

A modern, high-performance, and open-source 3D voxel engine built from the ground up in C++.

This project serves as a foundational voxel rendering engine focused on efficient rendering, smooth chunk management, and solid architectural principles.

## Technology Stack

![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-%23FFFFFF.svg?style=for-the-badge&logo=opengl)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white)

The engine is engineered with a lean, performant stack aimed at deep graphics control and game logic extensibility:

- **C++17:** Core engine logic, robust memory management, and multi-threading execution.
- **OpenGL 3.3+ (Core Profile):** Hardware-accelerated 3D graphics API.
- **GLFW & GLEW:** Multi-platform window state, context management, input handling, and OpenGL extension loading.
- **GLM:** Mathematics library tailored for graphics software (vectors, matrices, MVP transformations).
- **Dear ImGui:** Immediate-mode GUI library powering modern overlays, inventory layout, and customizable settings menus.
- **FastNoiseLite:** Fast, high-performance noise generation algorithm utilized for seamless procedural world and terrain generation.
- **stb_image:** Lightweight, efficient image loader for compiling sprite and block texture atlases.
- **CMake:** Cross-platform build system generation with automated dependency fetching.

## Key Technical Objectives

- **Efficient Meshing:** Minimizing draw calls and redundant vertex generation through internal-face culling and optimal block rendering.
- **Multi-Threading:** Offloading chunk generation and meshing logic from the main rendering thread to ensure stable frame rates and eliminate load stutter.
- **Resource Management:** Single texture atlas implementation and intelligent object pooling to eliminate texture thrashing and minimize runtime allocations.
- **Camera & Frustum Culling:** Smooth, 6-DoF capable 3D navigation that actively skips GPU uploads for out-of-view geometry.
- **Procedural Generation:** Utilizing seed-based noise algorithms to create dynamically expanding, unique voxel worlds on the fly.
- **Dynamic UI Systems:** Expandable, intuitive graphical systems such as a robust inventory array, quick-access hotbar, and real-time settings configurations.

## Building and Running

The project relies on CMake to manage its dependencies (such as GLFW and GLM, which are currently fetched during configuration).

For detailed build instructions, please refer to our internal documentation:

- **[Build and Run Instructions](Docs/BUILD-AND-RUN.md)**
- **[Internal Architecture & Implementation Guides](Docs/PROJECT-INSTRUCTIONS.md)**

## Contributing

As an open-source project, contributions targeting rendering optimizations, multi-threading architectures, and level-of-detail (LOD) systems are highly welcomed.

## License

This project is released as Open Source. Please see the accompanying license file **[LICENSE](LICENSE)** for precise copyright and redistribution terms.
