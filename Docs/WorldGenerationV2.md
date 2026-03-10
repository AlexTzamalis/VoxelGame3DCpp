# World Generation V2: The "Hytale Method"

This document serves as technical documentation on how the WorldGen V2 engine operates. It moves away from simplistic heightmaps (e.g. `Perlin Noise`) into advanced multi-layered deterministic 3D procedural generation.

## 1. Noise Remapping (The "Secret Sauce")

Instead of directly using a raw mapping grid with Perlin noise, we use **OpenSimplex2** combined with `Curve Mapping` (termed as noise remapping). We map out different terrains like valleys and peaks via different noise distributions (smooth valleys via constrained mappings, jagged peaks via multiplied outputs).
In `Chunk.cpp`, we utilize `std::sin()` as a step remap layer: `float stepHytaleRemap = std::sin((float)gY * 0.3f) * 2.5f;`. This enforces terracing on our landscapes, giving us "stepped" voxel cliffs!

## 2. 3D Density Fields

To achieve overhangs, arches, and floating islands, a simplistic `2D Heightmap` (`y = noise(x,z)`) does not work. We utilize a **3D Subtraction Density Matrix**:
`density = [Base 2D Height] - [Current Y Level] + [OpenSimplex2 3D Noise] + [Curve Mapped Steps]`
Any block where `density > 0` resolves as a solid voxel type. This fundamentally enables beautiful "Swiss Cheese" logic where mountains can generate floating fragments right over ravines.

## 3. Shared Flora Clustering

Instead of assigning a random `<x>%` uniform chance to trees or ferns across generic chunks, we integrated a global **Cluster Noise Mapping** field.
`float plantCluster = clusterNoise.GetNoise(gX, gZ);`
We run logic that multiplies or divides spawn generation metrics based on this map. High clustering values naturally generate dense, concentrated "Groves", while low values automatically yield open "Clearings," bringing the environment to a realistically intentional feel.

## 4. Performance Optimizations in V2

Because 3D Volumetric Evaluation is exceptionally laggy when executed tens of thousands of times per chunk rendering call, we incorporated the `Fast Path Exit` logic.
If `abs(gY - h) > 30`, we _skip_ evaluating the 3D Simplex field, as the voxel is either too high in the sky (confirmed Air), or completely buried under solid dirt (confirmed Subsurface). The same fast path logic is applied to cave formations, only evaluating `caveNoise3D` under deep conditions (`gY < -5`).

## Future Scalability: LOD & Distant Horizons

As chunk processing depth limits normal rendering views, the addition of a Fake Rendering LOD system (Level of Detail), highly similar to the **Distant Horizons** mod, would be highly beneficial as a long-term roadmap goal. We can drastically bypass meshing limits by pushing low-resolution meshes generated strictly from our 2D Base Height noise function.
