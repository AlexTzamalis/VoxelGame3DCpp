#include "world/Chunk.hpp"
#include "renderer/TextureAtlas.hpp"
#include "core/Config.hpp"
#include "world/Biome.hpp"
#include <GL/glew.h>
#include <cmath>

const int DIRS[6][3] = {
    { 1,  0,  0}, // +X
    {-1,  0,  0}, // -X
    { 0,  1,  0}, // +Y
    { 0, -1,  0}, // -Y
    { 0,  0,  1}, // +Z
    { 0,  0, -1}  // -Z
};

const float FACE_VERTICES[6][4][8] = {
    // +X (Right)
    {
        { 1.0f,  1.0f,  1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f }, // top-left
        { 1.0f,  0.0f,  1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f }, // bot-left
        { 1.0f,  0.0f,  0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f }, // bot-right
        { 1.0f,  1.0f,  0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f }  // top-right
    },
    // -X (Left)
    {
        { 0.0f,  1.0f,  0.0f,  -1.0f, 0.0f, 0.0f,   0.0f, 1.0f },
        { 0.0f,  0.0f,  0.0f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f },
        { 0.0f,  0.0f,  1.0f,  -1.0f, 0.0f, 0.0f,   1.0f, 0.0f },
        { 0.0f,  1.0f,  1.0f,  -1.0f, 0.0f, 0.0f,   1.0f, 1.0f }
    },
    // +Y (Top)
    {
        { 0.0f,  1.0f,  0.0f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f },
        { 0.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f },
        { 1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f },
        { 1.0f,  1.0f,  0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f }
    },
    // -Y (Bottom)
    {
        { 0.0f,  0.0f,  1.0f,   0.0f,-1.0f, 0.0f,   0.0f, 1.0f },
        { 0.0f,  0.0f,  0.0f,   0.0f,-1.0f, 0.0f,   0.0f, 0.0f },
        { 1.0f,  0.0f,  0.0f,   0.0f,-1.0f, 0.0f,   1.0f, 0.0f },
        { 1.0f,  0.0f,  1.0f,   0.0f,-1.0f, 0.0f,   1.0f, 1.0f }
    },
    // +Z (Front)
    {
        { 0.0f,  1.0f,  1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f },
        { 0.0f,  0.0f,  1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f },
        { 1.0f,  0.0f,  1.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f },
        { 1.0f,  1.0f,  1.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f }
    },
    // -Z (Back)
    {
        { 1.0f,  1.0f,  0.0f,   0.0f, 0.0f,-1.0f,   0.0f, 1.0f },
        { 1.0f,  0.0f,  0.0f,   0.0f, 0.0f,-1.0f,   0.0f, 0.0f },
        { 0.0f,  0.0f,  0.0f,   0.0f, 0.0f,-1.0f,   1.0f, 0.0f },
        { 0.0f,  1.0f,  0.0f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f }
    }
};

Chunk::Chunk(glm::ivec3 position) : position_(position) {
    // Memory is dynamically allocated on demand in setVoxel to save 90%+ RAM!
}

Chunk::~Chunk() {
}

int Chunk::getIndex(int x, int y, int z) const {
    return (x + 1) + (y + 1) * PADDED_SIZE + (z + 1) * PADDED_SIZE * PADDED_SIZE;
}

uint8_t Chunk::getVoxel(int x, int y, int z) const {
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) {
        return 0; // Air outside
    }
    if (voxels_.empty()) return 1; // Default to Base Air if unallocated
    return voxels_[getIndex(x, y, z)];
}

void Chunk::setVoxel(int x, int y, int z, uint8_t type) {
    if (x >= -1 && x <= CHUNK_SIZE && y >= -1 && y <= CHUNK_SIZE && z >= -1 && z <= CHUNK_SIZE) {
        if (voxels_.empty()) {
            if (type <= 1) return; // Never allocate 5.8KB memory just for Air blocks
            voxels_.resize(PADDED_SIZE * PADDED_SIZE * PADDED_SIZE, 1);
        }
        voxels_[getIndex(x, y, z)] = type;
    }
}

