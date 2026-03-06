#include "ChunkManager.hpp"
#include "WorldManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <unordered_set>
#include <chrono>
#include <GL/glew.h>

ChunkManager::ChunkManager() : isRunning_(true) {
    glGenVertexArrays(1, &mdiVAO_);
    glGenBuffers(1, &mdiVBO_);
    glGenBuffers(1, &mdiEBO_);
    glGenBuffers(1, &mdiIndirectBufferOpaque_);
    glGenBuffers(1, &mdiIndirectBufferTrans_);

    glBindVertexArray(mdiVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, mdiVBO_);
    glBufferData(GL_ARRAY_BUFFER, 40000000 * sizeof(VoxelVertex), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdiEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 60000000 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(VoxelVertex), (void*)12);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VoxelVertex), (void*)16);
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);

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

void ChunkManager::defragmentVRAM() {
    currentVertexOffset_ = 0;
    currentIndexOffset_ = 0;

    glBindBuffer(GL_ARRAY_BUFFER, mdiVBO_);
    glBufferData(GL_ARRAY_BUFFER, 40000000 * sizeof(VoxelVertex), nullptr, GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdiEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 60000000 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    for (auto& [pos, col] : columns_) {
        // If CPU data was freed, rebuild it from chunks
        if (col->vertices.empty() && col->opaqueCount > 0) {
            std::vector<VoxelVertex> allVerts;
            std::vector<unsigned int> allInds;
            std::vector<unsigned int> allTrans;
            for (int y = -32; y <= 16; ++y) {
                glm::ivec3 cpos(pos.x, y, pos.y);
                auto cit = chunks_.find(cpos);
                if (cit != chunks_.end()) {
                    unsigned int baseVert = allVerts.size();
                    const auto& cv = cit->second->getVertices();
                    const auto& ci = cit->second->getIndices();
                    const auto& ct = cit->second->getTransparentIndices();
                    allVerts.insert(allVerts.end(), cv.begin(), cv.end());
                    for (unsigned int idx : ci) allInds.push_back(idx + baseVert);
                    for (unsigned int idx : ct) allTrans.push_back(idx + baseVert);
                }
            }
            col->opaqueCount = allInds.size();
            col->transparentCount = allTrans.size();
            col->vertices = std::move(allVerts);
            col->indices = std::move(allInds);
            col->transparentIndices = std::move(allTrans);
        }
        
        if (col->vertices.empty()) {
            col->inVRAM = true;
            continue;
        }
        
        col->vertexOffset = currentVertexOffset_;
        col->indexOffset = currentIndexOffset_;
                
        glBufferSubData(GL_ARRAY_BUFFER, col->vertexOffset * sizeof(VoxelVertex), col->vertices.size() * sizeof(VoxelVertex), col->vertices.data());
                
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, col->indexOffset * sizeof(unsigned int), col->indices.size() * sizeof(unsigned int), col->indices.data());
        
        col->transparentIndexOffset = currentIndexOffset_ + col->indices.size();
        if (!col->transparentIndices.empty()) {
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, col->transparentIndexOffset * sizeof(unsigned int), col->transparentIndices.size() * sizeof(unsigned int), col->transparentIndices.data());
        }

        currentVertexOffset_ += col->vertices.size();
        currentIndexOffset_ += col->indices.size() + col->transparentIndices.size();
        col->allocatedVerts = col->vertices.size();
        col->allocatedInds = col->indices.size() + col->transparentIndices.size();
        col->inVRAM = true;
        
        // Free CPU data after defrag upload too
        col->vertices.clear();
        col->vertices.shrink_to_fit();
        col->indices.clear();
        col->indices.shrink_to_fit();
        col->transparentIndices.clear();
        col->transparentIndices.shrink_to_fit();
    }
}

