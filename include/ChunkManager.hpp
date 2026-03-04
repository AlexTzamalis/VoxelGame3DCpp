#pragma once

#include "Camera.hpp"
#include "Chunk.hpp"
#include "Config.hpp"
#include "FastNoiseLite.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <glm/glm.hpp>
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
    ~ChunkManager();

    void update(const glm::vec3& cameraPosition);
    void render(unsigned int shaderProgram, const Camera& camera) const;

private:
    void workerThreadFunc();

    // Contains fully loaded and rendered chunks
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> chunks_;

    // Keeps track of chunks currently being generated so we don't start duplicate jobs
    std::unordered_map<glm::ivec3, bool, IVec3Hash> generatingChunks_;

    FastNoiseLite noise_;
    
    // Multi-Threading Data Structures
    std::vector<std::thread> workers_;
    std::queue<glm::ivec3> pendingTasks_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> isRunning_;

    // Chunks returning from workers that need buffer uploads
    std::vector<std::unique_ptr<Chunk>> readyChunks_;
    std::mutex readyMutex_;
};
