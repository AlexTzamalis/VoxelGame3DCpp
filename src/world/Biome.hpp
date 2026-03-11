#pragma once

#include <string>
#include <cstdint>

class Chunk;

enum class TreeType {
    NONE,
    OAK,
    PINE,
    BIRCH,
    JUNGLE,
    ACACIA
};

class Biome {
public:
    std::string name;
    uint8_t surfaceBlock;
    uint8_t subSurfaceBlock;
    int treeDensity;
    TreeType treeType;
    float densityThreshold; // For 3D features

    Biome(std::string n, uint8_t surf, uint8_t sub, int density, TreeType t, float dt = 0.5f)
        : name(n), surfaceBlock(surf), subSurfaceBlock(sub), treeDensity(density), treeType(t), densityThreshold(dt) {}
        
    virtual ~Biome() = default;
};

class PlainsBiome : public Biome {
public:
    PlainsBiome() : Biome("Plains", 2, 3, 20, TreeType::OAK) {}
};

class DesertBiome : public Biome {
public:
    DesertBiome() : Biome("Desert", 9, 9, 0, TreeType::NONE) {}
};

class SnowBiome : public Biome {
public:
    SnowBiome() : Biome("Tundra", 11, 3, 10, TreeType::PINE) {}
};

class TaigaBiome : public Biome {
public:
    TaigaBiome() : Biome("Taiga", 11, 3, 60, TreeType::PINE) {}
};

class OceanBiome : public Biome {
public:
    OceanBiome() : Biome("Ocean", 9, 9, 0, TreeType::NONE) {}
};

class ForestBiome : public Biome {
public:
    ForestBiome() : Biome("Forest", 2, 3, 120, TreeType::OAK) {}
};

class JungleBiome : public Biome {
public:
    JungleBiome() : Biome("Jungle", 28, 3, 250, TreeType::JUNGLE) {}
};

class DeepOceanBiome : public Biome {
public:
    DeepOceanBiome() : Biome("Deep Ocean", 27 /*Gravel*/, 3, 0, TreeType::NONE) {}
};

class ExtremeMountainBiome : public Biome {
public:
    ExtremeMountainBiome() : Biome("Extreme Mountains", 22, 24, 2, TreeType::PINE, 0.2f) {}
};

class SavannaBiome : public Biome {
public:
    SavannaBiome() : Biome("Savanna", 30 /*Coarse Dirt*/, 3, 40, TreeType::ACACIA) {}
};

class SwampBiome : public Biome {
public:
    SwampBiome() : Biome("Swamp", 30, 3, 45, TreeType::OAK) {}
};

class BiomeManager {
public:
    static const Biome* getBiome(float continentalNoise, float tempNoise, float humidityNoise);
    static int getGlobalHeight(float continental, float erosion);
};
