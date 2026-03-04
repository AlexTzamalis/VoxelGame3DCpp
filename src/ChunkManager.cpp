#include "ChunkManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>

ChunkManager::ChunkManager() {
    // Configure FastNoiseLite for smooth, natural terrain
    noise_.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise_.SetSeed(1337); 
    noise_.SetFrequency(0.02f);       // Determines width / scale of hills
    noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise_.SetFractalOctaves(4);      // Detail levels
}

void ChunkManager::update(const glm::vec3& cameraPosition) {
    // Determine which chunk the camera is currently inside
    glm::ivec3 cameraChunkPos(
        std::floor(cameraPosition.x / Chunk::CHUNK_SIZE),
        0, // For now, we are locking generation to the Y=0 chunk layer only
        std::floor(cameraPosition.z / Chunk::CHUNK_SIZE)
    );

    // Keep track of which chunks are valid and within render distance this frame
    std::unordered_map<glm::ivec3, bool, IVec3Hash> activeChunks;

    for (int x = -renderDistance_; x <= renderDistance_; ++x) {
        for (int z = -renderDistance_; z <= renderDistance_; ++z) {
            glm::ivec3 chunkPos = cameraChunkPos + glm::ivec3(x, 0, z);
            activeChunks[chunkPos] = true;

            // If a chunk in the radius isn't in our map, generate it
            if (chunks_.find(chunkPos) == chunks_.end()) {
                auto chunk = std::make_unique<Chunk>(chunkPos);
                chunk->generateTerrain(noise_); 
                chunk->generateMesh();          
                chunk->updateBuffers();         
                chunks_[chunkPos] = std::move(chunk);
            }
        }
    }

    // Iterate through loaded chunks. Erase any that fell outside our active render distance
    for (auto it = chunks_.begin(); it != chunks_.end(); ) {
        if (activeChunks.find(it->first) == activeChunks.end()) {
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

void ChunkManager::render(unsigned int shaderProgram) const {
    // Render every currently loaded chunk
    for (const auto& [pos, chunk] : chunks_) {
        // Calculate the world position of the chunk
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(pos * Chunk::CHUNK_SIZE));
        
        // Pass model matrix to the active shader
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        
        chunk->render();
    }
}
