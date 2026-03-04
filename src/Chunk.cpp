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

void Chunk::generateTerrain(FastNoiseLite& noise) {
    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            float globalX = position_.x * CHUNK_SIZE + x;
            float globalZ = position_.z * CHUNK_SIZE + z;
            
            // FastNoiseLite inherently outputs roughly between [-1.0f, 1.0f]
            // We shift it up to 0->1, multiply it by 10 blocks vertical scale, and base it on 4 stone layers.
            float noiseHeight = noise.GetNoise(globalX, globalZ);
            int height = static_cast<int>((noiseHeight + 1.0f) * 0.5f * 10.0f) + 4;

            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                int globalY = position_.y * CHUNK_SIZE + y;
                
                // Solid ground level
                if (globalY < height) {
                    if (globalY == height - 1) {
                        setVoxel(x, y, z, 2); // Surface Grass
                    } else if (globalY > height - 4) {
                        setVoxel(x, y, z, 3); // Sub-Surface Dirt
                    } else {
                        setVoxel(x, y, z, 4); // Deep Stone
                    }
                } 
                // Any empty voxels below Sea Level (which is 6 blocks high globally) become Water
                else if (globalY <= 6) { 
                    setVoxel(x, y, z, 5); // Water
                } 
                // Actual atmospheric void
                else {
                    setVoxel(x, y, z, 1); // Transparent Air
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
    
    // Solid blocks MUST render their faces when touching transparent water
    // This prevents the user from seeing "through" the shore into the void!
    if (currentType != 5 && neighbor == 5) return true;
    
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
    
    if (type == 5) { // Route to correct index buffer
        waterIndices_.push_back(startIdx + 0);
        waterIndices_.push_back(startIdx + 1);
        waterIndices_.push_back(startIdx + 2);
        waterIndices_.push_back(startIdx + 0);
        waterIndices_.push_back(startIdx + 2);
        waterIndices_.push_back(startIdx + 3);
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
    waterIndices_.clear();

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
    waterIndexCount_ = waterIndices_.size();
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
    allIndices.insert(allIndices.end(), waterIndices_.begin(), waterIndices_.end());
    
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

void Chunk::renderWater() const {
    if (waterIndexCount_ > 0 && vao_ != 0) {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, waterIndexCount_, GL_UNSIGNED_INT, (void*)(indexCount_ * sizeof(unsigned int)));
        glBindVertexArray(0);
    }
}