void ChunkManager::update(const glm::vec3& cameraPosition) {
    // Process ALL ready chunks - mesh gen already happened off-thread, this is just pointer moves
    {
        std::lock_guard<std::mutex> lock(readyMutex_);
        for (auto& chunk : readyChunks_) {
            glm::ivec3 pos = chunk->getPosition();
            glm::ivec2 colPos(pos.x, pos.z);
            
            if (columns_.find(colPos) == columns_.end()) {
                columns_[colPos] = std::make_unique<ChunkColumn>(colPos);
            }
            columns_[colPos]->needsUpdate = true;
            
            chunks_[pos] = std::move(chunk);
            generatingChunks_.erase(pos);
        }
        readyChunks_.clear();
    }

    // Determine which chunk the camera is currently inside
    glm::ivec3 cameraChunkPos(
        std::floor(cameraPosition.x / Chunk::CHUNK_SIZE),
        std::floor(cameraPosition.y / Chunk::CHUNK_SIZE),
        std::floor(cameraPosition.z / Chunk::CHUNK_SIZE)
    );

    // CRITICAL OPTIMIZATION: Only run the expensive O(n³) scan when camera moves to a new chunk
    if (cameraChunkPos != lastScanChunkPos_) {
        lastScanChunkPos_ = cameraChunkPos;
        
        // Build active set for unloading
        std::unordered_set<glm::ivec3, IVec3Hash> activeKeys;
        std::vector<glm::ivec3> newTasks;

        for (int x = -Config::renderDistance; x <= Config::renderDistance; ++x) {
            for (int z = -Config::renderDistance; z <= Config::renderDistance; ++z) {
                if (x * x + z * z > Config::renderDistance * Config::renderDistance) continue;
                
                for (int y = -Config::renderDistanceY; y <= Config::renderDistanceY; ++y) {
                    int gy = cameraChunkPos.y + y;
                    if (gy < -32 || gy > 16) continue;
                    
                    glm::ivec3 chunkPos = cameraChunkPos + glm::ivec3(x, y, z);
                    activeKeys.insert(chunkPos);

                    if (chunks_.find(chunkPos) == chunks_.end() && generatingChunks_.find(chunkPos) == generatingChunks_.end()) {
                        generatingChunks_[chunkPos] = true;
                        newTasks.push_back(chunkPos);
                    }
                }
            }
        }
        
        if (!newTasks.empty()) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingTasks_.insert(pendingTasks_.end(), newTasks.begin(), newTasks.end());
            // Sort by distance to prioritize nearby chunks
            std::sort(pendingTasks_.begin(), pendingTasks_.end(), [&](const glm::ivec3& a, const glm::ivec3& b) {
                float distA = glm::distance(glm::vec3(a), glm::vec3(cameraChunkPos));
                float distB = glm::distance(glm::vec3(b), glm::vec3(cameraChunkPos));
                return distA > distB; // Back of vector is processed first
            });
            cv_.notify_all();
        }

        // Unload chunks outside radius
        for (auto it = chunks_.begin(); it != chunks_.end(); ) {
            if (activeKeys.find(it->first) == activeKeys.end()) {
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
    }

    // Column rebuilds - time-budgeted to prevent stutter
    auto frameStart = std::chrono::high_resolution_clock::now();
    const auto frameBudget = std::chrono::microseconds(4000); // 4ms max for rebuilds+uploads
    
    for (auto it = columns_.begin(); it != columns_.end(); ) {
        if (it->second->needsUpdate) {
            auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
            if (elapsed > frameBudget) break; // Defer remaining to next frame
            
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
                it = columns_.erase(it);
                mdiCommandsDirty_ = true;
                continue;
            } else {
                it->second->opaqueCount = allInds.size();
                it->second->transparentCount = allTrans.size();
                it->second->vertices = std::move(allVerts);
                it->second->indices = std::move(allInds);
                it->second->transparentIndices = std::move(allTrans);
                it->second->needsUpdate = false;
                it->second->inVRAM = false;
            }
        }
        ++it;
    }

    // VRAM uploads - reuse old slots when possible to avoid fragmentation
    auto uploadStart = std::chrono::high_resolution_clock::now();
    auto totalElapsed = uploadStart - frameStart;
    if (totalElapsed < frameBudget) {
        glBindBuffer(GL_ARRAY_BUFFER, mdiVBO_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdiEBO_);
        
        for (auto& [pos, col] : columns_) {
            if (!col->inVRAM && !col->vertices.empty()) {
                auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
                if (elapsed > frameBudget) break;
                
                size_t newVertCount = col->vertices.size();
                size_t newIndCount = col->indices.size() + col->transparentIndices.size();
                
                bool canReuse = (col->allocatedVerts > 0 && 
                                 newVertCount <= col->allocatedVerts && 
                                 newIndCount <= col->allocatedInds);
                
                if (canReuse) {
                    // Overwrite in-place — no offset change, no fragmentation!
                    glBufferSubData(GL_ARRAY_BUFFER, col->vertexOffset * sizeof(VoxelVertex), 
                                    newVertCount * sizeof(VoxelVertex), col->vertices.data());
                    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, col->indexOffset * sizeof(unsigned int), 
                                    col->indices.size() * sizeof(unsigned int), col->indices.data());
                    
                    col->transparentIndexOffset = col->indexOffset + col->indices.size();
                    if (!col->transparentIndices.empty()) {
                        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, col->transparentIndexOffset * sizeof(unsigned int), 
                                        col->transparentIndices.size() * sizeof(unsigned int), col->transparentIndices.data());
                    }
                } else {
                    // Need new slot — check if we have room to append
                    if (currentVertexOffset_ + newVertCount >= 40000000 || 
                        currentIndexOffset_ + newIndCount >= 60000000) {
                        // Out of space — trigger defrag and break
                        defragmentVRAM();
                        mdiCommandsDirty_ = true;
                        break;
                    }
                    
                    col->vertexOffset = currentVertexOffset_;
                    col->indexOffset = currentIndexOffset_;
                    
                    glBufferSubData(GL_ARRAY_BUFFER, col->vertexOffset * sizeof(VoxelVertex), 
                                    newVertCount * sizeof(VoxelVertex), col->vertices.data());
                    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, col->indexOffset * sizeof(unsigned int), 
                                    col->indices.size() * sizeof(unsigned int), col->indices.data());
                    
                    col->transparentIndexOffset = col->indexOffset + col->indices.size();
                    if (!col->transparentIndices.empty()) {
                        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, col->transparentIndexOffset * sizeof(unsigned int), 
                                        col->transparentIndices.size() * sizeof(unsigned int), col->transparentIndices.data());
                    }
                    
                    currentVertexOffset_ += newVertCount;
                    currentIndexOffset_ += newIndCount;
                    col->allocatedVerts = newVertCount;
                    col->allocatedInds = newIndCount;
                }
                
                col->opaqueCount = col->indices.size();
                col->transparentCount = col->transparentIndices.size();
                col->inVRAM = true;
                mdiCommandsDirty_ = true;
                
                // Free CPU-side mesh data after VRAM upload
                col->vertices.clear();
                col->vertices.shrink_to_fit();
                col->indices.clear();
                col->indices.shrink_to_fit();
                col->transparentIndices.clear();
                col->transparentIndices.shrink_to_fit();
            }
        }
    }
}

