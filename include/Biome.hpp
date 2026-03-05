#pragma once

#include <string>
#include <cstdint>

class Chunk;

enum class TreeType {
    NONE,
    OAK,
    PINE
};

class Biome {
public:
    std::string name;
    uint8_t surfaceBlock;
    uint8_t subSurfaceBlock;
    int treeDensity;
    TreeType treeType;

    Biome(std::string n, uint8_t surf, uint8_t sub, int density, TreeType t)
        : name(n), surfaceBlock(surf), subSurfaceBlock(sub), treeDensity(density), treeType(t) {}
        
    virtual ~Biome() = default;

    virtual int getTerrainHeight(float noiseVal) const;
};

class PlainsBiome : public Biome {
public:
    PlainsBiome() : Biome("Plains", 2 /*Grass*/, 3 /*Dirt*/, 4, TreeType::OAK) {}
};

class DesertBiome : public Biome {
public:
    DesertBiome() : Biome("Desert", 9 /*Sand*/, 9 /*Sand*/, 0, TreeType::NONE) {}
};

class SnowBiome : public Biome {
public:
    SnowBiome() : Biome("Tundra", 11 /*Snowy Grass*/, 3 /*Dirt*/, 2, TreeType::PINE) {} // mostly sparse
};

class TaigaBiome : public Biome {
public:
    TaigaBiome() : Biome("Taiga", 11 /*Snowy Grass*/, 3 /*Dirt*/, 12, TreeType::PINE) {} // Very dense pine trees
};

class OceanBiome : public Biome {
public:
    OceanBiome() : Biome("Deep Ocean", 9 /*Sand*/, 9 /*Sand*/, 0, TreeType::NONE) {}
    int getTerrainHeight(float noiseVal) const override;
};

class MountainBiome : public Biome {
public:
    MountainBiome() : Biome("Extreme Mountains", 4 /*Stone*/, 4 /*Stone*/, 1, TreeType::PINE) {}
    int getTerrainHeight(float noiseVal) const override;
};

class BiomeManager {
public:
    static const Biome* getBiome(float heightNoise, float tempNoise);
};
