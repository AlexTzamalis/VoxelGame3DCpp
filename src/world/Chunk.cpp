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
            float tempNoise = heightNoise.GetNoise(globalZ * 0.4f - 10000.0f, globalX * 0.4f + 10000.0f);
            
            const Biome* biome = BiomeManager::getBiome(noiseVal, tempNoise);
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

bool Chunk::isFaceVisible(uint8_t currentType, int x, int y, int z, int dx, int dy, int dz) const {
    int nx = x + dx;
    int ny = y + dy;
    int nz = z + dz;
    uint8_t neighbor = getVoxel(nx, ny, nz);
    
    if (neighbor == 0 || neighbor == 1) return true;
    
    bool isCurrentTransparent = (currentType == 5 || currentType == 7);
    bool isNeighborTransparent = (neighbor == 5 || neighbor == 7);
    
    if (!isCurrentTransparent && isNeighborTransparent) return true;
    if (isCurrentTransparent && isNeighborTransparent && currentType != neighbor) return true;
    
    return false;
}

void Chunk::addFace(int x, int y, int z, int dir, uint8_t type, int width, int height) {
    if (type <= 1) return; 
    
    int startIdx = vertices_.size();

    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    if (type == 2) { 
        if (dir == 2) { r = 0.35f; g = 0.75f; b = 0.25f; } 
    } else if (type == 5) { 
        a = 0.7f; 
    } else if (type == 6) { 
        r = 0.65f; g = 0.5f; b = 0.35f; 
    } else if (type == 7) { 
        r = 0.45f; g = 0.75f; b = 0.45f; 
        a = 1.0f; 
    } else if (type == 11) { 
        if (dir == 2) { r = 0.95f; g = 0.95f; b = 0.95f; } 
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
    
    if (type == 5 || type == 7) { 
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

void Chunk::generateMesh() {
    vertices_.clear();
    indices_.clear();
    transparentIndices_.clear();

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

                        addFace(x, y, z, dir, type, width, height);

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
