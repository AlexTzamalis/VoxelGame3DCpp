#include "ChunkManager.hpp"
#include "WorldManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <GL/glew.h>

ChunkManager::ChunkManager() : isRunning_(true) {
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

        // 1. Base terrain height (Continents / Hills)
        FastNoiseLite localHeightNoise;
        localHeightNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        // Bind dynamic world seed!
        localHeightNoise.SetSeed(Config::currentSeed); 
        localHeightNoise.SetFrequency(0.005f);       
        localHeightNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        localHeightNoise.SetFractalOctaves(5);      

        // 2. Caverns - Natural FBm carving
        FastNoiseLite localCaveNoise;
        localCaveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        localCaveNoise.SetSeed(Config::currentSeed + 9999); 
        localCaveNoise.SetFrequency(0.03f); 
        localCaveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        localCaveNoise.SetFractalOctaves(3); 

        // Heavy Lifting off the main thread
        auto chunk = std::make_unique<Chunk>(taskPos);
        
        std::vector<uint8_t> savedData(Chunk::PADDED_SIZE * Chunk::PADDED_SIZE * Chunk::PADDED_SIZE);
        if (WorldManager::loadChunk(taskPos, savedData)) {
            chunk->setVoxels(savedData);
            chunk->isModified_ = true; // Still flag true so it persists safely
        } else {
            chunk->generateTerrain(localHeightNoise, localCaveNoise); 
        }
        
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
        std::floor(cameraPosition.y / Chunk::CHUNK_SIZE),
        std::floor(cameraPosition.z / Chunk::CHUNK_SIZE)
    );

    // Keep track of which chunks are valid and within render distance this frame
    std::unordered_map<glm::ivec3, bool, IVec3Hash> activeChunks;

    for (int x = -Config::renderDistance; x <= Config::renderDistance; ++x) {
        for (int z = -Config::renderDistance; z <= Config::renderDistance; ++z) {
            
            // Mathematically cull the corners of the grid to make drawing distance a beautiful circle!
            if (x * x + z * z > Config::renderDistance * Config::renderDistance) continue;
            
            for (int y = -Config::renderDistanceY; y <= Config::renderDistanceY; ++y) {
                // Hard floor/ceiling limits so we don't load memory infinitely up or deeply down below generation layer
                if (cameraChunkPos.y + y < -32) continue; // Allows up to Y:-512
                if (cameraChunkPos.y + y > 16) continue;  // Allows up to Y:256
                
                glm::ivec3 chunkPos = cameraChunkPos + glm::ivec3(x, y, z);
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
    }

    // Unload chunks outside radius
    for (auto it = chunks_.begin(); it != chunks_.end(); ) {
        if (activeChunks.find(it->first) == activeChunks.end()) {
            if (it->second->isModified_) {
                WorldManager::saveChunk(it->first, it->second->getVoxels());
            }
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }

    // Also prune generating requests (if player moved so fast we left range before it even started)
    // For now we can ignore pruning generating jobs to keep logic simple, they will just get instantly deleted next frame
}

void ChunkManager::render(unsigned int shaderProgram, const Camera& camera, bool bypassFrustum, float radialDistLimit) const {
    glm::vec3 camPos = camera.position();
    float limitSq = radialDistLimit * radialDistLimit;

    // 1. OPAQUE RENDER PASS
    for (const auto& [pos, chunk] : chunks_) {
        glm::vec3 minBound = glm::vec3(pos) * static_cast<float>(Chunk::CHUNK_SIZE);
        
        // Optimize shadow cascades dropping chunks drastically out of shadow camera radius
        if (radialDistLimit > 0.0f) {
            float dx = minBound.x - camPos.x;
            float dz = minBound.z - camPos.z;
            if (dx * dx + dz * dz > limitSq) continue;
        }

        glm::vec3 maxBound = minBound + glm::vec3(Chunk::CHUNK_SIZE);
        if (!bypassFrustum && Config::frustumCulling && !camera.isBoxInFrustum(minBound, maxBound)) continue; 
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, minBound);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        
        chunk->render();
    }
    
    // 2. TRANSPARENT RENDER PASS (Water & Leaves)
    for (const auto& [pos, chunk] : chunks_) {
        glm::vec3 minBound = glm::vec3(pos) * static_cast<float>(Chunk::CHUNK_SIZE);
        
        if (radialDistLimit > 0.0f) {
            float dx = minBound.x - camPos.x;
            float dz = minBound.z - camPos.z;
            if (dx * dx + dz * dz > limitSq) continue;
        }

        glm::vec3 maxBound = minBound + glm::vec3(Chunk::CHUNK_SIZE);
        if (!bypassFrustum && Config::frustumCulling && !camera.isBoxInFrustum(minBound, maxBound)) continue; 
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, minBound);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        
        chunk->renderTransparent();
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
        it->second->isModified_ = true;
        
        // Rebuild the mesh instantly on main thread for instantaneous player feedback
        it->second->generateMesh();
        it->second->updateBuffers();

        // Update padding voxel data on direct neighboring faces!
        auto checkNeighbor = [&](int dx, int dy, int dz) {
            glm::ivec3 nPos = cPos + glm::ivec3(dx, dy, dz);
            auto nIt = chunks_.find(nPos);
            if (nIt != chunks_.end()) {
                int nx = lx - dx * Chunk::CHUNK_SIZE;
                int ny = ly - dy * Chunk::CHUNK_SIZE;
                int nz = lz - dz * Chunk::CHUNK_SIZE;
                // Directly set the hidden padding face inside the adjacent chunk!
                nIt->second->setVoxel(nx, ny, nz, type);
                
                nIt->second->generateMesh();
                nIt->second->updateBuffers();
            }
        };

        if (lx == 0) checkNeighbor(-1, 0, 0);
        if (lx == Chunk::CHUNK_SIZE - 1) checkNeighbor(1, 0, 0);
        if (ly == 0) checkNeighbor(0, -1, 0);
        if (ly == Chunk::CHUNK_SIZE - 1) checkNeighbor(0, 1, 0);
        if (lz == 0) checkNeighbor(0, 0, -1);
        if (lz == Chunk::CHUNK_SIZE - 1) checkNeighbor(0, 0, 1);
    }
}

void ChunkManager::clear() {
    std::lock_guard<std::mutex> lock1(queueMutex_);
    std::lock_guard<std::mutex> lock2(readyMutex_);
    std::queue<glm::ivec3> emptyQueue;
    std::swap(pendingTasks_, emptyQueue);
    readyChunks_.clear();
    
    // Save any outstanding dirty chunks to disk as the world closes completely
    for(const auto& [pos, chunk] : chunks_) {
        if (chunk->isModified_) {
            WorldManager::saveChunk(pos, chunk->getVoxels());
        }
    }
    
    chunks_.clear();
    generatingChunks_.clear();
}

bool ChunkManager::isChunkColumnLoaded(int cx, int cz) const {
    // Check if at least the surface chunk (Y=0 to Y=4) is loaded
    for (int y = 0; y <= 4; ++y) {
        if (chunks_.find(glm::ivec3(cx, y, cz)) != chunks_.end()) {
            return true;
        }
    }
    return false;
}
