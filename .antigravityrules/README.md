# VoxelGame3D — Project Rules & Conventions

## Directory Structure

```
src/
├── core/           # Application entry, config, windowing, platform utilities
├── renderer/       # OpenGL wrappers, shaders, textures, camera
├── world/          # Chunk system, world management, biomes, noise
└── game/           # Player logic, inventory, gameplay systems
```

**Rule:** Every new file MUST be placed in the appropriate domain folder.
If a file genuinely spans multiple domains, place it in the domain it _depends on the least_.

## Include Conventions

- **Always use module-qualified includes:** `#include "renderer/Camera.hpp"`, not `#include "Camera.hpp"`
- The CMake include root is `src/`, so all quoted includes are relative to `src/`.
- System/library includes use angle brackets: `#include <glm/glm.hpp>`
- Third-party includes (stb, ImGui) use quoted includes without domain prefix.

## Naming Conventions

| Element      | Convention       | Example                |
| ------------ | ---------------- | ---------------------- |
| Files        | PascalCase       | `ChunkManager.hpp`     |
| Classes      | PascalCase       | `ChunkManager`         |
| Functions    | camelCase        | `getVoxelGlobal()`     |
| Variables    | camelCase        | `currentSeed`          |
| Private vars | trailing `_`     | `voxels_`, `position_` |
| Constants    | UPPER_SNAKE_CASE | `CHUNK_SIZE`           |
| Enums        | PascalCase       | `GameMode::CREATIVE`   |

## Adding New Files

1. Choose the correct domain folder
2. Create both `.hpp` and `.cpp` (co-located)
3. Use `#pragma once` in all headers
4. Add the `.cpp` to `CMakeLists.txt` under the correct domain section
5. Use module-qualified includes in your new files

## Build System

- **Generator:** CMake 3.16+, C++17
- **Dependencies:** GLFW 3.4, GLM 1.0.1, GLEW 2.2.0, stb, ImGui (vendored in `thirdparty/`)
- **Build:** `cmake --build build`
- **Include root:** `${CMAKE_CURRENT_SOURCE_DIR}/src`

## Code Style

- C++17 standard
- Prefer `std::unique_ptr` for ownership
- Use `#pragma once` over include guards
- Keep headers minimal — forward-declare when possible
- Order includes: own header first, then project headers (alphabetical), then system headers
