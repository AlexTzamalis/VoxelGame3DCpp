#pragma once

#include "Chunk.hpp"
#include "FastNoiseLite.h"
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

// Hash function for glm::ivec3 to use in std::unordered_map
struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager() = default;

    void update(const glm::vec3& cameraPosition);
    void render(unsigned int shaderProgram) const; // Needs shader to pass chunk matrices if not done in Main

private:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> chunks_;
    FastNoiseLite noise_;
    
    int renderDistance_ = 4; // Render a 9x9 chunk grid around the player (adjust for performance!)
};
