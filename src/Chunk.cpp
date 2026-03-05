#include "Chunk.hpp"
#include <GL/glew.h>

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
        { 0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f }, // top-left
        { 0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f }, // bot-left
        { 0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f }, // bot-right
        { 0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f }  // top-right
    },
    // -X (Left)
    {
        {-0.5f,  0.5f, -0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 1.0f },
        {-0.5f, -0.5f, -0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f },
        {-0.5f, -0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   1.0f, 0.0f },
        {-0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   1.0f, 1.0f }
    },
    // +Y (Top)
    {
        {-0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f },
        {-0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f },
        { 0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f },
        { 0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f }
    },
    // -Y (Bottom)
    {
        {-0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f,   0.0f, 1.0f },
        {-0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f,   0.0f, 0.0f },
        { 0.5f, -0.5f, -0.5f,   0.0f,-1.0f, 0.0f,   1.0f, 0.0f },
        { 0.5f, -0.5f,  0.5f,   0.0f,-1.0f, 0.0f,   1.0f, 1.0f }
    },
    // +Z (Front)
    {
        {-0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f },
        {-0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f },
        { 0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f },
        { 0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f }
    },
    // -Z (Back)
    {
        { 0.5f,  0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   0.0f, 1.0f },
        { 0.5f, -0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   0.0f, 0.0f },
        {-0.5f, -0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   1.0f, 0.0f },
        {-0.5f,  0.5f, -0.5f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f }
    }
};

Chunk::Chunk(glm::ivec3 position) : position_(position) {
    voxels_.resize(PADDED_SIZE * PADDED_SIZE * PADDED_SIZE, 0);
    // Generation logic is now handled fully by ChunkManager via generateTerrain
}

Chunk::~Chunk() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ebo_);
    }
}

int Chunk::getIndex(int x, int y, int z) const {
    return (x + 1) + (y + 1) * PADDED_SIZE + (z + 1) * PADDED_SIZE * PADDED_SIZE;
}

uint8_t Chunk::getVoxel(int x, int y, int z) const {
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) {
        return 0; // Air outside
    }
    return voxels_[getIndex(x, y, z)];
}

void Chunk::setVoxel(int x, int y, int z, uint8_t type) {
    if (x >= -1 && x <= CHUNK_SIZE && y >= -1 && y <= CHUNK_SIZE && z >= -1 && z <= CHUNK_SIZE) {
        voxels_[getIndex(x, y, z)] = type;
    }
}

