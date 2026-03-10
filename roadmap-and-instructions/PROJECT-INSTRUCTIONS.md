# Voxel Game 3D — Project Instructions

**Stack:** C++ · OpenGL · Texture Atlas (PNG)  
**Goal:** A fully performant voxel engine with production-grade optimizations.

All markdown documentation lives under **`Docs/`**.

---

## 1. Overview

This document defines how to build the Voxel Game 3D: architecture, performance targets, and implementation order. The engine must be **performance-first**, using a single **texture atlas** (PNG), **face culling**, **multi-threading**, **chunk optimization**, **LOD**, **smart memory management**, and a **smooth 3D camera** on all axes.

---

## 2. Technology Stack

| Component        | Choice              | Notes                                      |
|-----------------|---------------------|--------------------------------------------|
| Language        | C++ (C++17 minimum) | For threading, move semantics, containers  |
| Graphics        | OpenGL (core profile) | 3.3+ recommended                         |
| Windowing/Input | GLFW or SDL2        | Cross-platform window and input            |
| OpenGL Loader   | GLAD or GLEW        | Modern function loading                    |
| Math            | GLM                 | Matrices, vectors, MVP                     |
| Textures        | **Single PNG texture atlas** | One draw call per chunk; no texture swaps |

---

## 3. Performance Pillars

The build is considered “fully performant” only when these are in place:

1. **Face culling** — No interior or back faces; only visible block faces.
2. **Multi-threading** — Chunk mesh generation and (optionally) world gen off the main thread.
3. **Chunk optimization** — Fixed-size chunks, greedy meshing (optional), and efficient data layout.
4. **Level of Detail (LOD)** — Fewer details for distant chunks (e.g. simplified meshes or lower resolution).
5. **Smart memory management** — Pooled/arena allocators, minimal allocations in hot paths, cache-friendly voxel/chunk data.
6. **Smart camera & vertices** — Frustum culling, smooth camera on X/Y/Z, and vertex data tuned for the GPU (packed attributes, single atlas).

---

## 4. Texture Atlas (PNG)

- **Single file:** e.g. `assets/textures-atlas.png`.
- **Layout:** Grid of tile textures (e.g. 16×16 or 32×32 pixels per tile).
- **Usage:**
  - Load once at startup; bind once per frame (or per chunk batch).
  - Compute UVs per block face from block ID and face direction so that **no texture switching** is needed during rendering.
- **Benefits:** One draw call per chunk (or per LOD batch), no texture thrashing, better batching.

**Implementation notes:**
- Define tile size and atlas dimensions in a config or constants.
- Map block ID → (u, v) or (col, row) in the atlas.
- Pass UVs in vertex attributes (or derive in shader from packed data if applicable).

---

## 5. Face Culling

- **Rule:** Generate a face only when the **neighbour block in that direction is air** (or non-solid).
- **Result:** No interior faces, no back faces against other blocks; large reduction in vertices and overdraw.
- **Implementation:**
  - During meshing, for each non-air voxel and each of the 6 directions, check the neighbour.
  - If neighbour is air (or transparent), emit the face (2 triangles, 4 vertices); otherwise skip.
- **OpenGL:** Enable `GL_CULL_FACE` with `GL_BACK` (or match your winding order) so the GPU culls back-facing triangles.

---

## 6. Multi-Threading

- **Main thread:** Input, camera, rendering, and **GPU upload only**.
- **Worker threads:** Chunk mesh generation (and optionally world generation).
- **Flow:**
  1. Main thread decides which chunks are needed (e.g. around camera).
  2. Request mesh (and/or terrain) for a chunk on a worker; store result in a thread-safe queue or per-chunk “pending mesh” buffer.
  3. Main thread, once per frame, consumes finished meshes and uploads to GPU (VBO/IBO) or updates buffers.
- **Critical:** Never create/destroy OpenGL objects (VAOs, VBOs, textures) from worker threads; only the main/rendering thread touches GL.
- **Goal:** No hitches when new chunks load; smooth movement and rotation.

---

## 7. Chunk Optimization

- **Chunk size:** e.g. 16×16×16 or 32×32×32 (configurable). Stored as flat array:  
  `uint8_t voxels[WIDTH * HEIGHT * DEPTH]`  
  Index: `x + y * WIDTH + z * WIDTH * HEIGHT`.
- **Chunk manager:** e.g. `std::map<glm::ivec3, Chunk*>` or a hash map keyed by chunk coordinates. Generate chunks around the camera; unload chunks beyond a distance threshold.
- **Meshing:** Only generate geometry for visible faces (see Face Culling). Optionally implement **greedy meshing (ambient occlusion / face merging)** to merge adjacent same-block faces into quads/strips to reduce vertex count.
- **Data layout:** Keep voxel data contiguous and cache-friendly; avoid per-block heap allocations in hot paths.

---

## 8. Level of Detail (LOD)

- **Idea:** Chunks farther from the camera use lower geometric (and optionally texture) detail.
- **Options:**
  - **Distance-based LOD levels:** e.g. LOD0 (full mesh), LOD1 (every 2nd voxel), LOD2 (every 4th), etc., with simplified meshes for higher LOD levels.
  - **Precomputed LOD meshes:** When a chunk is generated, build 2–3 mesh variants (full, half, quarter) and choose by distance.
  - **Smooth transitions:** Fade or morph between LOD levels to avoid popping (optional, more advanced).
