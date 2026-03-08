#include "world/Biome.hpp"
#include <cmath>

int Biome::getTerrainHeight(float noiseVal) const {
    float n01 = (noiseVal + 1.0f) * 0.5f;
    return static_cast<int>(n01 * 40.0f) + 65; // Much flatter base (65-105)
}

int OceanBiome::getTerrainHeight(float noiseVal) const {
    // Deep ocean! Range from 5 to 30
    float n01 = (noiseVal + 1.0f) * 0.5f;
    n01 = std::pow(n01, 1.5f);
    return static_cast<int>(n01 * 25.0f) + 10;
}

int ExtremeMountainBiome::getTerrainHeight(float noiseVal) const {
    float n01 = (noiseVal + 1.0f) * 0.5f;
    n01 = std::pow(n01, 1.15f); // Smooth peaks
    return static_cast<int>(n01 * 120.0f) + 90; // Up to 210
}

const Biome* BiomeManager::getBiome(float continentalNoise, float tempNoise) {
    static PlainsBiome plains;
    static ForestBiome forest;
    static DesertBiome desert;
    static SnowBiome tundra;
    static TaigaBiome taiga;
    static OceanBiome ocean;
    static DeepOceanBiome deepOcean;
    static JungleBiome jungle;
    static ExtremeMountainBiome mountain;

    // 1. Continental Placement (Macro Scale)
    if (continentalNoise < -0.65f) return &deepOcean;
    if (continentalNoise < -0.35f) return &ocean;
    
    // 2. Mountains (Extreme Peaks)
    if (continentalNoise > 0.82f) return &mountain;
    
    // 3. Temperature & Humidity based Biomes
    if (tempNoise > 0.6f) return &desert;
    if (tempNoise > 0.25f) return &jungle;
    if (tempNoise < -0.5f) return &tundra;
    if (tempNoise < -0.2f) return &taiga;
    
    // Forests vs Plains (Transition zone)
    if (continentalNoise > 0.2f) return &forest;
    
    return &plains; 
}
