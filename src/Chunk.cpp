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
    voxels_.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 0);
    fillRandom();
}

Chunk::~Chunk() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ebo_);
    }
}

int Chunk::getIndex(int x, int y, int z) const {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

uint8_t Chunk::getVoxel(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return 0; // Air outside
    }
    return voxels_[getIndex(x, y, z)];
}

void Chunk::setVoxel(int x, int y, int z, uint8_t type) {
    if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
        voxels_[getIndex(x, y, z)] = type;
    }
}

void Chunk::fillRandom() {
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                if (y < CHUNK_SIZE / 2) {
                    setVoxel(x, y, z, 1); // 1 = Dirt/Grass
                } else if (y == CHUNK_SIZE / 2 && (x % 2 == 0 || z % 2 == 0)) {
                    setVoxel(x, y, z, 2); // 2 = Stone, for variation
                } else {
                    setVoxel(x, y, z, 0); // 0 = Air
                }
            }
        }
    }
}

bool Chunk::isFaceVisible(int x, int y, int z, int dx, int dy, int dz) const {
    int nx = x + dx;
    int ny = y + dy;
    int nz = z + dz;
    return getVoxel(nx, ny, nz) == 0;
}

void Chunk::addFace(int x, int y, int z, int dir, uint8_t type) {
    if (type == 0) return;
    
    int startIdx = vertices_.size() / 8; // 8 floats per vertex

    // Atlas mappings based on 16x16 block configurations
    // For type 1 (Dirt), UV col 1, row 15
    // For type 2 (Stone), UV col 2, row 15
    int tileX = type % 16;
    int tileY = 15 - (type / 16); 
    
    float u0 = tileX / 16.0f;
    float v0 = tileY / 16.0f;
    float u1 = (tileX + 1) / 16.0f;
    float v1 = (tileY + 1) / 16.0f;

    for (int i = 0; i < 4; ++i) {
        float vx = FACE_VERTICES[dir][i][0] + x;
        float vy = FACE_VERTICES[dir][i][1] + y;
        float vz = FACE_VERTICES[dir][i][2] + z;
        
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
    }
    
    indices_.push_back(startIdx + 0);
    indices_.push_back(startIdx + 1);
    indices_.push_back(startIdx + 2);
    indices_.push_back(startIdx + 0);
    indices_.push_back(startIdx + 2);
    indices_.push_back(startIdx + 3);
}

void Chunk::generateMesh() {
    vertices_.clear();
    indices_.clear();

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                uint8_t type = getVoxel(x, y, z);
                if (type != 0) {
                    for (int i = 0; i < 6; ++i) {
                        if (isFaceVisible(x, y, z, DIRS[i][0], DIRS[i][1], DIRS[i][2])) {
                            addFace(x, y, z, i, type);
                        }
                    }
                }
            }
        }
    }
    
    indexCount_ = indices_.size();
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
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), indices_.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
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