- **Goal:** Fewer triangles and draw calls for distant chunks while keeping near-field detail.

---

## 9. Smart Memory Management

- **Pools:** Use object pools or arena allocators for chunk objects, mesh buffers, or temporary meshing data to avoid repeated `new`/`delete` and fragmentation.
- **Reuse:** Reuse VBOs/IBOs when chunks are unloaded (e.g. put buffer IDs back into a pool instead of freeing).
- **Containers:** Prefer `std::vector` with `reserve()` for vertex/index arrays; avoid frequent reallocations during meshing.
- **Cache:** Keep chunk and voxel data in compact, sequential layouts to improve cache usage during meshing and iteration.

---

## 10. Smart Camera & Vertices (Smooth 3D View)

- **Camera:** First-person camera with **smooth movement and rotation on all three axes (X, Y, Z)**. Use GLM for view and projection matrices (MVP).
- **Input:** Handle look (pitch/yaw), move (forward/back, strafe, up/down if applicable), and optionally roll; apply smoothing (e.g. lerp or velocity-based) to avoid jitter.
- **Frustum culling:** Do not send chunks that are entirely outside the camera frustum to the GPU. Compute chunk AABB in world space and test against the view frustum; skip rendering fully outside chunks.
- **Vertices:** Use a **packed vertex format** where possible (e.g. position + normal + UV packed into minimal attributes) to reduce bandwidth and improve cache usage. All faces use the same texture atlas so UVs are the only texture coordinates needed.

---

## 11. Implementation Phases (Suggested Order)

| Phase | Focus | Deliverable |
|-------|--------|-------------|
| **1. Foundation** | Window, OpenGL context, GLAD/GLEW, basic shaders (vertex + fragment) with color/texture support. | Window + clear screen. |
| **2. Camera** | First-person camera with GLM (view + projection), input (GLFW/SDL). Smooth movement and rotation on X, Y, Z. | Controllable 3D camera. |
| **3. Single cube** | One hardcoded cube, textured from the atlas (one tile). Rotating and visible. | One textured cube. |
| **4. Chunk data** | Chunk class: dimensions (e.g. 16³), flat `uint8_t` array, coordinate → index mapping. | Chunk with voxel storage. |
| **5. Face culling** | Meshing that only adds faces when neighbour is air. Build vertex/index buffers from chunk. | Chunk renders as blocks. |
| **6. Chunk manager** | Map of chunks by `glm::ivec3`, load/unload by camera position/distance. | Multiple chunks in world. |
| **7. Texture atlas** | Load `textures-atlas.png`, bind once; UVs from block ID. No texture switching per block. | All blocks use atlas. |
| **8. Multi-threading** | Worker threads for chunk mesh generation; main thread only uploads to GPU. | No hitches on chunk load. |
| **9. Frustum culling** | Skip chunks fully outside camera frustum. | Fewer draw calls. |
| **10. LOD** | Distance-based LOD levels for chunks; simplified meshes or lower resolution for far chunks. | Better performance at distance. |
| **11. Memory & polish** | Pools, buffer reuse, packed vertices; smooth camera and final tuning. | Stable, performant build. |

---

## 12. File & Asset Conventions

- **Docs:** All `.md` files live under **`Docs/`** (e.g. this file, `VOXEL-GAME-INSTRUCTIONS.md`).
- **Assets:** Texture atlas path: e.g. **`assets/textures-atlas.png`** (or as configured).
- **Code:** Keep rendering (OpenGL), chunk/mesh generation, and world generation in separate modules where possible to simplify threading and testing.

---

## 13. Quick Reference: Key Formulas & Conventions

- **Chunk index (1D):** `index = x + y * WIDTH + z * WIDTH * HEIGHT`
- **Block ID:** `0` = air; `1+` = block type (stone, dirt, etc.)
- **Face:** 4 vertices → 2 triangles (6 indices). Only add if neighbour in that direction is air.
- **MVP:** `gl_Position = projection * view * model * vec4(position, 1.0);`

---

## 14. Success Criteria for “Fully Performant” Build

- [ ] Single texture atlas (PNG) used for all block textures; no per-block texture switches.
- [ ] Face culling: only visible block faces generated and rendered.
- [ ] Chunk mesh generation runs on worker threads; main thread only does GPU upload and render.
- [ ] Chunks are optimized (fixed size, efficient storage, optional greedy meshing).
- [ ] LOD reduces triangle count for distant chunks.
- [ ] Memory: pools/reuse, minimal allocations in hot paths, cache-friendly data.
- [ ] Camera: smooth movement and rotation on X, Y, Z; frustum culling excludes off-screen chunks.
- [ ] No visible stutter when new chunks load; smooth 3D navigation.

Use this document as the single source of truth to start and iterate on the Voxel Game 3D codebase. For high-level engine phases and “pro tips,” see **`Docs/VOXEL-GAME-INSTRUCTIONS.md`**.
