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

class ForestBiome : public Biome {
public:
    ForestBiome() : Biome("Forest", 2, 3, 22, TreeType::OAK) {}
};

class JungleBiome : public Biome {
public:
    JungleBiome() : Biome("Jungle", 2, 3, 40, TreeType::OAK) {}
};

class DeepOceanBiome : public Biome {
public:
    DeepOceanBiome() : Biome("Deep Ocean", 9, 9, 0, TreeType::NONE) {}
    int getTerrainHeight(float noiseVal) const override {
        float n01 = (noiseVal + 1.0f) * 0.5f;
        return static_cast<int>(n01 * 15.0f) + 12;
    }
};

class ExtremeMountainBiome : public Biome {
public:
    ExtremeMountainBiome() : Biome("Extreme Mountains", 4, 4, 1, TreeType::PINE) {}
    int getTerrainHeight(float noiseVal) const override;
};

class BiomeManager {
public:
    static const Biome* getBiome(float heightNoise, float tempNoise);
};
