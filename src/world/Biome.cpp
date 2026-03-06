#include "world/Biome.hpp"
#include <cmath>

int Biome::getTerrainHeight(float noiseVal) const {
    float n01 = (noiseVal + 1.0f) * 0.5f;
    n01 = std::pow(n01, 1.25f);
    return static_cast<int>(n01 * 90.0f) + 40; // 40 to 130
}

int OceanBiome::getTerrainHeight(float noiseVal) const {
    // Deep ocean! Range from 5 to 30
    float n01 = (noiseVal + 1.0f) * 0.5f;
    n01 = std::pow(n01, 1.5f);
    return static_cast<int>(n01 * 25.0f) + 10;
}

int MountainBiome::getTerrainHeight(float noiseVal) const {
    // Extreme mountains!
    float n01 = (noiseVal + 1.0f) * 0.5f;
    n01 = std::pow(n01, 0.9f); // sharper peaks
    return static_cast<int>(n01 * 180.0f) + 50; // up to 230!
}

const Biome* BiomeManager::getBiome(float heightNoise, float tempNoise) {
    static PlainsBiome plains;
    static DesertBiome desert;
    static SnowBiome snow;
    static TaigaBiome taiga;
    static OceanBiome ocean;
    static MountainBiome mountain;

    if (heightNoise < -0.4f) {
        return &ocean;
    }
    else if (heightNoise > 0.6f) {
        return &mountain;
    }
    else {
        if (tempNoise > 0.4f) {
            return &desert;
        } else if (tempNoise < -0.4f) {
            return &taiga;
        } else if (tempNoise < -0.2f) {
            return &snow;
        } else {
            return &plains; // Default
        }
    }
}