void ChunkManager::render(unsigned int shaderProgram, const Camera& camera, bool bypassFrustum, float radialDistLimit) const {
    glm::vec3 camPos = camera.position();
    float limitSq = radialDistLimit * radialDistLimit;

    // Send global identity matrix once! 
    glm::mat4 globalModel = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(globalModel));

    // Rebuild cached MDI commands only when dirty
    if (mdiCommandsDirty_) {
        cachedOpaqueCmds_.clear();
        cachedTransCmds_.clear();
        cachedOpaqueCmds_.reserve(columns_.size());
        cachedTransCmds_.reserve(columns_.size());

        for (const auto& [pos, column] : columns_) {
            if (!column->inVRAM) continue;
            
            if (column->opaqueCount > 0) {
                DrawElementsIndirectCommand cmd;
                cmd.count = column->opaqueCount;
                cmd.instanceCount = 1;
                cmd.firstIndex = column->indexOffset;
                cmd.baseVertex = column->vertexOffset;
                cmd.baseInstance = 0;
                cachedOpaqueCmds_.push_back(cmd);
            }
            
            if (column->transparentCount > 0) {
                DrawElementsIndirectCommand cmd;
                cmd.count = column->transparentCount;
                cmd.instanceCount = 1;
                cmd.firstIndex = column->transparentIndexOffset;
                cmd.baseVertex = column->vertexOffset;
                cmd.baseInstance = 0;
                cachedTransCmds_.push_back(cmd);
            }
        }
        mdiCommandsDirty_ = false;
    }

    glBindVertexArray(mdiVAO_);
    
    if (!cachedOpaqueCmds_.empty()) {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mdiIndirectBufferOpaque_);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, cachedOpaqueCmds_.size() * sizeof(DrawElementsIndirectCommand), cachedOpaqueCmds_.data(), GL_DYNAMIC_DRAW);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, cachedOpaqueCmds_.size(), 0);
    }
    
    if (!cachedTransCmds_.empty()) {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mdiIndirectBufferTrans_);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, cachedTransCmds_.size() * sizeof(DrawElementsIndirectCommand), cachedTransCmds_.data(), GL_DYNAMIC_DRAW);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, cachedTransCmds_.size(), 0);
    }
    
    glBindVertexArray(0);
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
        mdiCommandsDirty_ = true;

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
    
    currentVertexOffset_ = 0;
    currentIndexOffset_ = 0;
    lastScanChunkPos_ = glm::ivec3(999999);
    mdiCommandsDirty_ = true;
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
