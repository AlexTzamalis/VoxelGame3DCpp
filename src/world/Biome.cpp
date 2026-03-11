#include "world/Biome.hpp"
#include <cmath>

#include <glm/glm.hpp>

#define SEA_LEVEL 63

int BiomeManager::getGlobalHeight(float continental, float erosion) {
    // Continentalness Spline Lookup table
    struct SplinePoint { float n; float h; };
    static const SplinePoint points[] = {
        {-1.0f, 15.0f},  // Deep Ocean
        {-0.6f, 35.0f},  // Ocean
        {-0.25f, 62.0f}, // Coastline
        { 0.1f, 75.0f},  // Plains
        { 0.4f, 130.0f}, // Hills / Mountains
        { 0.8f, 210.0f}, // High Peaks
        { 1.0f, 255.0f}  // Extreme Peaks
    };

    float baseH = points[0].h;
    for (int i = 0; i < 6; ++i) {
        if (continental >= points[i].n && continental <= points[i+1].n) {
            float t = (continental - points[i].n) / (points[i+1].n - points[i].n);
            baseH = points[i].h + t * (points[i+1].h - points[i].h);
            break;
        }
    }
    if (continental > points[6].n) baseH = points[6].h;

    // Erosion Logic: High erosion (positive) = flatter terrain towards sea level
    // Low erosion (negative) = more extreme heights
    if (erosion > 0.4f) {
        // Flatten towards plains height if eroded heavily
        float t = (erosion - 0.4f) * 1.5f;
        if (t > 1.0f) t = 1.0f;
        baseH = baseH * (1.0f - t) + 70.0f * t;
    }
    
    return static_cast<int>(baseH);
}

const Biome* BiomeManager::getBiome(float continental, float temp, float humidity) {
    static PlainsBiome plains;
    static ForestBiome forest;
    static DesertBiome desert;
    static SnowBiome tundra;
    static TaigaBiome taiga;
    static OceanBiome ocean;
    static DeepOceanBiome deepOcean;
    static JungleBiome jungle;
    static ExtremeMountainBiome mountain;
    static SavannaBiome savanna;
    static SwampBiome swamp;

    // 1. Continental Placement (Macro Scale)
    if (continental < -0.55f) return &deepOcean;
    if (continental < -0.2f) return &ocean;
    
    // 2. Mountains (Extreme Peaks) - Higher threshold for peaks
    if (continental > 0.65f) return &mountain;
    
    // 3. Climate Matrix (Temp vs Humidity)
    if (temp < -0.4f) {
        if (humidity > 0.1f) return &taiga;
        return &tundra;
    }
    
    if (temp > 0.5f) {
        if (humidity < -0.3f) return &desert;
        if (humidity > 0.3f) return &jungle;
        return &savanna;
    }

    // Temperate Zone
    if (humidity > 0.45f) return &swamp;
    if (humidity > 0.1f) return &forest;
    
    return &plains; 
}
