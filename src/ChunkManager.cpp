#include "ChunkManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <GL/glew.h>

ChunkManager::ChunkManager() : isRunning_(true) {
    // Configure FastNoiseLite for smooth, natural terrain
    noise_.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise_.SetSeed(1337); 
    noise_.SetFrequency(0.02f);       // Determines width / scale of hills
    noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise_.SetFractalOctaves(4);      // Detail levels

    // Launch worker threads
    int numThreads = std::thread::hardware_concurrency() - 1;
    if (numThreads <= 0) numThreads = 1;

    for (int i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ChunkManager::workerThreadFunc, this);
    }
}

ChunkManager::~ChunkManager() {
    isRunning_ = false;
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ChunkManager::workerThreadFunc() {
    while (isRunning_) {
        glm::ivec3 taskPos;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] { return !pendingTasks_.empty() || !isRunning_; });

            if (!isRunning_ && pendingTasks_.empty()) return;

            taskPos = pendingTasks_.front();
            pendingTasks_.pop();
        }

        // Heavy Lifting off the main thread
        auto chunk = std::make_unique<Chunk>(taskPos);
        chunk->generateTerrain(noise_); 
        chunk->generateMesh();

        // Push finished chunk back safely
        {
            std::lock_guard<std::mutex> lock(readyMutex_);
            readyChunks_.push_back(std::move(chunk));
        }
    }
}

void ChunkManager::update(const glm::vec3& cameraPosition) {
    // Consistently poll workers and upload to OpenGL strictly on MAIN thread
    std::vector<std::unique_ptr<Chunk>> finishedChunks;
    {
        std::lock_guard<std::mutex> lock(readyMutex_);
        finishedChunks.swap(readyChunks_);
    }

    for (auto& chunk : finishedChunks) {
        chunk->updateBuffers(); // Create VAO/VBO inside main OpenGL Context
        glm::ivec3 pos = chunk->getPosition();
        chunks_[pos] = std::move(chunk);
        generatingChunks_.erase(pos);
    }

    // Determine which chunk the camera is currently inside
    glm::ivec3 cameraChunkPos(
        std::floor(cameraPosition.x / Chunk::CHUNK_SIZE),
        0, // For now, we are locking generation to the Y=0 chunk layer only
        std::floor(cameraPosition.z / Chunk::CHUNK_SIZE)
    );

    // Keep track of which chunks are valid and within render distance this frame
    std::unordered_map<glm::ivec3, bool, IVec3Hash> activeChunks;

    for (int x = -Config::renderDistance; x <= Config::renderDistance; ++x) {
        for (int z = -Config::renderDistance; z <= Config::renderDistance; ++z) {
            glm::ivec3 chunkPos = cameraChunkPos + glm::ivec3(x, 0, z);
            activeChunks[chunkPos] = true;

            // If it's missing entirely (not loaded, and not generating currently)
            if (chunks_.find(chunkPos) == chunks_.end() && generatingChunks_.find(chunkPos) == generatingChunks_.end()) {
                generatingChunks_[chunkPos] = true; // Mark as started
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    pendingTasks_.push(chunkPos);
                }
                cv_.notify_one();
            }
        }
    }

    // Unload chunks outside radius
    for (auto it = chunks_.begin(); it != chunks_.end(); ) {
        if (activeChunks.find(it->first) == activeChunks.end()) {
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }

    // Also prune generating requests (if player moved so fast we left range before it even started)
    // For now we can ignore pruning generating jobs to keep logic simple, they will just get instantly deleted next frame
}

void ChunkManager::render(unsigned int shaderProgram, const Camera& camera) const {
    // Render every currently loaded chunk
    int chunksRendered = 0;
    for (const auto& [pos, chunk] : chunks_) {
        // Calculate the bounding box of the chunk
        glm::vec3 minBound = glm::vec3(pos) * static_cast<float>(Chunk::CHUNK_SIZE);
        glm::vec3 maxBound = minBound + glm::vec3(Chunk::CHUNK_SIZE);

        if (Config::frustumCulling && !camera.isBoxInFrustum(minBound, maxBound)) {
            continue; // Skip rendering totally!
        }
        
        chunksRendered++;

        // Calculate the world position of the chunk
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, minBound);
        
        // Pass model matrix to the active shader
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        
        chunk->render();
    }
}

uint8_t ChunkManager::getVoxelGlobal(int x, int y, int z) const {
    int cx = std::floor(static_cast<float>(x) / Chunk::CHUNK_SIZE);
    int cy = std::floor(static_cast<float>(y) / Chunk::CHUNK_SIZE);
    int cz = std::floor(static_cast<float>(z) / Chunk::CHUNK_SIZE);
    
    glm::ivec3 cPos(cx, cy, cz);
    auto it = chunks_.find(cPos);
    if (it != chunks_.end()) {
        int lx = x - cx * Chunk::CHUNK_SIZE;
        int ly = y - cy * Chunk::CHUNK_SIZE;
        int lz = z - cz * Chunk::CHUNK_SIZE;
        return it->second->getVoxel(lx, ly, lz);
    }
    return 0; // Air outside
}

void ChunkManager::setVoxelGlobal(int x, int y, int z, uint8_t type) {
    int cx = std::floor(static_cast<float>(x) / Chunk::CHUNK_SIZE);
    int cy = std::floor(static_cast<float>(y) / Chunk::CHUNK_SIZE);
    int cz = std::floor(static_cast<float>(z) / Chunk::CHUNK_SIZE);
    
    glm::ivec3 cPos(cx, cy, cz);
    auto it = chunks_.find(cPos);
    if (it != chunks_.end()) {
        int lx = x - cx * Chunk::CHUNK_SIZE;
        int ly = y - cy * Chunk::CHUNK_SIZE;
        int lz = z - cz * Chunk::CHUNK_SIZE;
        it->second->setVoxel(lx, ly, lz, type);
        
        // Rebuild the mesh instantly on main thread for instantaneous player feedback
        it->second->generateMesh();
        it->second->updateBuffers();
    }
}