void Chunk::generateTerrain(FastNoiseLite& heightNoise, FastNoiseLite& caveNoise) {
    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            float globalX = position_.x * CHUNK_SIZE + x;
            float globalZ = position_.z * CHUNK_SIZE + z;
            
            float noiseVal = heightNoise.GetNoise(globalX, globalZ);
            
            // Push values through an exponent to make plains flatter and mountains beautifully taller!
            float n01 = (noiseVal + 1.0f) * 0.5f;
            n01 = std::pow(n01, 1.25f);
            
            // Height range: smooth continents going from 40 to ~130
            int height = static_cast<int>(n01 * 90.0f) + 40; 

            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int globalY = position_.y * CHUNK_SIZE + y;
                
                if (globalY < height) {
                    float caveVal = caveNoise.GetNoise(globalX, (float)globalY, globalZ);
                    
                    // Use a narrow band around 0 to create interconnected twisting tunnel caves!
                    float tunnelThreshold = 0.04f;
                    
                    // Decrease cave likelihood near the surface to prevent swiss cheese mountains
                    float depth = height - globalY;
                    if (depth < 15.0f) {
                        tunnelThreshold *= (depth / 15.0f); // Taper off the tunnel size near surface
                    }
                    
                    if (globalY > -95 && std::abs(caveVal) < tunnelThreshold) {
                        setVoxel(x, y, z, 1); // Hollow Air Cave
                    } else {
                        // Terrain placement (Dirt/Grass/Stone/Ores)
                        if (globalY == height - 1) {
                            if (globalY < 14) setVoxel(x, y, z, 3); // Dirt near water
                            else setVoxel(x, y, z, 2); // Grass
                        } else if (globalY > height - 4) {
                            setVoxel(x, y, z, 3); // Dirt
                        } else {
                            // Pseudo-random Ore (Type 8) vs Stone (Type 4)
                            if (globalY < 25 && ((int)globalX * 73 + (int)globalY * 31 + (int)globalZ * 17) % 100 < 3) {
                                setVoxel(x, y, z, 8); // Coal Ore
                            } else {
                                setVoxel(x, y, z, 4); // Deep Stone
                            }
                        }
                    }
                } 
                else if (globalY <= 63) {
                    setVoxel(x, y, z, 5); // Sea Level Water
                } 
                else {
                    setVoxel(x, y, z, 1); // Base Air
                    
                    // Forest System (Sparse, highly randomized Deterministic Trees)
                    // Better avalanche hash function to completely prevent linear "Tree Snakes"
                    uint32_t mixX = (uint32_t)std::abs((int)globalX);
                    uint32_t mixZ = (uint32_t)std::abs((int)globalZ);
                    uint32_t hashValue = (mixX * 374761393U ^ mixZ * 668265263U);
                    hashValue = (hashValue ^ (hashValue >> 13)) * 1274126177U;
                    
                    if (height > 63 && height < 100 && (hashValue % 1000) < 6) { // 0.6% chance of tree trunk per surface tile
                        if (globalY >= height && globalY <= height + 4) {
                            setVoxel(x, y, z, 6); // Wood Trunk
                            continue;
                        }
                    }
                    
                    // Tree Leaves Algorithm
                    bool isLeaf = false;
                    for(int dx = -2; dx <= 2 && !isLeaf; ++dx) {
                        for(int dz = -2; dz <= 2 && !isLeaf; ++dz) {
                            int tx = globalX + dx;
                            int tz = globalZ + dz;
                            uint32_t tMixX = (uint32_t)std::abs(tx);
                            uint32_t tMixZ = (uint32_t)std::abs(tz);
                            uint32_t tHash = (tMixX * 374761393U ^ tMixZ * 668265263U);
                            tHash = (tHash ^ (tHash >> 13)) * 1274126177U;
                            
                            if ((tHash % 1000) < 6) {
                                float tNoise = heightNoise.GetNoise((float)tx, (float)tz);
                                float tn01 = std::pow((tNoise + 1.0f) * 0.5f, 1.25f);
                                int tHeight = static_cast<int>(tn01 * 90.0f) + 40; 
                                if (tHeight > 63) {
                                    int dy = globalY - tHeight;
                                    if (dy >= 3 && dy <= 5) { // Leaves populate height 3 to 5 above the trunk base
                                        if (dy == 5 && (std::abs(dx) == 2 || std::abs(dz) == 2)) continue; // Round off top
                                        isLeaf = true;
                                    }
                                }
                            }
                        }
                    }
                    if (isLeaf) {
                        setVoxel(x, y, z, 7); // Leaf Block
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
    
    // Always render faces exposed to air/void
    if (neighbor == 0 || neighbor == 1) return true;
    
    // Identify transparent geometry (Water=5, Leaves=7)
    bool isCurrentTransparent = (currentType == 5 || currentType == 7);
    bool isNeighborTransparent = (neighbor == 5 || neighbor == 7);
    
    // Solid blocks MUST render their faces when touching transparent blocks (fix void clipping)
    if (!isCurrentTransparent && isNeighborTransparent) return true;
    
    // Two different transparent blocks touching (e.g. Water vs Leaves should show border between them!)
    if (isCurrentTransparent && isNeighborTransparent && currentType != neighbor) return true;
    
    return false;
}

void Chunk::addFace(int x, int y, int z, int dir, uint8_t type) {
    if (type <= 1) return; // Ignore air blocks
    
    int startIdx = vertices_.size() / 12; // 12 floats per vertex (Pos, Norm, UV, Color+Alpha)

    int tileX = 0;
    int tileY = 15;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    if (type == 2) { // Grass Block
        if (dir == 2) { // +Y (Top Face)
            tileX = 0; // 1st item in 2nd row (Grass Top Grayscale)
            tileY = 14;
            r = 0.35f; g = 0.75f; b = 0.25f; // Vibrant biome green tint
        } else if (dir == 3) { // -Y (Bottom Face)
            tileX = 2; // Dirt (3rd item in 1st row)
            tileY = 15;
        } else { // Sides
            tileX = 1; // Grass Side (2nd item in 1st row)
            tileY = 15;
        }
    } else if (type == 3) { // Dirt
        tileX = 2;
        tileY = 15;
    } else if (type == 4) { // Stone
        tileX = 3;
        tileY = 15;
    } else if (type == 5) { // Water
        tileX = 1; // 2nd item in 2nd row
        tileY = 14;
        a = 0.7f; // Transparent opacity!
    } else if (type == 6) { // Wood Trunk
        tileX = 3; // Placeholder uses Stone pattern
        tileY = 15;
        r = 0.45f; g = 0.3f; b = 0.15f; // Deep Brown Tint
    } else if (type == 7) { // Leaves
        tileX = 0; // Placeholder uses Grayscale Grass Top 
        tileY = 14;
        r = 0.2f; g = 0.7f; b = 0.2f; // Very bright green foliage
        a = 0.85f; // Partially transparent geometry
    } else if (type == 8) { // Coal Ore Placeholder
        tileX = 3; // Stone
        tileY = 15;
        r = 0.2f; g = 0.2f; b = 0.2f; // Dark Coal color
    } else {
        tileX = type % 16;
        tileY = 15 - (type / 16);
    }
    
    float u0 = tileX / 16.0f;
    float v0 = tileY / 16.0f;
    float u1 = (tileX + 1) / 16.0f;
    float v1 = (tileY + 1) / 16.0f;

    for (int i = 0; i < 4; ++i) {
        float vx = FACE_VERTICES[dir][i][0] + x;
        float vy = FACE_VERTICES[dir][i][1] + y;
        float vz = FACE_VERTICES[dir][i][2] + z;
        
        // Lower the mesh height of water top faces to separate it elegantly from shores
        if (type == 5 && FACE_VERTICES[dir][i][1] > 0.0f) {
            vy -= 0.15f;
        }
        
        // Shrink leaves very slightly to make the blocks feel more organic and rounded
        if (type == 7) {
            vx -= FACE_VERTICES[dir][i][3] * 0.05f;
            vy -= FACE_VERTICES[dir][i][4] * 0.05f;
            vz -= FACE_VERTICES[dir][i][5] * 0.05f;
        }
        
        float nx = FACE_VERTICES[dir][i][3];
        float ny = FACE_VERTICES[dir][i][4];
        float nz = FACE_VERTICES[dir][i][5];
        
        float bu = FACE_VERTICES[dir][i][6];
        float bv = FACE_VERTICES[dir][i][7];
        
        float u = bu == 0.0f ? u0 : u1;
        float v = bv == 0.0f ? v0 : v1;

        vertices_.push_back(vx);
        vertices_.push_back(vy);
        vertices_.push_back(vz);
        vertices_.push_back(nx);
        vertices_.push_back(ny);
        vertices_.push_back(nz);
        vertices_.push_back(u);
        vertices_.push_back(v);
        vertices_.push_back(r);
        vertices_.push_back(g);
        vertices_.push_back(b);
        vertices_.push_back(a);
    }
    
    if (type == 5 || type == 7) { // Route to transparent index buffer
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

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                uint8_t type = getVoxel(x, y, z);
                if (type > 1) {
                    for (int i = 0; i < 6; ++i) {
                        if (isFaceVisible(type, x, y, z, DIRS[i][0], DIRS[i][1], DIRS[i][2])) {
                            addFace(x, y, z, i, type);
                        }
                    }
                }
            }
        }
    }
    
    indexCount_ = indices_.size();
    transparentIndexCount_ = transparentIndices_.size();
    bufferNeedsUpdate_ = true;
}

void Chunk::updateBuffers() {
    if (!bufferNeedsUpdate_) return;
    
    if (vao_ == 0) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
    }

    glBindVertexArray(vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(float), vertices_.data(), GL_STATIC_DRAW);
    
    // Upload both sets sequentially
    std::vector<unsigned int> allIndices = indices_;
    allIndices.insert(allIndices.end(), transparentIndices_.begin(), transparentIndices_.end());
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, allIndices.size() * sizeof(unsigned int), allIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);
    
    bufferNeedsUpdate_ = false;
}

void Chunk::render() const {
    if (indexCount_ > 0 && vao_ != 0) {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Chunk::renderTransparent() const {
    if (transparentIndexCount_ > 0 && vao_ != 0) {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, transparentIndexCount_, GL_UNSIGNED_INT, (void*)(indexCount_ * sizeof(unsigned int)));
        glBindVertexArray(0);
    }
}
