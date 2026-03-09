#include "world/ChunkManager.hpp"
#include "world/WorldManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
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
        if (taskPos.y == 999999) {
            // Background LOD Generation
            auto lodCol = generateLODColumn(glm::ivec2(taskPos.x, taskPos.z));
            {
                std::lock_guard<std::mutex> lock(readyMutex_);
                readyLODColumns_.push_back(std::move(lodCol));
            }
            continue;
        }

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
            for (int y = -16; y <= 48; ++y) {
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

void ChunkManager::update(const Camera& camera) {
    glm::vec3 cameraPosition = camera.getRenderPosition();
    
    // Dynamic MDI refresh for frustum culling when camera moves/rotates
    static glm::vec3 lastCamPos = cameraPosition;
    static float lastYaw = camera.yaw();
    if (glm::distance(cameraPosition, lastCamPos) > 0.1f || std::abs(lastYaw - camera.yaw()) > 0.05f) {
        mdiCommandsDirty_ = true;
        lastCamPos = cameraPosition;
        lastYaw = camera.yaw();
    }

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
        
        for (auto& lod : readyLODColumns_) {
            glm::ivec2 pos = lod->position;
            lodColumns_[pos] = std::move(lod);
            lodColumns_[pos]->needsUpdate = false; 
            lodColumns_[pos]->inVRAM = false;      
            generatingLODs_.erase(pos);
            mdiCommandsDirty_ = true;
        }
        readyLODColumns_.clear();
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
                // Generate FULL vertical columns to prevent void gaps and preserve bedrock/mountains
                for (int gy = -8; gy <= 28; ++gy) {
                    glm::ivec3 chunkPos = glm::ivec3(cameraChunkPos.x + x, gy, cameraChunkPos.z + z);
                    activeKeys.insert(chunkPos);

                    if (chunks_.find(chunkPos) == chunks_.end() && generatingChunks_.find(chunkPos) == generatingChunks_.end()) {
                        generatingChunks_[chunkPos] = true;
                        newTasks.push_back(chunkPos);
                    }
                }
            }
        }
        
        // Queue LOD Tasks
        // Spiral Search for LODs (Faster & better prioritization)
        if (Config::enableLOD) {
            int maxRadius = Config::renderDistance + Config::lodDistance;
            int renderDistSq = Config::renderDistance * Config::renderDistance;
            
            // Increase budget for heavy distance worlds
            int lodCount = 0;
            int lodBudget = 2000;
            int lodLimitSq = maxRadius * maxRadius;
            for (int r = Config::renderDistance + 1; r <= maxRadius && lodCount < lodBudget; ++r) {
                for (int x = -r; x <= r && lodCount < lodBudget; ++x) {
                    for (int z = -r; z <= r && lodCount < lodBudget; ++z) {
                        if (x*x + z*z < (r-1)*(r-1) || x*x + z*z > r*r) continue; 
                        if (x*x + z*z > lodLimitSq) continue;
                        if (x*x + z*z <= renderDistSq) continue;
                        
                        glm::ivec2 lodPos = glm::ivec2(cameraChunkPos.x + x, cameraChunkPos.z + z);
                        if (lodColumns_.find(lodPos) == lodColumns_.end() && generatingLODs_.find(lodPos) == generatingLODs_.end()) {
                            generatingLODs_[lodPos] = true;
                            newTasks.push_back(glm::ivec3(lodPos.x, 999999, lodPos.y)); 
                            lodCount++;
                        }
                    }
                }
            }
        }
        
        if (!newTasks.empty()) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            // Efficient prioritization: Only sort the new batch to find closest ones
            std::sort(newTasks.begin(), newTasks.end(), [&](const glm::ivec3& a, const glm::ivec3& b) {
                glm::vec3 pa = (a.y == 999999) ? glm::vec3(a.x, 64, a.z) : glm::vec3(a);
                glm::vec3 pb = (b.y == 999999) ? glm::vec3(b.x, 64, b.z) : glm::vec3(b);
                float distA = glm::distance(pa, cameraPosition);
                float distB = glm::distance(pb, cameraPosition);
                return distA > distB; // Closest at the back
            });
            pendingTasks_.insert(pendingTasks_.end(), newTasks.begin(), newTasks.end());
            cv_.notify_all();
        }

        // Unload chunks outside radius WITH HYSTERESIS to stop flickering
        int thresholdH2 = (Config::renderDistance + 1) * (Config::renderDistance + 1);
        int thresholdY = Config::renderDistanceY + 1;

        for (auto it = chunks_.begin(); it != chunks_.end(); ) {
            int dx = it->first.x - cameraChunkPos.x;
            int dz = it->first.z - cameraChunkPos.z;
            int py = it->first.y;

            // Only unload if X/Z is outside radius, OR if vertical is ridiculously outside generated bounds
            if (dx*dx + dz*dz > thresholdH2 || py < -12 || py > 32) {
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
        
        // Unload far LODs
        if (Config::enableLOD) {
            int lodDistLimit = Config::renderDistance + Config::lodDistance + 4;
            int lodThresholdLimitSq = lodDistLimit * lodDistLimit;
            for (auto it = lodColumns_.begin(); it != lodColumns_.end(); ) {
                int dx = it->first.x - cameraChunkPos.x;
                int dz = it->first.y - cameraChunkPos.z;
                // Only unload if truly outside the radius
                if (dx*dx + dz*dz > lodThresholdLimitSq) {
                    mdiCommandsDirty_ = true;
                    it = lodColumns_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    // Column rebuilds - time-budgeted to prevent stutter
    auto frameStart = std::chrono::high_resolution_clock::now();
    const auto frameBudget = std::chrono::microseconds(12000); // 12ms max for rebuilds+uploads
    
    for (auto it = columns_.begin(); it != columns_.end(); ) {
        if (it->second->needsUpdate) {
            auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
            if (elapsed > frameBudget) break; // Defer remaining to next frame
            
            std::vector<VoxelVertex> allVerts;
            std::vector<unsigned int> allInds;
            std::vector<unsigned int> allTrans;
            
            bool hasAnyChunk = false;
            for (int y = -16; y <= 48; ++y) {
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
        for (auto& [pos, col] : lodColumns_) {
            if (!col->inVRAM && !col->vertices.empty()) {
                auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
                if (elapsed > frameBudget) break;
                
                size_t newVertCount = col->vertices.size();
                size_t newIndCount = col->indices.size() + col->transparentIndices.size();
                
                bool canReuse = (col->allocatedVerts > 0 && 
                                 newVertCount <= col->allocatedVerts && 
                                 newIndCount <= col->allocatedInds);
                
                if (canReuse) {
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
                    if (currentVertexOffset_ + newVertCount >= 40000000 || 
                        currentIndexOffset_ + newIndCount >= 60000000) {
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
    float limitSq = (radialDistLimit + 128.0f) * (radialDistLimit + 128.0f);

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
            
            // Radial Distance Culling (Matches fog and prevents square 'cuts')
            float distX = (float)pos.x * Chunk::CHUNK_SIZE + 8.0f - camPos.x;
            float distZ = (float)pos.y * Chunk::CHUNK_SIZE + 8.0f - camPos.z;
            if (distX*distX + distZ*distZ > limitSq) continue;

            // Frustum Culling (Re-enabled with proper column alignment)
            if (Config::frustumCulling && !bypassFrustum) {
                glm::vec3 min(pos.x * Chunk::CHUNK_SIZE, -128, pos.y * Chunk::CHUNK_SIZE);
                glm::vec3 max(pos.x * Chunk::CHUNK_SIZE + 16, 256, pos.y * Chunk::CHUNK_SIZE + 16);
                if (!camera.isBoxInFrustum(min, max)) continue;
            }

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
        
        if (Config::enableLOD) {
            for (const auto& [pos, column] : lodColumns_) {
                if (!column->inVRAM || columns_.find(pos) != columns_.end()) continue;
                
                // Radial Distance Culling for LOD
                float lodDistX = (float)pos.x * Chunk::CHUNK_SIZE + 8.0f - camPos.x;
                float lodDistZ = (float)pos.y * Chunk::CHUNK_SIZE + 8.0f - camPos.z;
                if (lodDistX*lodDistX + lodDistZ*lodDistZ > limitSq) continue;

                // Frustum Culling for LOD
                if (Config::frustumCulling && !bypassFrustum) {
                    glm::vec3 min(pos.x * Chunk::CHUNK_SIZE, -128, pos.y * Chunk::CHUNK_SIZE);
                    glm::vec3 max(pos.x * Chunk::CHUNK_SIZE + 16, 256, pos.y * Chunk::CHUNK_SIZE + 16);
                    if (!camera.isBoxInFrustum(min, max)) continue;
                }

                if (column->opaqueCount > 0) {
                    DrawElementsIndirectCommand cmd;
                    cmd.count = column->opaqueCount;
                    cmd.instanceCount = 1;
                    cmd.firstIndex = column->indexOffset;
                    cmd.baseVertex = column->vertexOffset;
                    cmd.baseInstance = 0;
                    cachedOpaqueCmds_.push_back(cmd);
                }
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

bool ChunkManager::isChunkColumnLoaded(int cx, int cy, int cz) const {
    // Check if the chunks around the player's Y are loaded so physics don't stop underground
    for (int y = cy - 1; y <= cy + 1; ++y) {
        if (chunks_.find(glm::ivec3(cx, y, cz)) != chunks_.end()) {
            return true;
        }
    }
    return false;
}

#include "world/Biome.hpp"
#include "renderer/TextureAtlas.hpp"

std::unique_ptr<ChunkColumn> ChunkManager::generateLODColumn(glm::ivec2 gridPos) {
    auto col = std::make_unique<ChunkColumn>(gridPos);
    
    // SYNCED NOISE WITH WORLD GEN V2
    FastNoiseLite heightNoise;
    heightNoise.SetSeed(Config::currentSeed);
    heightNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    heightNoise.SetFrequency(0.005f);
    heightNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    heightNoise.SetFractalOctaves(4);
    
    int startIdx = 0;
    int step = Config::lodQuality; 
    if (step < 1) step = 1;

    for (int x = 0; x < Chunk::CHUNK_SIZE; x += step) {
        for (int z = 0; z < Chunk::CHUNK_SIZE; z += step) {
            float gX = (float)gridPos.x * Chunk::CHUNK_SIZE + x;
            float gZ = (float)gridPos.y * Chunk::CHUNK_SIZE + z;
            
            float hVal = heightNoise.GetNoise(gX, gZ);
            float tVal = heightNoise.GetNoise(gZ * 0.45f + 2137.0f, gX * 0.45f - 1492.0f);
            float cVal = heightNoise.GetNoise(gX * 0.1f, gZ * 0.1f); // Lower freq Continental Noise
            
            const Biome* biome = BiomeManager::getBiome(cVal, tVal);
            int h = biome->getTerrainHeight(hVal);
            
            float vx = gX;
            float vz = gZ;
            float vh = (float)h;
            
            uint8_t surf = biome->surfaceBlock;
            if (h <= 63) {
                vh = 63.0f;
                surf = 5; // Water surface
            }
            
            float rVal = 1.0f, gVal = 1.0f, bVal = 1.0f;
            if (surf == 2 || surf == 11) { // Tint grass
                float t = glm::clamp(tVal * 0.5f + 0.5f, 0.0f, 1.0f);
                float hm = glm::clamp(hVal * 0.5f + 0.5f, 0.0f, 1.0f);
                rVal = glm::mix(glm::mix(0.50f, 0.25f, hm), glm::mix(0.75f, 0.15f, hm), t);
                gVal = glm::mix(glm::mix(0.65f, 0.55f, hm), glm::mix(0.75f, 0.85f, hm), t);
                bVal = glm::mix(glm::mix(0.55f, 0.35f, hm), glm::mix(0.25f, 0.15f, hm), t);
            }
            
            float tid = TextureAtlas::getUVForBlock(surf, 2);
            uint32_t pD0 = (2 & 0x7) | (1 << 3) | (1 << 8) | ((static_cast<uint32_t>(tid) & 0xFFFF) << 13) | (0 << 29);
            uint32_t pD1 = (2 & 0x7) | (1 << 3) | (1 << 8) | ((static_cast<uint32_t>(tid) & 0xFFFF) << 13) | (1 << 29);
            uint32_t pD2 = (2 & 0x7) | (1 << 3) | (1 << 8) | ((static_cast<uint32_t>(tid) & 0xFFFF) << 13) | (2 << 29);
            uint32_t pD3 = (2 & 0x7) | (1 << 3) | (1 << 8) | ((static_cast<uint32_t>(tid) & 0xFFFF) << 13) | (3 << 29);

            VoxelVertex v0; v0.x = vx; v0.y = vh; v0.z = vz; v0.data = pD0;
            v0.r = (uint8_t)(rVal*255); v0.g = (uint8_t)(gVal*255); v0.b = (uint8_t)(bVal*255); v0.a = (surf==5)?180:255;
            VoxelVertex v1; v1.x = vx + step; v1.y = vh; v1.z = vz; v1.data = pD1;
            v1.r = (uint8_t)(rVal*255); v1.g = (uint8_t)(gVal*255); v1.b = (uint8_t)(bVal*255); v1.a = (surf==5)?180:255;
            VoxelVertex v2; v2.x = vx + step; v2.y = vh; v2.z = vz + step; v2.data = pD2;
            v2.r = (uint8_t)(rVal*255); v2.g = (uint8_t)(gVal*255); v2.b = (uint8_t)(bVal*255); v2.a = (surf==5)?180:255;
            VoxelVertex v3; v3.x = vx; v3.y = vh; v3.z = vz + step; v3.data = pD3;
            v3.r = (uint8_t)(rVal*255); v3.g = (uint8_t)(gVal*255); v3.b = (uint8_t)(bVal*255); v3.a = (surf==5)?180:255;

            col->vertices.push_back(v0); col->vertices.push_back(v1); col->vertices.push_back(v2); col->vertices.push_back(v3);
            if (surf == 5) {
                // Water: (0, 1, 2) and (0, 2, 3) currently point down, fix to (0, 2, 1) and (0, 3, 2)
                col->transparentIndices.push_back(startIdx+0); col->transparentIndices.push_back(startIdx+2); col->transparentIndices.push_back(startIdx+1);
                col->transparentIndices.push_back(startIdx+0); col->transparentIndices.push_back(startIdx+3); col->transparentIndices.push_back(startIdx+2);
            } else {
                // Terrain: Reverse to face UP
                col->indices.push_back(startIdx+0); col->indices.push_back(startIdx+2); col->indices.push_back(startIdx+1);
                col->indices.push_back(startIdx+0); col->indices.push_back(startIdx+3); col->indices.push_back(startIdx+2);
            }
            startIdx += 4;
            
            // SOLID SKIRT (4 vertical faces to bedrock for a gap-less distance world)
            if (surf != 5) {
                float skirtDepth = 120.0f; // Deeper skirts for high mountains
                auto addSkirtFace = [&](int dir, uint32_t tidVal) {
                    uint32_t pSD = (dir & 0x7) | (1 << 3) | (1 << 8) | ((uint32_t(tidVal) & 0xFFFF) << 13);
                    float x1 = vx, z1 = vz, x2 = vx + step, z2 = vz + step;
                    
                    VoxelVertex s0, s1, s2, s3;
                    if (dir == 5) { // -Z
                        s0.x = x1; s0.y = vh; s0.z = z1; s1.x = x2; s1.y = vh; s1.z = z1;
                        s2.x = x2; s2.y = vh-skirtDepth; s2.z = z1; s3.x = x1; s3.y = vh-skirtDepth; s3.z = z1;
                    } else if (dir == 4) { // +Z
                        s0.x = x2; s0.y = vh; s0.z = z2; s1.x = x1; s1.y = vh; s1.z = z2;
                        s2.x = x1; s2.y = vh-skirtDepth; s2.z = z2; s3.x = x2; s3.y = vh-skirtDepth; s3.z = z2;
                    } else if (dir == 0) { // +X
                        s0.x = x2; s0.y = vh; s0.z = z1; s1.x = x2; s1.y = vh; s1.z = z2;
                        s2.x = x2; s2.y = vh-skirtDepth; s2.z = z2; s3.x = x2; s3.y = vh-skirtDepth; s3.z = z1;
                    } else { // -X (dir 1)
                        s0.x = x1; s0.y = vh; s0.z = z2; s1.x = x1; s1.y = vh; s1.z = z1;
                        s2.x = x1; s2.y = vh-skirtDepth; s2.z = z1; s3.x = x1; s3.y = vh-skirtDepth; s3.z = z2;
                    }
                    
                    s0.data = pSD | (0 << 29); s1.data = pSD | (3 << 29); s2.data = pSD | (2 << 29); s3.data = pSD | (1 << 29);
                    s0.r = (uint8_t)(rVal*160); s0.g = (uint8_t)(gVal*160); s0.b = (uint8_t)(bVal*160); s0.a = 255;
                    s1.r = (uint8_t)(rVal*160); s1.g = (uint8_t)(gVal*160); s1.b = (uint8_t)(bVal*160); s1.a = 255;
                    s2.r = (uint8_t)(rVal*100); s2.g = (uint8_t)(gVal*100); s2.b = (uint8_t)(bVal*100); s2.a = 255;
                    s3.r = (uint8_t)(rVal*100); s3.g = (uint8_t)(gVal*100); s3.b = (uint8_t)(bVal*100); s3.a = 255;

                    col->vertices.push_back(s0); col->vertices.push_back(s1); col->vertices.push_back(s2); col->vertices.push_back(s3);
                    col->indices.push_back(startIdx+0); col->indices.push_back(startIdx+1); col->indices.push_back(startIdx+2);
                    col->indices.push_back(startIdx+0); col->indices.push_back(startIdx+2); col->indices.push_back(startIdx+3);
                    startIdx += 4;
                };

                addSkirtFace(5, tid); // -Z
                addSkirtFace(4, tid); // +Z
                addSkirtFace(0, tid); // +X
                addSkirtFace(1, tid); // -X
            }
        }
    }
    
    col->opaqueCount = col->indices.size();
    col->transparentCount = col->transparentIndices.size();
    col->needsUpdate = true;
    return col;
}
