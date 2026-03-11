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

void Chunk::generateTerrain(FastNoiseLite& tectonic, FastNoiseLite& erosion, FastNoiseLite& temp, FastNoiseLite& hum, FastNoiseLite& density, FastNoiseLite& cave, FastNoiseLite& cluster) {
    if (Config::currentWorldType == 3) {
        generateTerrainV2(tectonic, erosion, temp, hum, density, cave, cluster);
        return;
    }
    
    // V1/Simple Fallback
    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            float gX = position_.x * CHUNK_SIZE + x;
            float gZ = position_.z * CHUNK_SIZE + z;
            float n = tectonic.GetNoise(gX, gZ);
            int h = static_cast<int>(60 + (n + 1.0f) * 40.0f);

            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int gY = position_.y * CHUNK_SIZE + y;
                if (gY < h) {
                    if (gY == h - 1) setVoxel(x, y, z, 2);
                    else if (gY > h - 4) setVoxel(x, y, z, 3);
                    else setVoxel(x, y, z, 4);
                } else if (gY <= 63) {
                    setVoxel(x, y, z, 5);
                }
            }
        }
    }
}

void Chunk::generateTerrainV2(FastNoiseLite& tectonic, FastNoiseLite& erosion, FastNoiseLite& temp, FastNoiseLite& hum, FastNoiseLite& density3D, FastNoiseLite& cave3D, FastNoiseLite& cluster) {
    struct ColumnInfo {
        int height;
        const Biome* biome;
        bool wantTree;
        TreeType tType;
        bool wantRock;
        uint8_t rockType;
        uint8_t groundType; 
        float slopeNormal; // y-component of the surface normal
    };

    FastNoiseLite rockNoise;
    rockNoise.SetSeed(Config::currentSeed + 777);
    rockNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    rockNoise.SetFrequency(0.04f);

    ColumnInfo grid[24][24];
    // First pass: Biome and Base Height
    for (int lx = 0; lx < 24; ++lx) {
        for (int lz = 0; lz < 24; ++lz) {
            float gX = (float)position_.x * CHUNK_SIZE - 4 + lx;
            float gZ = (float)position_.z * CHUNK_SIZE - 4 + lz;
            
            float tec = tectonic.GetNoise(gX, gZ);
            float tmp = temp.GetNoise(gX, gZ);
            float hmd = hum.GetNoise(gX, gZ);
            float ero = erosion.GetNoise(gX, gZ);
            
            grid[lx][lz].biome = BiomeManager::getBiome(tec, tmp, hmd);
            grid[lx][lz].height = BiomeManager::getGlobalHeight(tec, ero);
            grid[lx][lz].wantTree = false;
            grid[lx][lz].wantRock = false;
        }
    }

    // Second pass: Contextual Heuristics (Normals) & Distribution (Jittered Grid)
    for (int lx = 4; lx < 20; ++lx) {
        for (int lz = 4; lz < 20; ++lz) {
            // Sample 3x3 for normal
            float hL = (float)grid[lx-1][lz].height;
            float hR = (float)grid[lx+1][lz].height;
            float hU = (float)grid[lx][lz-1].height;
            float hD = (float)grid[lx][lz+1].height;
            
            glm::vec3 va(2.0f, hR - hL, 0.0f);
            glm::vec3 vb(0.0f, hD - hU, 2.0f);
            glm::vec3 normal = glm::normalize(glm::cross(vb, va));
            grid[lx][lz].slopeNormal = normal.y;

            float gX = (float)position_.x * CHUNK_SIZE - 4 + lx;
            float gZ = (float)position_.z * CHUNK_SIZE - 4 + lz;

            // Jittered Grid Distribution (Poisson Disc approx)
            // Divide chunk into 2x2 cells for trees (4x more spots than 4x4)
            if (lx % 2 == 0 && lz % 2 == 0) {
                uint32_t cellSeed = static_cast<uint32_t>(gX) * 131 + static_cast<uint32_t>(gZ) * 17 + Config::currentSeed;
                int jX = (cellSeed % 2);
                int jZ = (cellSeed / 2) % 2;
                
                int targetX = lx + jX;
                int targetZ = lz + jZ;
                
                if (targetX < 24 && targetZ < 24) {
                    float cld = cluster.GetNoise(gX + jX, gZ + jZ);
                    int chance = grid[targetX][targetZ].biome->treeDensity;
                    
                    // User wants 10x more tree placement & dense forests
                    if (cld < -0.3f) chance = 0; // Natural gaps
                    else if (cld > 0.4f) chance *= 5; // Very dense forest clumps
                    else if (cld > 0.1f) chance *= 2; // Denser
                    
                    uint32_t tSeed = static_cast<uint32_t>(gX + jX) * 374761393U ^ static_cast<uint32_t>(gZ + jZ) * 668265263U ^ Config::currentSeed;
                    if (chance > 0 && (tSeed % 1000) < (uint32_t)chance && grid[targetX][targetZ].height > 65) {
                        grid[targetX][targetZ].wantTree = true;
                        grid[targetX][targetZ].tType = grid[targetX][targetZ].biome->treeType;
                    }
                }
            }

            float rn = rockNoise.GetNoise(gX, gZ);
            if (rn > 0.96f && grid[lx][lz].height > 63 && !grid[lx][lz].wantTree) {
                grid[lx][lz].wantRock = true;
                uint32_t rSeed = (uint32_t)gX * 123 + (uint32_t)gZ * 456 + Config::currentSeed;
                int rRnd = rSeed % 100;
                if (rRnd < 30) grid[lx][lz].rockType = 24;
                else if (rRnd < 60) grid[lx][lz].rockType = 22;
                else if (rRnd < 90) grid[lx][lz].rockType = 23;
                else grid[lx][lz].rockType = 4;
            }
        }
    }

    auto generateStructure = [&](int bx, int by, int bz, const std::vector<std::pair<glm::ivec3, uint8_t>>& bks) {
        for (const auto& b : bks) {
            int lx = bx + b.first.x;
            int ly = by + b.first.y;
            int lz = bz + b.first.z;
            if (lx >= -1 && lx <= CHUNK_SIZE && lz >= -1 && lz <= CHUNK_SIZE && (ly + position_.y * CHUNK_SIZE) < 256) {
                 uint8_t existing = getVoxel(lx, ly, lz);
                 if (existing <= 1 || existing == 7) { // Only replace air (0/1) or existing leaves
                    setVoxel(lx, ly, lz, b.second);
                 }
            }
        }
    };
            
    // Step 1: Base Terrain & 3D Density
    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            float gX = position_.x * CHUNK_SIZE + x;
            float gZ = position_.z * CHUNK_SIZE + z;
            const ColumnInfo& col = grid[x + 4][z + 4];
            int h = col.height;
            const Biome* b = col.biome;

            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int gY = position_.y * CHUNK_SIZE + y;
                float density = (float)h - (float)gY;
                if (std::abs(gY - h) < 25) {
                    density += density3D.GetNoise(gX, (float)gY * 0.8f, gZ) * 7.2f;
                }

                if (density > 0.0f) {
                    float cvVal = 1.0f;
                    if (gY < h - 4) {
                        float v1 = cave3D.GetNoise(gX, (float)gY * 0.7f, gZ);
                        float v2 = cave3D.GetNoise(gX + 123.0f, (float)gY * 0.7f, gZ + 456.0f);
                        cvVal = std::abs(v1) + std::abs(v2);
                    }
                    
                    if (gY == -64) setVoxel(x, y, z, 12);
                    else if (cvVal < 0.08f) setVoxel(x, y, z, 0); // Air in caves
                    else {
                        // Correct surface detection: check if density above is < 0 or if we're at CHUNK limit
                        float h_above = (float)h - (float)(gY + 1) + density3D.GetNoise(gX, (float)(gY + 1) * 0.8f, gZ) * 7.2f;
                        bool isSurface = (h_above <= 0.0f);
                        
                        if (isSurface && gY >= 63) {
                            if (gY < 64 && b->name != "Ocean" && b->name != "Deep Ocean") setVoxel(x, y, z, 9);
                            else setVoxel(x, y, z, b->surfaceBlock);
                        } else if (h - gY <= 10) {
                            // Subsurface layers (Dirt/Sandstone etc)
                            if (gY < 63 && b->name == "Ocean") setVoxel(x, y, z, 27); // Gravel under water
                            else setVoxel(x, y, z, b->subSurfaceBlock);
                        } else {
                            uint32_t oreHash = static_cast<uint32_t>(std::abs((int)gX * 73 + (int)gY * 31 + (int)gZ * 17 + Config::currentSeed)) % 1000;
                            uint8_t stoneType = (gY < 0) ? 17 : 4;
                            if (gY < 40 && oreHash < 15) setVoxel(x, y, z, 13);
                            else if (gY < 15 && oreHash > 985) setVoxel(x, y, z, 15);
                            else if (gY < -10 && oreHash > 975 && oreHash <= 985) setVoxel(x, y, z, 14);
                            else if (gY < 50 && oreHash > 900 && oreHash < 915) setVoxel(x, y, z, 16);
                            else setVoxel(x, y, z, stoneType);
                        }
                    }
                } else if (gY <= 63) setVoxel(x, y, z, 5);
                else setVoxel(x, y, z, 1);
            }
        }
    }

    // Step 2: Decoration (Rocks, Trees, Flora)
    FastNoiseLite flowerPatch;
    flowerPatch.SetSeed(Config::currentSeed + 999);
    flowerPatch.SetFrequency(0.08f);

    for (int lx = 4; lx < 20; ++lx) {
        for (int lz = 4; lz < 20; ++lz) {
            const ColumnInfo& col = grid[lx][lz];
            int relX = lx - 4;
            int relZ = lz - 4;

            int surfaceY = col.height;
            uint8_t groundV = getVoxel(relX, surfaceY - position_.y * CHUNK_SIZE, relZ);
            // Ground Validation: Restrict to soil types (Grass, Dirt, Podzol, Coarse Dirt)
            bool isSoil = (groundV == 2 || groundV == 3 || groundV == 29 || groundV == 30);
            if (!isSoil && groundV != 9 && groundV != 10) continue; 
            
            int bx = relX;
            int by = surfaceY + 1 - position_.y * CHUNK_SIZE;
            int bz = relZ;

            if (col.wantTree && isSoil) {
                std::vector<std::pair<glm::ivec3, uint8_t>> prefab;
                uint8_t wood = (col.tType == TreeType::BIRCH) ? 20 : 6;
                uint8_t leaf = 7;
                
                if (col.tType == TreeType::JUNGLE) {
                    // JUNGLE VARIANT (Massive 2x2 trunk)
                    int th = 12 + (Config::currentSeed % 8);
                    for (int y = 0; y < th; ++y) {
                        prefab.push_back({{0, y, 0}, wood}); prefab.push_back({{1, y, 0}, wood});
                        prefab.push_back({{0, y, 1}, wood}); prefab.push_back({{1, y, 1}, wood});
                    }
                    // Giant branching
                    growBranch(0, th, 0, glm::normalize(glm::vec3(0.5f, 0.4f, 0.5f)), 8.0f, 2, col.tType, prefab);
                    growBranch(1, th, 1, glm::normalize(glm::vec3(-0.5f, 0.4f, -0.5f)), 8.0f, 2, col.tType, prefab);
                } else if (col.tType == TreeType::PINE) {
                    // PINE VARIANT (Conical)
                    int th = 9 + (Config::currentSeed % 6);
                    for (int y = 0; y < th; ++y) prefab.push_back({{0, y, 0}, wood});
                    for (int y = 2; y <= th + 2; ++y) {
                        int r = (th + 2 - y) / 2 + 1;
                        for (int dx = -r; dx <= r; ++dx) {
                            for (int dz = -r; dz <= r; ++dz) {
                                if (dx*dx + dz*dz <= r*r + 1) prefab.push_back({{dx, y, dz}, leaf});
                            }
                        }
                    }
                } else if (col.tType == TreeType::BIRCH) {
                    // BIRCH VARIANT (Tall & Slim)
                    int th = 7 + (Config::currentSeed % 4);
                    for (int y = 0; y < th; ++y) prefab.push_back({{0, y, 0}, 20}); // White wood
                    growBranch(0, th, 0, glm::normalize(glm::vec3(0, 1, 0)), 4.0f, 1, col.tType, prefab);
                } else if (col.tType == TreeType::ACACIA) {
                    // PALM/ACACIA (Leaning)
                    glm::vec3 dir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.4f));
                    growBranch(0, 0, 0, dir, 8.0f, 0, col.tType, prefab);
                    glm::vec3 end = dir * 8.0f;
                    for (int dx = -3; dx <= 3; ++dx) {
                        for (int dz = -3; dz <= 3; ++dz) {
                            if (dx*dx + dz*dz < 10) prefab.push_back({{(int)end.x + dx, (int)end.y, (int)end.z + dz}, 7});
                        }
                    }
                } else if (col.slopeNormal < 0.75f) {
                    // SLOPE TREE
                    glm::vec3 leanDir = glm::normalize(glm::vec3((lx < 12) ? 0.7f : -0.7f, 0.7f, 0.0f));
                    growBranch(0, 0, 0, leanDir, 5.0f, 1, col.tType, prefab);
                } else if (surfaceY <= 65) {
                    // WEEPING WILLOW
                    for(int y=0; y<6; ++y) prefab.push_back({{0, y, 0}, wood});
                    growBranch(0, 6, 0, glm::normalize(glm::vec3(0.5f, 0.1f, 0.5f)), 5.0f, 1, col.tType, prefab);
                    growBranch(0, 6, 0, glm::normalize(glm::vec3(-0.5f, 0.1f, -0.5f)), 5.0f, 1, col.tType, prefab);
                } else {
                    // MASSIVE OAK
                    int th = 5 + (Config::currentSeed % 4);
                    for (int y = 0; y < th; ++y) prefab.push_back({{0, y, 0}, wood});
                    growBranch(0, th, 0, glm::normalize(glm::vec3(0.7f, 0.4f, 0.3f)), 6.0f, 2, col.tType, prefab);
                    growBranch(0, th, 0, glm::normalize(glm::vec3(-0.7f, 0.4f, -0.3f)), 6.0f, 2, col.tType, prefab);
                }
                generateStructure(bx, by, bz, prefab);
            } else if (col.wantRock) {
                std::vector<std::pair<glm::ivec3, uint8_t>> rBlocks;
                uint8_t rt = col.rockType;
                rBlocks.push_back({{0, 0, 0}, rt});
                rBlocks.push_back({{1, 0, 0}, rt}); rBlocks.push_back({{-1, 0, 0}, rt});
                rBlocks.push_back({{0, 0, 1}, rt}); rBlocks.push_back({{0, 0, -1}, rt});
                rBlocks.push_back({{0, 1, 0}, rt});
                generateStructure(bx, by, bz, rBlocks);
            } else if (isSoil) {
                // FLORA CLUMPING (Air-check protection)
                if (getVoxel(bx, by, bz) == 0 && (by + 1 >= CHUNK_SIZE || getVoxel(bx, by + 1, bz) == 0)) {
                    float fClump = flowerPatch.GetNoise((float)position_.x * CHUNK_SIZE + bx, (float)position_.z * CHUNK_SIZE + bz);
                    uint32_t fSeed = static_cast<uint32_t>(relX * 73 + relZ * 31 + Config::currentSeed);
                    
                    if (fClump > 0.45f) { 
                        uint8_t flower = 35; // Default Poppy
                        if (fClump > 0.78f) flower = 37; 
                        else if (fClump > 0.62f) flower = 36; 
                        setVoxel(bx, by, bz, flower);
                    } else if ((fSeed % 100) < 14) { 
                        setVoxel(bx, by, bz, 31);
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
    
    bool isCurrentTransparent = (currentType == 5 || currentType == 7 || currentType == 21 || (currentType >= 31 && currentType <= 38) || currentType == 40 || currentType == 42 || currentType == 44);
    bool isNeighborTransparent = (neighbor == 5 || neighbor == 7 || neighbor == 21 || (neighbor >= 31 && neighbor <= 38) || neighbor == 40 || neighbor == 42 || neighbor == 44);
    
    if (!isCurrentTransparent && isNeighborTransparent) return true;
    if (isCurrentTransparent && isNeighborTransparent && currentType != neighbor) return true;
    
    return false;
}

void Chunk::addFace(int x, int y, int z, int dir, uint8_t type, int width, int height, float temp, float hum) {
    if (type <= 1) return; 
    
    int startIdx = vertices_.size();

    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    if (type == 2 || type == 7 || type == 11 || (type >= 31 && type <= 38)) {
        // Biome Polytone math based on Temperature and Humidity
        float t = glm::clamp(temp * 0.5f + 0.5f, 0.0f, 1.0f);
        float h = glm::clamp(hum * 0.5f + 0.5f, 0.0f, 1.0f);

        float rVal = glm::mix(glm::mix(0.35f, 0.20f, h), glm::mix(0.55f, 0.10f, h), t);
        float gVal = glm::mix(glm::mix(0.70f, 0.60f, h), glm::mix(0.80f, 0.95f, h), t);
        float bVal = glm::mix(glm::mix(0.40f, 0.25f, h), glm::mix(0.20f, 0.05f, h), t);
        
        if (type == 7 || (type >= 31 && type <= 38)) { 
            // Leaves and foliage are slightly darker
            rVal *= 0.8f; gVal *= 0.8f; bVal *= 0.8f; 
            if (dir == 2 || dir == 3) { r = rVal; g = gVal; b = bVal; } 
            else { r = rVal * 0.9f; g = gVal * 0.9f; b = bVal * 0.9f; }
            a = 0.85f; // Tells vertex shader to apply wind sway!
        } else if (type == 2) { 
            // Grass - top face should always be tinted
            if (dir == 2) { r = rVal; g = gVal * 1.1f; b = bVal; } 
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
    
    if (type == 7 || (type >= 31 && type <= 38)) { // tint leaves, plants, kelp & flowers
        float t = glm::clamp(temp * 0.5f + 0.5f, 0.0f, 1.0f);
        float h = glm::clamp(hum * 0.5f + 0.5f, 0.0f, 1.0f);
        r = glm::mix(glm::mix(0.45f, 0.25f, h), glm::mix(0.65f, 0.15f, h), t) * 0.8f;
        g = glm::mix(glm::mix(0.60f, 0.50f, h), glm::mix(0.70f, 0.78f, h), t) * 0.8f;
        b = glm::mix(glm::mix(0.50f, 0.35f, h), glm::mix(0.25f, 0.15f, h), t) * 0.8f;
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
                    bool isCross = (type >= 31 && type <= 38); 
                    
                    if (dir == 0 && (isCross || type == 7)) {
                        // Add cross planes for plants/flowers OR internal depth planes for leaves
                        addCrossFace(x, y, z, type, tempMap[x][z], humMap[x][z]);
                    }
                    
                    if (isCross) continue;
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
void Chunk::growBranch(int bx, int by, int bz, glm::vec3 dir, float length, int depth, TreeType type, std::vector<std::pair<glm::ivec3, uint8_t>>& bks) {
    uint8_t wood = 6;
    uint8_t leaf = 7;
    if (type == TreeType::BIRCH) { wood = 39; leaf = 40; }
    else if (type == TreeType::PINE) { wood = 41; leaf = 42; }
    else if (type == TreeType::JUNGLE) { wood = 43; leaf = 44; }
    
    int steps = static_cast<int>(length * 2.0f);
    if (steps < 4) steps = 4;
    for (int i = 0; i < steps; ++i) {
        glm::vec3 p = dir * (length * (float)i / (float)steps);
        bks.push_back({{bx + (int)p.x, by + (int)p.y, bz + (int)p.z}, wood});
        // Extra thickness for main trunk
        if (length > 6.0f) {
            bks.push_back({{bx + (int)p.x + 1, by + (int)p.y, bz + (int)p.z}, wood});
            bks.push_back({{bx + (int)p.x, by + (int)p.y, bz + (int)p.z + 1}, wood});
        }
    }
    
    glm::vec3 end = dir * length;
    int ex = bx + (int)end.x;
    int ey = by + (int)end.y;
    int ez = bz + (int)end.z;

    // LEAF BALL
    int leafR = (depth == 0) ? 3 : 2;
    for (int dy = -leafR; dy <= leafR; ++dy) {
        for (int dx = -leafR; dx <= leafR; ++dx) {
            for (int dz = -leafR; dz <= leafR; ++dz) {
                if (dx*dx + dy*dy + dz*dz <= leafR * leafR + 1) {
                    bks.push_back({{ex + dx, ey + dy, ez + dz}, leaf});
                }
            }
        }
    }

    if (depth <= 0) return;

    // More branches for density
    int numBranches = 3 + (depth % 2);
    for (int i = 0; i < numBranches; ++i) {
        float angle = (float)i * 2.0f * 3.14159f / (float)numBranches;
        float r1 = ((Config::currentSeed + i) % 100) / 100.0f;
        
        glm::vec3 nextDir = glm::normalize(dir + glm::vec3(cos(angle) * 0.7f, 0.3f + r1 * 0.4f, sin(angle) * 0.7f));
        nextDir = glm::normalize(nextDir + glm::vec3(0, 0.5f, 0)); // Light seeking
        
        growBranch(ex, ey, ez, nextDir, length * 0.8f, depth - 1, type, bks);
    }
}
