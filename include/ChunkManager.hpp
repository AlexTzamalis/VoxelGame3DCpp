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
#include <vector>
#include <condition_variable>
#include <atomic>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

struct IVec2Hash {
    std::size_t operator()(const glm::ivec2& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

#pragma pack(push, 1)
struct DrawElementsIndirectCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int baseVertex;
    unsigned int baseInstance;
};
#pragma pack(pop)

struct ChunkColumn {
    glm::ivec2 position;
    
    std::vector<VoxelVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<unsigned int> transparentIndices;
    
    unsigned int vertexOffset = 0;
    unsigned int indexOffset = 0;
    unsigned int transparentIndexOffset = 0;
    
    // Cached counts for rendering after CPU data is freed
    unsigned int opaqueCount = 0;
    unsigned int transparentCount = 0;
    
    bool needsUpdate = false;
    bool inVRAM = false;
    
    ChunkColumn(glm::ivec2 pos) : position(pos) {}
    ~ChunkColumn() {}
};

class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager();

    void update(const glm::vec3& cameraPosition);
    void render(unsigned int shaderProgram, const Camera& camera, bool bypassFrustum = false, float radialDistLimit = -1.0f) const;

    void clear();
    bool isChunkColumnLoaded(int cx, int cz) const;

    uint8_t getVoxelGlobal(int x, int y, int z) const;
    void setVoxelGlobal(int x, int y, int z, uint8_t type);

private:
    void workerThreadFunc();
    void defragmentVRAM();

    unsigned int mdiVAO_ = 0;
    unsigned int mdiVBO_ = 0;
    unsigned int mdiEBO_ = 0;
    unsigned int mdiIndirectBufferOpaque_ = 0;
    unsigned int mdiIndirectBufferTrans_ = 0;
    unsigned int currentVertexOffset_ = 0;
    unsigned int currentIndexOffset_ = 0;
    
    // Cached MDI draw command arrays (rebuilt only when dirty)
    mutable std::vector<DrawElementsIndirectCommand> cachedOpaqueCmds_;
    mutable std::vector<DrawElementsIndirectCommand> cachedTransCmds_;
    mutable bool mdiCommandsDirty_ = true;

    // Contains fully loaded and rendered chunks
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> chunks_;
    std::unordered_map<glm::ivec2, std::unique_ptr<ChunkColumn>, IVec2Hash> columns_;

    // Keeps track of chunks currently being generated so we don't start duplicate jobs
    std::unordered_map<glm::ivec3, bool, IVec3Hash> generatingChunks_;
    
    // Multi-Threading Data Structures
    std::vector<std::thread> workers_;
    std::vector<glm::ivec3> pendingTasks_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> isRunning_;

    // Chunks returning from workers that need buffer uploads
    std::vector<std::unique_ptr<Chunk>> readyChunks_;
    std::mutex readyMutex_;
};
