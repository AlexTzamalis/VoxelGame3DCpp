#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "world/FastNoiseLite.h"
#include "world/Biome.hpp"

#pragma pack(push, 1)
struct VoxelVertex {
    float x, y, z;
    uint32_t data;
    uint8_t r, g, b, a;
};
#pragma pack(pop)

class Chunk {
public:
    static const int CHUNK_SIZE = 16;
    static const int PADDED_SIZE = 18; // Includes 1-block neighbor padding
    
    Chunk(glm::ivec3 position);
    ~Chunk();

    // Voxel access
    uint8_t getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, uint8_t type);
    
    // Serialization Access
    const std::vector<uint8_t>& getVoxels() const { return voxels_; }
    void setVoxels(const std::vector<uint8_t>& voxels) { voxels_ = voxels; }
    
    bool isModified_ = false;

    void generateTerrain(FastNoiseLite& tectonic, FastNoiseLite& erosion, FastNoiseLite& temp, FastNoiseLite& hum, FastNoiseLite& density, FastNoiseLite& cave, FastNoiseLite& cluster);
    void generateTerrainV2(FastNoiseLite& tectonic, FastNoiseLite& erosion, FastNoiseLite& temp, FastNoiseLite& hum, FastNoiseLite& density, FastNoiseLite& cave, FastNoiseLite& cluster);
    void generateMesh();
    // Mesh data access for ChunkColumn
    const std::vector<VoxelVertex>& getVertices() const { return vertices_; }
    const std::vector<unsigned int>& getIndices() const { return indices_; }
    const std::vector<unsigned int>& getTransparentIndices() const { return transparentIndices_; }

    glm::ivec3 getPosition() const { return position_; }

private:
    int getIndex(int x, int y, int z) const;
    bool isFaceVisible(uint8_t currentType, int x, int y, int z, int dx, int dy, int dz) const;
    void addFace(int x, int y, int z, int dir, uint8_t type, int width, int height, float temp = 0.5f, float hum = 0.5f);
    void addCrossFace(int x, int y, int z, uint8_t type, float temp = 0.5f, float hum = 0.5f);
    void growBranch(int bx, int by, int bz, glm::vec3 dir, float length, int depth, TreeType type, std::vector<std::pair<glm::ivec3, uint8_t>>& bks);

    glm::ivec3 position_;
    std::vector<uint8_t> voxels_;

    std::vector<VoxelVertex> vertices_;
    std::vector<unsigned int> indices_;
    std::vector<unsigned int> transparentIndices_;
};
