#include "ChunkManager.hpp"
#include "WorldManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <GL/glew.h>

ChunkColumn::ChunkColumn(glm::ivec2 pos) : position(pos) {}

ChunkColumn::~ChunkColumn() {
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    }
}

void ChunkColumn::updateBuffers(const std::vector<VoxelVertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<unsigned int>& transIndices) {
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VoxelVertex), vertices.data(), GL_STATIC_DRAW);
    
    std::vector<unsigned int> allIndices = indices;
    allIndices.insert(allIndices.end(), transIndices.begin(), transIndices.end());
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, allIndices.size() * sizeof(unsigned int), allIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(VoxelVertex), (void*)12);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)16);
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    
    indexCount = indices.size();
    transparentIndexCount = transIndices.size();
}

void ChunkColumn::render() const {
    if (indexCount > 0 && vao != 0) {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void ChunkColumn::renderTransparent() const {
    if (transparentIndexCount > 0 && vao != 0) {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, transparentIndexCount, GL_UNSIGNED_INT, (void*)(indexCount * sizeof(unsigned int)));
        glBindVertexArray(0);
    }
}

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

            taskPos = pendingTasks_.back();
            pendingTasks_.pop_back();
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
        glm::ivec3 pos = chunk->getPosition();
        glm::ivec2 colPos(pos.x, pos.z);
        
        if (columns_.find(colPos) == columns_.end()) {
            columns_[colPos] = std::make_unique<ChunkColumn>(colPos);
        }
        columns_[colPos]->needsUpdate = true;
        
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

    std::vector<glm::ivec3> newTasks;

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
                    newTasks.push_back(chunkPos);
                }
            }
        }
    }
    
    if (!newTasks.empty()) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingTasks_.insert(pendingTasks_.end(), newTasks.begin(), newTasks.end());
        // Sort DESCENDING so the closest chunks are placed at the back and popped first by worker threads
        std::sort(pendingTasks_.begin(), pendingTasks_.end(), [&](const glm::ivec3& a, const glm::ivec3& b) {
            float distA = (a.x - cameraChunkPos.x)*(a.x - cameraChunkPos.x) + (a.y - cameraChunkPos.y)*(a.y - cameraChunkPos.y) + (a.z - cameraChunkPos.z)*(a.z - cameraChunkPos.z);
            float distB = (b.x - cameraChunkPos.x)*(b.x - cameraChunkPos.x) + (b.y - cameraChunkPos.y)*(b.y - cameraChunkPos.y) + (b.z - cameraChunkPos.z)*(b.z - cameraChunkPos.z);
            return distA > distB;
        });
        cv_.notify_all();
    }

    // Unload chunks outside radius
    for (auto it = chunks_.begin(); it != chunks_.end(); ) {
        if (activeChunks.find(it->first) == activeChunks.end()) {
            if (it->second->isModified_) {
                WorldManager::saveChunk(it->first, it->second->getVoxels());
            }
            glm::ivec2 colPos(it->first.x, it->first.z);
            if (columns_.find(colPos) != columns_.end()) {
                columns_[colPos]->needsUpdate = true;
            }
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up empty columns and update modified columns
    for (auto it = columns_.begin(); it != columns_.end(); ) {
        if (it->second->needsUpdate) {
            std::vector<VoxelVertex> allVerts;
            std::vector<unsigned int> allInds;
            std::vector<unsigned int> allTrans;
            
            bool hasAnyChunk = false;
            for (int y = -32; y <= 16; ++y) {
                 glm::ivec3 pos(it->first.x, y, it->first.y);
                 auto cit = chunks_.find(pos);
                 if (cit != chunks_.end()) {
                      hasAnyChunk = true;
                      unsigned int baseVert = allVerts.size();
                      const auto& cVerts = cit->second->getVertices();
                      const auto& cInds = cit->second->getIndices();
                      const auto& cTrans = cit->second->getTransparentIndices();
                      
                      allVerts.insert(allVerts.end(), cVerts.begin(), cVerts.end());
                      for (unsigned int idx : cInds) allInds.push_back(idx + baseVert);
                      for (unsigned int idx : cTrans) allTrans.push_back(idx + baseVert);
                 }
            }
            
            if (!hasAnyChunk) {
                // Column is permanently dead, cull it.
                it = columns_.erase(it);
                continue;
            } else {
                it->second->updateBuffers(allVerts, allInds, allTrans);
                it->second->needsUpdate = false;
            }
        }
        ++it;
    }

    // Also prune generating requests (if player moved so fast we left range before it even started)
    // For now we can ignore pruning generating jobs to keep logic simple, they will just get instantly deleted next frame
}

void ChunkManager::render(unsigned int shaderProgram, const Camera& camera, bool bypassFrustum, float radialDistLimit) const {
    glm::vec3 camPos = camera.position();
    float limitSq = radialDistLimit * radialDistLimit;

    // Send global identity matrix once! 
    glm::mat4 globalModel = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(globalModel));

    // 1. OPAQUE RENDER PASS
    for (const auto& [pos, column] : columns_) {
        glm::vec3 minBound = glm::vec3(pos.x, -32.0f, pos.y) * static_cast<float>(Chunk::CHUNK_SIZE);
        
        // Optimize shadow cascades dropping chunks drastically out of shadow camera radius
        if (radialDistLimit > 0.0f) {
            float dx = (pos.x * Chunk::CHUNK_SIZE) - camPos.x;
            float dz = (pos.y * Chunk::CHUNK_SIZE) - camPos.z;
            if (dx * dx + dz * dz > limitSq) continue;
        }

        glm::vec3 maxBound = glm::vec3(pos.x + 1, 16.0f, pos.y + 1) * static_cast<float>(Chunk::CHUNK_SIZE);
        if (!bypassFrustum && Config::frustumCulling && !camera.isBoxInFrustum(minBound, maxBound)) continue; 
        
        column->render();
    }
    
    // 2. TRANSPARENT RENDER PASS (Water & Leaves)
    for (const auto& [pos, column] : columns_) {
        glm::vec3 minBound = glm::vec3(pos.x, -32.0f, pos.y) * static_cast<float>(Chunk::CHUNK_SIZE);
        
        if (radialDistLimit > 0.0f) {
            float dx = (pos.x * Chunk::CHUNK_SIZE) - camPos.x;
            float dz = (pos.y * Chunk::CHUNK_SIZE) - camPos.z;
            if (dx * dx + dz * dz > limitSq) continue;
        }

        glm::vec3 maxBound = glm::vec3(pos.x + 1, 16.0f, pos.y + 1) * static_cast<float>(Chunk::CHUNK_SIZE);
        if (!bypassFrustum && Config::frustumCulling && !camera.isBoxInFrustum(minBound, maxBound)) continue; 
        
        column->renderTransparent();
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
        columns_[glm::ivec2(cx, cz)]->needsUpdate = true;

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
                glm::ivec2 colPos(nPos.x, nPos.z);
                if (columns_.find(colPos) != columns_.end()) columns_[colPos]->needsUpdate = true;
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
    pendingTasks_.clear();
    readyChunks_.clear();
    
    // Save any outstanding dirty chunks to disk as the world closes completely
    for(const auto& [pos, chunk] : chunks_) {
        if (chunk->isModified_) {
            WorldManager::saveChunk(pos, chunk->getVoxels());
        }
    }
    
    chunks_.clear();
    columns_.clear();
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