void Chunk::generateTerrain(FastNoiseLite& heightNoise, FastNoiseLite& caveNoise) {
    if (Config::currentWorldType == 3) {
        generateTerrainV2();
        return;
    }
    if (Config::currentWorldType == 1) { // Flat World (1 Stone, 2 Dirt, 1 Grass)
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int globalY = position_.y * CHUNK_SIZE + y;
                for (int z = -1; z <= CHUNK_SIZE; ++z) {
                    if (globalY == -3) setVoxel(x, y, z, 4); // Stone
                    else if (globalY == -2 || globalY == -1) setVoxel(x, y, z, 3); // Dirt
                    else if (globalY == 0) setVoxel(x, y, z, 2); // Grass
                    else setVoxel(x, y, z, 1); // Air
                }
            }
        }
        return;
    }
    
    if (Config::currentWorldType == 2) { // Skyblock (Central Island)
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            int globalX = position_.x * CHUNK_SIZE + x;
            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int globalY = position_.y * CHUNK_SIZE + y;
                for (int z = -1; z <= CHUNK_SIZE; ++z) {
                    int globalZ = position_.z * CHUNK_SIZE + z;
                    if (globalX >= -3 && globalX <= 3 && globalZ >= -3 && globalZ <= 3) {
                        if (globalY == 0) setVoxel(x, y, z, 2); // Grass
                        else if (globalY >= -2 && globalY < 0) setVoxel(x, y, z, 3); // Dirt
                        else setVoxel(x, y, z, 1); // Air
                    } else {
                        setVoxel(x, y, z, 1); // Air
                    }
                }
            }
        }
        return;
    }

    auto getTreeAt = [&](int tx, int tz, int& outHeight, const Biome*& outBiome) -> bool {
        uint32_t tMixX = (uint32_t)std::abs(tx + Config::currentSeed * 7);
        uint32_t tMixZ = (uint32_t)std::abs(tz + Config::currentSeed * 13);
        uint32_t tHash = (tMixX * 374761393U ^ tMixZ * 668265263U);
        tHash = (tHash ^ (tHash >> 13)) * 1274126177U;
        
        float tNoise = heightNoise.GetNoise((float)tx, (float)tz);
        float tTemp = heightNoise.GetNoise(tz * 0.4f - 10000.0f, tx * 0.4f + 10000.0f);
        const Biome* b = BiomeManager::getBiome(tNoise, tTemp);
        
        if (b->treeDensity > 0 && (tHash % 1000) < b->treeDensity) {
            int th = b->getTerrainHeight(tNoise);
            if (th > 64 && th < 130) {
                outHeight = th;
                outBiome = b;
                return true;
            }
        }
        return false;
    };

    struct TreeInfo {
        bool exists;
        int height;
        const Biome* biome;
    };
    TreeInfo localTrees[24][24];
    for (int lx = 0; lx < 24; ++lx) {
        for (int lz = 0; lz < 24; ++lz) {
            int tx = position_.x * CHUNK_SIZE - 4 + lx;
            int tz = position_.z * CHUNK_SIZE - 4 + lz;
            localTrees[lx][lz].exists = getTreeAt(tx, tz, localTrees[lx][lz].height, localTrees[lx][lz].biome);
        }
    }

    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            float globalX = position_.x * CHUNK_SIZE + x;
            float globalZ = position_.z * CHUNK_SIZE + z;
            
            float noiseVal = heightNoise.GetNoise(globalX, globalZ);
            float tempNoise = heightNoise.GetNoise(globalZ * 0.45f + 2137.0f, globalX * 0.45f - 1492.0f);
            float continental = heightNoise.GetNoise(globalX * 0.1f, globalZ * 0.1f);
            
            const Biome* biome = BiomeManager::getBiome(continental, tempNoise);
            int height = biome->getTerrainHeight(noiseVal);

            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int globalY = position_.y * CHUNK_SIZE + y;
                
                if (globalY < height) {
                    float caveVal = caveNoise.GetNoise(globalX, (float)globalY, globalZ);
                    float tunnelThreshold = 0.5f;
                    
                    float depth = height - globalY;
                    if (depth < 15.0f) {
                        tunnelThreshold += (15.0f - depth) * 0.05f;
                    }
                    
                    if (globalY == -64) {
                        setVoxel(x, y, z, 12); // Bedrock
                    } else if (globalY > -95 && caveVal > tunnelThreshold) {
                        setVoxel(x, y, z, 1); // Air Cave
                    } else {
                        if (globalY == height - 1) {
                            if (globalY < 64 && biome->name != "Deep Ocean") setVoxel(x, y, z, 9); // Sand beach
                            else setVoxel(x, y, z, biome->surfaceBlock);
                        } else if (globalY > height - 4) {
                            setVoxel(x, y, z, biome->subSurfaceBlock);
                        } else {
                            uint8_t baseStone = (globalY < -10) ? 17 : 4; 
                            int modHash = std::abs(((int)globalX * 73 + (int)globalY * 31 + (int)globalZ * 17 + Config::currentSeed) % 1000);
                            
                            if (globalY < 16 && modHash < 3)        setVoxel(x, y, z, 15);
                            else if (globalY < 32 && modHash < 7)   setVoxel(x, y, z, 14);
                            else if (globalY < 55 && modHash < 15)  setVoxel(x, y, z, 13);
                            else if (globalY < 30 && modHash > 994) setVoxel(x, y, z, 16);
                            else if (globalY < 16 && modHash > 990) setVoxel(x, y, z, 18);
                            else if (globalY < 60 && modHash > 980) setVoxel(x, y, z, 8);
                            else setVoxel(x, y, z, baseStone);
                        }
                    }
                } 
                else if (globalY <= 63) {
                    setVoxel(x, y, z, 5); // Water
                } 
                else {
                    setVoxel(x, y, z, 1); // Base Air
                    
                    if (biome->treeDensity == 0) continue;

                    // Evaluate tree structure exactly for current position globally
                    bool isWood = false;
                    bool isLeaf = false;
                    bool isSnow = false;
                    
                    // Note: Check up to radius 3 for pine tree leaves
                    for (int dx = -3; dx <= 3 && !isWood && !isLeaf && !isSnow; ++dx) {
                        for (int dz = -3; dz <= 3 && !isWood && !isLeaf && !isSnow; ++dz) {
                            int tx = globalX + dx;
                            int tz = globalZ + dz;
                            int lx = x + dx + 4;
                            int lz = z + dz + 4;
                            
                            if (localTrees[lx][lz].exists) {
                                int tHeight = localTrees[lx][lz].height;
                                const Biome* tBiome = localTrees[lx][lz].biome;
                                int dy = globalY - tHeight;
                                // Core properties of this tree
                                if (tBiome->treeType == TreeType::OAK) {
                                    if (dx == 0 && dz == 0 && dy >= 0 && dy <= 4) isWood = true;
                                    else if (dy >= 3 && dy <= 5) {
                                        if (std::abs(dx) <= 2 && std::abs(dz) <= 2) {
                                            if (dy == 5 && (std::abs(dx) == 2 || std::abs(dz) == 2)) continue;
                                            isLeaf = true;
                                        }
                                    }
                                } else if (tBiome->treeType == TreeType::PINE) {
                                    // Pine goes from 0 to 7 high
                                    if (dx == 0 && dz == 0 && dy >= 0 && dy <= 7) isWood = true;
                                    else if (dy >= 2 && dy <= 8) {
                                        int radius = (8 - dy) / 2 + 1; // 2 at dy=2, 2 at dy=3, 1 at dy=4..
                                        if (dy == 2) radius = 2;
                                        if (dy == 3) radius = 2;
                                        if (dy == 4) radius = 1;
                                        if (dy == 5) radius = 1;
                                        if (dy == 6) radius = 1;
                                        if (dy == 7) radius = 0;
                                        
                                        if (std::abs(dx) <= radius && std::abs(dz) <= radius) {
                                            if (std::abs(dx) == radius && std::abs(dz) == radius && radius > 0) {
                                                // Round corners slightly
                                                int rHash = std::abs(tx * 31 + tz * 17 + dy) % 100;
                                                if (rHash < 40) continue; 
                                            }
                                            isLeaf = true;
                                        }
                                    }
                                }
                                
                                // Added Snow placement logic on top of leaf bounds!
                                if (tBiome->treeType == TreeType::PINE && tBiome->name == "Taiga") {
                                    // If we are exactly +1 above a leaf block, place snow!
                                    int ldy = dy - 1; 
                                    if (ldy >= 2 && ldy <= 7) {
                                        int radius = (8 - ldy) / 2 + 1;
                                        if (ldy == 2) radius = 2;
                                        if (ldy == 3) radius = 2;
                                        if (ldy == 4) radius = 1;
                                        if (ldy == 5) radius = 1;
                                        if (ldy == 6) radius = 1;
                                        if (ldy == 7) radius = 0;
                                        
                                        if (std::abs(dx) <= radius && std::abs(dz) <= radius) {
                                            // Make sure we didn't just place a leaf HERE too!
                                            // Snow only exists if we aren't leaf or wood
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (isWood) setVoxel(x, y, z, 6); // Spruce Log / Oak Log
                    else if (isLeaf) setVoxel(x, y, z, 7); // Leaf Block
                    else if (isSnow) setVoxel(x, y, z, 10); // Snow
                }
            }
            
            // Post-pass for snow on top of leaves
            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int globalY = position_.y * CHUNK_SIZE + y;
                if (globalY > height) {
                    uint8_t under = getVoxel(x, y - 1, z);
                    uint8_t current = getVoxel(x, y, z);
                    if (current == 1 && under == 7 && (biome->name == "Taiga" || biome->name == "Tundra")) {
                        // Place snow randomly on top of exposed leaves
                        if (std::abs((int)globalX * 13 + (int)globalZ * 7 + globalY) % 100 < 80) {
                            setVoxel(x, y, z, 10);
                        }
                    }
                }
            }
        }
    }
}

void Chunk::generateTerrainV2() {
    // V2: Crazy generation, tectonic world, realistic landscapes, fixed caves, deeper non-linear lakes, 10-15 biomes.
    FastNoiseLite tectonicNoise;
    tectonicNoise.SetSeed(Config::currentSeed + 1234);
    tectonicNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); // Curve Mapped OpenSimplex2
    tectonicNoise.SetFrequency(0.0015f);
    tectonicNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    
    FastNoiseLite erosionNoise;
    erosionNoise.SetSeed(Config::currentSeed + 5678);
    erosionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    erosionNoise.SetFrequency(0.002f);
    erosionNoise.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive); // Domain Warping
    
    FastNoiseLite tempNoise;
    tempNoise.SetSeed(Config::currentSeed + 9101);
    tempNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    tempNoise.SetFrequency(0.004f);

    FastNoiseLite humNoise;
    humNoise.SetSeed(Config::currentSeed + 1121);
    humNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    humNoise.SetFrequency(0.004f);
    
    FastNoiseLite caveNoise3D;
    caveNoise3D.SetSeed(Config::currentSeed + 3141);
    caveNoise3D.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); 
    caveNoise3D.SetFrequency(0.025f); // "Swiss Cheese" logic
    
    FastNoiseLite densityNoise3D;
    densityNoise3D.SetSeed(Config::currentSeed + 9999);
    densityNoise3D.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    densityNoise3D.SetFrequency(0.015f);
    densityNoise3D.SetFractalType(FastNoiseLite::FractalType_FBm);
    densityNoise3D.SetFractalOctaves(3);

    FastNoiseLite clusterNoise;
    clusterNoise.SetSeed(Config::currentSeed + 7777);
    clusterNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    clusterNoise.SetFrequency(0.008f); // Shared Flora Clustering Noise

    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            float gX = position_.x * CHUNK_SIZE + x;
            float gZ = position_.z * CHUNK_SIZE + z;
            
            float tec = tectonicNoise.GetNoise(gX, gZ); // -1 to 1
            float ero = erosionNoise.GetNoise(gX, gZ);  // -1 to 1
            float tmp = tempNoise.GetNoise(gX, gZ);
            float hum = humNoise.GetNoise(gX, gZ);
            
            // Build Continent Height
            float baseH = (tec * 0.5f + 0.5f); // 0 to 1
            float terrainVariance = 1.0f - std::abs(ero); // Bumpy vs smooth
            
            // Mountains are where continentalness is high and erosion is low
            float finalNormalized = baseH * baseH + (terrainVariance * 0.4f * (baseH)); 
            
            // Bias land massively to reduce oceans!
            finalNormalized = finalNormalized * 1.25f + 0.1f;
            
            // Height remap: -100 to 200 (Normal Large Mountains)
            int h = static_cast<int>(finalNormalized * 260.0f) - 60;
            
            // Lakes (deep curved lakes instead of straight digged)
            if (h < 0 && h > -40) {
                // Blend down smoothly for lakes
                h -= std::pow(-h, 1.2f);
            }

            // Biome selection inline for our 15 distinct environments
            uint8_t surf = 2, sub = 3;
            std::string bName = "Plains";
            int treeChance = 0;
            TreeType tType = TreeType::NONE;
            
            if (h < 5) { // Ocean / Deep Lake levels
                if (tmp < -0.3f) { bName = "Frozen Ocean"; surf = 10; sub = 10; } // Snow
                else if (tmp > 0.4f) { bName = "Warm Ocean"; surf = 9; sub = 9; } // Sand
                else { bName = "Deep Ocean"; surf = 27; sub = 3; } // Gravel over dirt
            } else if (h > 180) { // Extremely High Mountains
                if (tmp < 0.0f) { bName = "Snowy Peaks"; surf = 10; sub = 26; } // Snow over Calcite
                else { bName = "Stone Peaks"; surf = 22; sub = 24; } // Diorite over Andesite
                if (hum > 0.3f && h < 250) { treeChance = 20; tType = TreeType::PINE; }
            } else {
                if (tmp < -0.4f) {
                    if (hum < -0.2f) { bName = "Tundra"; surf = 11; sub = 3; treeChance = 5; tType = TreeType::PINE; }
                    else { bName = "Snowy Taiga"; surf = 11; sub = 3; treeChance = 60; tType = TreeType::PINE; }
                } else if (tmp < 0.0f) {
                    if (hum > 0.3f) { bName = "Old Growth Pine"; surf = 29; sub = 3; treeChance = 90; tType = TreeType::PINE; } // Podzol
                    else { bName = "Grove"; surf = 10; sub = 3; treeChance = 15; tType = TreeType::PINE; } // Snow
                } else if (tmp > 0.5f) {
                    if (hum < -0.2f) { bName = "Desert"; surf = 9; sub = 9; }
                    else if (hum > 0.3f) { bName = "Jungle"; surf = 28; sub = 3; treeChance = 150; tType = TreeType::OAK; } // Moss_block
                    else { bName = "Savanna"; surf = 30; sub = 3; treeChance = 10; tType = TreeType::OAK; } // Coarse dirt
                } else {
                    if (hum > 0.4f) { bName = "Swamp"; surf = 30; sub = 3; treeChance = 20; tType = TreeType::OAK; } // Coarse dirt
                    else if (hum > 0.1f) { bName = "Forest"; surf = 2; sub = 3; treeChance = 80; tType = TreeType::OAK; }
                    else { bName = "Plains"; surf = 2; sub = 3; treeChance = 2; tType = TreeType::OAK; }
                }
            }
            
            // Clustering Noise for plants/trees
            float plantCluster = clusterNoise.GetNoise(gX, gZ); 
            if (plantCluster < -0.2f) treeChance /= 4; // Clearing
            else if (plantCluster > 0.3f) treeChance *= 2; // Forest Grove
            
            // Check Tree
            uint32_t cxHash = std::hash<int>()(static_cast<int>(gX));
            uint32_t czHash = std::hash<int>()(static_cast<int>(gZ));
            uint32_t tMix = cxHash ^ (czHash << 1) ^ Config::currentSeed;
            bool wantTree = (treeChance > 0 && (tMix % 1000) < (uint32_t)treeChance);
            
            // 3D Density Evaluation column (overscanned by 5 to check surface states!)
            float blockDensities[CHUNK_SIZE + 6]; 
            for (int y = -1; y <= CHUNK_SIZE + 4; ++y) {
                int gY = position_.y * CHUNK_SIZE + y;
                if (gY <= -100) {
                    blockDensities[y + 1] = 100.0f;
                } else if (std::abs(gY - h) > 30) {
                    // Optimization: Far from surface, skip expensive 3D volumetric noise
                    blockDensities[y + 1] = (float)h - (float)gY;
                } else {
                    float stepHytaleRemap = std::sin((float)gY * 0.3f) * 2.5f; // Terracing
                    float denVal = densityNoise3D.GetNoise(gX, (float)gY * 1.5f, gZ) * 22.0f;
                    blockDensities[y + 1] = (float)h - (float)gY + denVal + stepHytaleRemap;
                }
            }

            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int gY = position_.y * CHUNK_SIZE + y;
                
                float density = blockDensities[y + 1];
                float denAbove1 = blockDensities[y + 2];
                float denAbove4 = blockDensities[y + 5];
                
                if (density > 0.0f) {
                    float cvVal = 1.0f;
                    if (gY < -5 && gY > -95) {
                        // Optimization: Only evaluate 3D cave noise deep underground!
                        cvVal = caveNoise3D.GetNoise(gX, (float)gY * 1.5f, gZ); 
                    }
                    
                    if (gY <= -100) {
                        setVoxel(x, y, z, 12); // Bedrock limit at -100
                    } else if (gY < -5 && std::abs(cvVal) < 0.15f) { 
                        // Modified Hytale 3D caves (Swiss Cheese tunnels)
                        setVoxel(x, y, z, 1);
                    } else {
                        // Terrain placement based on depth evaluation!
                        if (denAbove1 <= 0.0f) {
                            if (gY < 6 && surf == 2) setVoxel(x, y, z, 9); // Beach sand near water
                            else setVoxel(x, y, z, surf);
                        } else if (denAbove4 <= 0.0f) {
                            setVoxel(x, y, z, sub);
                        } else {
                            // Safe Ores
                            int oreHash = std::abs((int)gX * 73 + (int)gY * 31 + (int)gZ * 17 + Config::currentSeed) % 1000;
                            if (gY < -20) setVoxel(x, y, z, 17); // Deepslate
                            else if (gY > 150) setVoxel(x, y, z, 25); // Tuff in high peaks!
                            else if (gY > 100 && (oreHash < 500)) setVoxel(x, y, z, 24); // Andesite mountains!
                            else setVoxel(x, y, z, 4); // Stone
                            
                            if (gY < 40 && oreHash < 10) setVoxel(x, y, z, 13); // Iron
                            else if (gY < 10 && oreHash > 985) setVoxel(x, y, z, 15); // Gold
                            else if (gY < -20 && oreHash > 970 && oreHash <= 985) setVoxel(x, y, z, 14); // Diamonds!
                        }
                    }
                } 
                else {
                    // Empty space evaluation (Water, Grass)
                    if (gY <= 5) {
                        setVoxel(x, y, z, 5); // Still water
                    } else {
                        // Flora Surface Clustering
                        float denBelow = (y >= 0) ? blockDensities[y] : -1.0f;
                        if (denBelow > 0.0f && !wantTree && plantCluster > 0.0f) {
                            uint32_t fMix = (uint32_t)std::abs((int)gX * 13 + (int)gZ * 7 + gY + Config::currentSeed);
                            if (surf == 2 || surf == 30) { 
                                if (fMix % 100 < 20) setVoxel(x, y, z, 31); 
                                else if (fMix % 100 < 23) setVoxel(x, y, z, 32); 
                                else setVoxel(x, y, z, 1);
                            } else if (surf == 28) {
                                if (fMix % 100 < 40) setVoxel(x, y, z, 32); 
                                else if (fMix % 100 < 60) setVoxel(x, y, z, 31); 
                                else setVoxel(x, y, z, 1);
                            } else if (surf == 9) {
                                if (fMix % 100 < 3) setVoxel(x, y, z, 33); 
                                else setVoxel(x, y, z, 1);
                            } else setVoxel(x, y, z, 1);
                        } else {
                            setVoxel(x, y, z, 1);
                        }
                    }
                }
            }
        }
    }
    
    // Post process trees correctly using proper neighbor overlap
    for (int lx = 0; lx < 24; ++lx) {
        for (int lz = 0; lz < 24; ++lz) {
            int tx = position_.x * CHUNK_SIZE - 4 + lx;
            int tz = position_.z * CHUNK_SIZE - 4 + lz;
            
            float tec = tectonicNoise.GetNoise((float)tx, (float)tz);
            float ero = erosionNoise.GetNoise((float)tx, (float)tz);
            float tmp = tempNoise.GetNoise((float)tx, (float)tz);
            float hum = humNoise.GetNoise((float)tx, (float)tz);
            
            float baseH = (tec * 0.5f + 0.5f);
            float terrainVariance = 1.0f - std::abs(ero);
            float finalNormalized = baseH * baseH + (terrainVariance * 0.4f * (baseH)); 
            finalNormalized = finalNormalized * 1.25f + 0.1f;
            
            int th = static_cast<int>(finalNormalized * 260.0f) - 60;
            if (th < 0 && th > -40) th -= std::pow(-th, 1.2f);
            
            int tChance = 0;
            TreeType tt = TreeType::NONE;
            
            if (th > 5 && th < 250) {
                if (tmp < -0.4f) {
                    if (hum < -0.2f) { tChance = 5; tt = TreeType::PINE; }
                    else { tChance = 60; tt = TreeType::PINE; }
                } else if (tmp < 0.0f) {
                    if (hum > 0.3f) { tChance = 90; tt = TreeType::PINE; }
                    else { tChance = 15; tt = TreeType::PINE; }
                } else if (tmp > 0.5f) {
                    if (hum < -0.2f) {} // Desert
                    else if (hum > 0.3f) { tChance = 150; tt = TreeType::OAK; }
                    else { tChance = 10; tt = TreeType::OAK; }
                } else {
                    if (hum > 0.4f) { tChance = 20; tt = TreeType::OAK; }
                    else if (hum > 0.1f) { tChance = 80; tt = TreeType::OAK; }
                    else { tChance = 2; tt = TreeType::OAK; }
                }
            }
            
            // Better random hash for tree spawning to eliminate linear patterns
            auto hashV = [](int x, int z) {
                uint32_t a = (uint32_t)x * 374761393U;
                uint32_t b = (uint32_t)z * 668265263U;
                a ^= a >> 13; a *= 1274126177U;
                b ^= b >> 13; b *= 1274126177U;
                return (a ^ b) ^ Config::currentSeed;
            };
            uint32_t postTreeMix = hashV(tx, tz);
            
            if (tChance > 0 && (postTreeMix % 1000) < (uint32_t)tChance && th > 5 && th < 250) {
                // Loop over the tree's volume (relative to root)
                for (int dy = 0; dy <= 8; ++dy) {
                    int gY = th + dy;
                    int chunkY = gY - position_.y * CHUNK_SIZE;
                    if (chunkY < -1 || chunkY > CHUNK_SIZE) continue; 
                    
                    int rootRelX = lx - 4;
                    int rootRelZ = lz - 4;
                    
                    if (tt == TreeType::OAK) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            for (int dz = -2; dz <= 2; ++dz) {
                                int relX = rootRelX + dx;
                                int relZ = rootRelZ + dz;
                                if (relX >= -1 && relX <= CHUNK_SIZE && relZ >= -1 && relZ <= CHUNK_SIZE) {
                                    if (dx == 0 && dz == 0 && dy <= 4) {
                                        setVoxel(relX, chunkY, relZ, 6);
                                    } else if (dy >= 3 && dy <= 5 && std::abs(dx) <= 2 && std::abs(dz) <= 2) {
                                        if (std::abs(dx) == 2 && std::abs(dz) == 2 && dy == 5) continue;
                                        if (getVoxel(relX, chunkY, relZ) <= 1) setVoxel(relX, chunkY, relZ, 7);
                                    }
                                }
                            }
                        }
                    } else if (tt == TreeType::PINE) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            for (int dz = -2; dz <= 2; ++dz) {
                                int relX = rootRelX + dx;
                                int relZ = rootRelZ + dz;
                                if (relX >= -1 && relX <= CHUNK_SIZE && relZ >= -1 && relZ <= CHUNK_SIZE) {
                                    if (dx == 0 && dz == 0 && dy <= 7) {
                                        setVoxel(relX, chunkY, relZ, 6);
                                    } else if (dy >= 2 && dy <= 8) {
                                        int rad = (8 - dy) / 2 + 1;
                                        if (dy == 2) rad = 2; else if (dy == 3) rad = 2;
                                        else if (dy == 4) rad = 1; else if (dy == 5) rad = 1;
                                        else if (dy == 6) rad = 1; else rad = 0;
                                        
                                        if (std::abs(dx) <= rad && std::abs(dz) <= rad && rad > 0) {
                                            if (std::abs(dx) == rad && std::abs(dz) == rad && (std::abs(tx*31+tz*17+dy)%100 < 40)) continue;
                                            if (getVoxel(relX, chunkY, relZ) <= 1) setVoxel(relX, chunkY, relZ, 7);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

bool Chunk::isFaceVisible(uint8_t currentType, int x, int y, int z, int dx, int dy, int dz) const {
    int nx = x + dx;
    int ny = y + dy;
    int nz = z + dz;
    uint8_t neighbor = getVoxel(nx, ny, nz);
    
    if (neighbor == 0 || neighbor == 1) return true;
    
    bool isCurrentTransparent = (currentType == 5 || currentType == 7 || currentType == 21 || currentType == 31 || currentType == 32 || currentType == 33);
    bool isNeighborTransparent = (neighbor == 5 || neighbor == 7 || neighbor == 21 || neighbor == 31 || neighbor == 32 || neighbor == 33);
    
    if (!isCurrentTransparent && isNeighborTransparent) return true;
    if (isCurrentTransparent && isNeighborTransparent && currentType != neighbor) return true;
    
    return false;
}

void Chunk::addFace(int x, int y, int z, int dir, uint8_t type, int width, int height, float temp, float hum) {
    if (type <= 1) return; 
    
    int startIdx = vertices_.size();

    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    if (type == 2 || type == 7 || type == 11 || type == 31 || type == 32) {
        // Biome Polytone math based on Temperature and Humidity
        float t = glm::clamp(temp * 0.5f + 0.5f, 0.0f, 1.0f);
        float h = glm::clamp(hum * 0.5f + 0.5f, 0.0f, 1.0f);

        float rVal = glm::mix(glm::mix(0.50f, 0.25f, h), glm::mix(0.75f, 0.15f, h), t);
        float gVal = glm::mix(glm::mix(0.65f, 0.55f, h), glm::mix(0.75f, 0.85f, h), t);
        float bVal = glm::mix(glm::mix(0.55f, 0.35f, h), glm::mix(0.25f, 0.15f, h), t);
        
        if (type == 7 || type == 31 || type == 32) { 
            // Leaves and foliage are slightly darker
            rVal *= 0.8f; gVal *= 0.8f; bVal *= 0.8f; 
            if (dir == 2 || dir == 3) { r = rVal; g = gVal; b = bVal; } 
            else { r = rVal * 0.9f; g = gVal * 0.9f; b = bVal * 0.9f; }
            a = 0.85f; // Tells vertex shader to apply wind sway!
        } else if (type == 2) { 
            // Grass
            if (dir == 2) { r = rVal; g = gVal; b = bVal; } 
        } else if (type == 11) {
            // Snowy grass shouldn't be fully tinted, but somewhat duller underlying green
            if (dir == 2) { r = 0.95f; g = 0.95f; b = 0.95f; } // pure snow on top
            else if (dir != 3) { r = rVal; g = gVal; b = bVal; } // side grass tinted
        }
    } else if (type == 5) { 
        a = 0.7f; 
    } else if (type == 6) { 
        r = 0.65f; g = 0.5f; b = 0.35f; 
    }
    
    float layer = TextureAtlas::getUVForBlock(type, dir);

    for (int i = 0; i < 4; ++i) {
        float vx = FACE_VERTICES[dir][i][0];
        float vy = FACE_VERTICES[dir][i][1];
        float vz = FACE_VERTICES[dir][i][2];
        
        if (dir == 0 || dir == 1) { // Normal X
            vy *= height; 
            vz *= width;  
        } else if (dir == 2 || dir == 3) { // Normal Y
            vx *= width;  
            vz *= height; 
        } else if (dir == 4 || dir == 5) { // Normal Z
            vx *= width;  
            vy *= height; 
        }

        vx += (position_.x * CHUNK_SIZE) + x;
        vy += (position_.y * CHUNK_SIZE) + y;
        vz += (position_.z * CHUNK_SIZE) + z;
        
        if (type == 5 && FACE_VERTICES[dir][i][1] > 0.5f && dir == 2) {
            vy -= 0.15f;
        }
        
        uint32_t packedData = (dir & 0x7) | 
                              ((width & 0x1F) << 3) | 
                              ((height & 0x1F) << 8) | 
                              ((static_cast<uint32_t>(layer) & 0xFFFF) << 13) | 
                              ((i & 0x3) << 29);

        VoxelVertex vertex;
        vertex.x = vx;
        vertex.y = vy;
        vertex.z = vz;
        vertex.data = packedData;
        vertex.r = static_cast<uint8_t>(r * 255.0f);
        vertex.g = static_cast<uint8_t>(g * 255.0f);
        vertex.b = static_cast<uint8_t>(b * 255.0f);
        vertex.a = static_cast<uint8_t>(a * 255.0f);

        vertices_.push_back(vertex);
    }
    
    if (type == 5 || type == 7 || type == 21 || type == 31 || type == 32 || type == 33 || type == 34) { 
        transparentIndices_.push_back(startIdx + 0);
        transparentIndices_.push_back(startIdx + 1);
        transparentIndices_.push_back(startIdx + 2);
        transparentIndices_.push_back(startIdx + 0);
        transparentIndices_.push_back(startIdx + 2);
        transparentIndices_.push_back(startIdx + 3);
    } else {
        indices_.push_back(startIdx + 0);
        indices_.push_back(startIdx + 1);
        indices_.push_back(startIdx + 2);
        indices_.push_back(startIdx + 0);
        indices_.push_back(startIdx + 2);
        indices_.push_back(startIdx + 3);
    }    
}

void Chunk::addCrossFace(int x, int y, int z, uint8_t type, float temp, float hum) {
    if (type <= 1) return;
    
    float layer = TextureAtlas::getUVForBlock(type, 0); 
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 0.85f;
    
    if (type == 31 || type == 32 || type == 34) { // tint plants & kelp
        float t = glm::clamp(temp * 0.5f + 0.5f, 0.0f, 1.0f);
        float h = glm::clamp(hum * 0.5f + 0.5f, 0.0f, 1.0f);
        r = glm::mix(glm::mix(0.50f, 0.25f, h), glm::mix(0.75f, 0.15f, h), t) * 0.85f;
        g = glm::mix(glm::mix(0.65f, 0.55f, h), glm::mix(0.75f, 0.85f, h), t) * 0.85f;
        b = glm::mix(glm::mix(0.55f, 0.35f, h), glm::mix(0.25f, 0.15f, h), t) * 0.85f;
    }
    
    auto addQuad = [&](float x1, float z1, float x2, float z2, int normD) {
        int startIdx = vertices_.size();
        for (int i = 0; i < 4; ++i) {
            float vx, vy, vz;
            if (i == 0) { vx = x1; vz = z1; vy = 0.0f; }
            if (i == 1) { vx = x2; vz = z2; vy = 0.0f; }
            if (i == 2) { vx = x2; vz = z2; vy = 1.0f; }
            if (i == 3) { vx = x1; vz = z1; vy = 1.0f; }
            
            int cornerConfig = (i == 0) ? 1 : ((i == 1) ? 2 : ((i == 2) ? 3 : 0)); 
            
            VoxelVertex vertex;
            vertex.x = (position_.x * CHUNK_SIZE) + x + vx;
            vertex.y = (position_.y * CHUNK_SIZE) + y + vy;
            vertex.z = (position_.z * CHUNK_SIZE) + z + vz;
            vertex.data = (normD & 0x7) | (1 << 3) | (1 << 8) | ((static_cast<uint32_t>(layer) & 0xFFFF) << 13) | ((cornerConfig & 0x3) << 29);
            vertex.r = static_cast<uint8_t>(r * 255.0f);
            vertex.g = static_cast<uint8_t>(g * 255.0f);
            vertex.b = static_cast<uint8_t>(b * 255.0f);
            vertex.a = static_cast<uint8_t>(a * 255.0f);
            vertices_.push_back(vertex);
        }
        transparentIndices_.push_back(startIdx + 0);
        transparentIndices_.push_back(startIdx + 1);
        transparentIndices_.push_back(startIdx + 2);
        transparentIndices_.push_back(startIdx + 0);
        transparentIndices_.push_back(startIdx + 2);
        transparentIndices_.push_back(startIdx + 3);
    };
    
    addQuad(0.15f, 0.15f, 0.85f, 0.85f, 0); // Diag 1: +X like
    addQuad(0.85f, 0.85f, 0.15f, 0.15f, 1); // Diag 1 back: -X like
    addQuad(0.15f, 0.85f, 0.85f, 0.15f, 4); // Diag 2: +Z like
    addQuad(0.85f, 0.15f, 0.15f, 0.85f, 5); // Diag 2 back: -Z like
}

void Chunk::generateMesh() {
    vertices_.clear();
    indices_.clear();
    transparentIndices_.clear();

    float tempMap[CHUNK_SIZE][CHUNK_SIZE] = {0};
    float humMap[CHUNK_SIZE][CHUNK_SIZE] = {0};

    if (Config::currentWorldType == 3) { 
        FastNoiseLite tempNoise, humNoise;
        tempNoise.SetSeed(Config::currentSeed + 9101);
        tempNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        tempNoise.SetFrequency(0.004f);
        
        humNoise.SetSeed(Config::currentSeed + 1121);
        humNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        humNoise.SetFrequency(0.005f);

        for (int i = 0; i < CHUNK_SIZE; ++i) {
            for (int k = 0; k < CHUNK_SIZE; ++k) {
                tempMap[i][k] = tempNoise.GetNoise(position_.x * CHUNK_SIZE + (float)i, position_.z * CHUNK_SIZE + (float)k);
                humMap[i][k]  = humNoise.GetNoise(position_.x * CHUNK_SIZE + (float)i, position_.z * CHUNK_SIZE + (float)k);
            }
        }
    }

    for (int dir = 0; dir < 6; ++dir) {
        for (int slice = 0; slice < CHUNK_SIZE; ++slice) {
            uint8_t mask[CHUNK_SIZE][CHUNK_SIZE] = {0};
            
            for (int v = 0; v < CHUNK_SIZE; ++v) {
                for (int u = 0; u < CHUNK_SIZE; ++u) {
                    int x, y, z;
                    if (dir == 0 || dir == 1) { x = slice; y = v; z = u; }
                    else if (dir == 2 || dir == 3) { x = u; y = slice; z = v; }
                    else { x = u; y = v; z = slice; }
                    
                    uint8_t type = getVoxel(x, y, z);
                    bool isCross = (type >= 31 && type <= 34); 
                    
                    if (isCross) {
                        if (dir == 0) addCrossFace(x, y, z, type, tempMap[x][z], humMap[x][z]);
                        // Plant bypasses solid greedy meshing!
                        continue;
                    }

                    if (type > 1 && isFaceVisible(type, x, y, z, DIRS[dir][0], DIRS[dir][1], DIRS[dir][2])) {
                        mask[u][v] = type;
                    }
                }
            }
            
            for (int v = 0; v < CHUNK_SIZE; ++v) {
                for (int u = 0; u < CHUNK_SIZE; ++u) {
                    uint8_t type = mask[u][v];
                    if (type != 0) {
                        int width = 1;
                        if (type != 5) { // DISALLOW greedy meshing for water to allow smooth waves
                            while (u + width < CHUNK_SIZE && mask[u + width][v] == type) {
                                width++;
                            }
                        }
                        
                        int height = 1;
                        if (type != 5) { // DISALLOW greedy meshing for water
                            bool done = false;
                            while (v + height < CHUNK_SIZE) {
                                for (int i = 0; i < width; ++i) {
                                    if (mask[u + i][v + height] != type) { done = true; break; }
                                }
                                if (done) break;
                                height++;
                            }
                        }
                        
                        int x, y, z;
                        if (dir == 0 || dir == 1) { x = slice; y = v; z = u; }
                        else if (dir == 2 || dir == 3) { x = u; y = slice; z = v; }
                        else { x = u; y = v; z = slice; }

                        float localTemp = tempMap[x][z];
                        float localHum = humMap[x][z];

                        addFace(x, y, z, dir, type, width, height, localTemp, localHum);

                        for (int hv = 0; hv < height; ++hv) {
                            for (int wu = 0; wu < width; ++wu) {
                                mask[u + wu][v + hv] = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}
