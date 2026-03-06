#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>

namespace TextureAtlas {
    // Generates a single OpenGL texture by combining all PNGs found in the directory.
    // Automatically binds it and uploads to the GPU.
    bool build(const std::string& directoryPath);

    // Binds the generated atlas to the specified texture unit
    void bind(unsigned int unit = 0);

    // Fast lookup for meshing - Pre-configured standard map
    float getUVForBlock(uint8_t blockType, int faceDir);

    // Advanced lookup for UI or future custom blocks
    float getUV(const std::string& textureName);
    
    // Return texture ID
    unsigned int getTextureID();
    
    // Standalone GUI textures
    unsigned int getGuiTexture(const std::string& name);
}
