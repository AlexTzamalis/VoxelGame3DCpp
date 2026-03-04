#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "FastNoiseLite.h"

class Chunk {
public:
    static const int CHUNK_SIZE = 16;
    static const int PADDED_SIZE = 18; // Includes 1-block neighbor padding
    
    Chunk(glm::ivec3 position);
    ~Chunk();

    // Voxel access
    uint8_t getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, uint8_t type);

    // Initialization and updates
    void generateTerrain(FastNoiseLite& heightNoise, FastNoiseLite& caveNoise);
    void generateMesh();
    void updateBuffers();
    
    // Rendering
    void render() const;
    void renderTransparent() const;

    glm::ivec3 getPosition() const { return position_; }

private:
    int getIndex(int x, int y, int z) const;
    bool isFaceVisible(uint8_t currentType, int x, int y, int z, int dx, int dy, int dz) const;
    void addFace(int x, int y, int z, int dir, uint8_t type);

    glm::ivec3 position_;
    std::vector<uint8_t> voxels_;

    // Mesh data
    std::vector<float> vertices_;
    std::vector<unsigned int> indices_;
    std::vector<unsigned int> transparentIndices_;
    
    // OpenGL buffers
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    
    bool bufferNeedsUpdate_ = false;
    unsigned int indexCount_ = 0;
    unsigned int transparentIndexCount_ = 0;
};
