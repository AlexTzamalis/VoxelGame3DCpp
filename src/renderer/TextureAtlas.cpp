#include "renderer/TextureAtlas.hpp"
#include "world/BlockRegistry.hpp"
#include "stb_image.h"
#include <GL/glew.h>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace fs = std::filesystem;

namespace {
    unsigned int atlasTextureId = 0;
    std::unordered_map<std::string, float> textureLayers; 
    std::unordered_map<std::string, unsigned int> guiTextures; // standalone GL_TEXTURE_2D
}

bool TextureAtlas::build(const std::string& directoryPath) {
    std::string path = directoryPath;
    bool isZip = false;
    std::string tempExtractDir = ".temp_texture_pack";

    if (path.length() >= 4 && path.substr(path.length() - 4) == ".zip") {
        isZip = true;
        path = tempExtractDir;
        if (fs::exists(tempExtractDir)) fs::remove_all(tempExtractDir);
        fs::create_directory(tempExtractDir);
        
        std::string cmd = "tar -xf \"" + directoryPath + "\" -C \"" + tempExtractDir + "\"";
        if (std::system(cmd.c_str()) != 0) return false;
    }

    if (isZip) {
        BlockRegistry::loadBlockStates(tempExtractDir + "/assets");
    } else {
        BlockRegistry::loadBlockStates(path + "/assets");
    }

    struct ImageData {
        std::string name;
        std::string path;
        int width, height, channels;
        unsigned char* data;
        bool isGui;
    };

    std::vector<ImageData> images;
    int maxTileExt = 16; 

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.path().extension() == ".png") {
            std::string relPath = fs::relative(entry.path(), path).string();
            bool isGui = (relPath.find("gui") != std::string::npos) || 
                         (relPath.find("skin") != std::string::npos) || 
                         (relPath.find("skybox") != std::string::npos);

            ImageData img;
            img.name = entry.path().stem().string();
            img.path = relPath;
            img.isGui = isGui;
            
            stbi_set_flip_vertically_on_load(isGui ? 0 : 1); // HUD textures usually not flipped
            img.data = stbi_load(entry.path().string().c_str(), &img.width, &img.height, &img.channels, 4);
            
            if (img.data) {
                if (isGui) {
                    std::cerr << "Found GUI image: " << img.name << "\n";
                } else {
                    if (img.height > img.width && img.height % img.width == 0) img.height = img.width; 
                    maxTileExt = std::max({maxTileExt, img.width, img.height});
                }
                images.push_back(img);
            } else {
                std::cerr << "stbi_load failed on: " << entry.path().string() << "\n";
            }
        }
    }
    
    // Fallback Texture explicitly created so we never crash!
    ImageData missingTex;
    missingTex.name = "missing_texture";
    missingTex.path = "";
    missingTex.width = maxTileExt;
    missingTex.height = maxTileExt;
    missingTex.channels = 4;
    missingTex.isGui = false;
    missingTex.data = new unsigned char[maxTileExt * maxTileExt * 4];
    for (int y = 0; y < maxTileExt; ++y) {
        for (int x = 0; x < maxTileExt; ++x) {
            bool isMagenta = ((x / (maxTileExt/2)) + (y / (maxTileExt/2))) % 2 == 0;
            int idx = (y * maxTileExt + x) * 4;
            missingTex.data[idx + 0] = isMagenta ? 255 : 0;
            missingTex.data[idx + 1] = 0;
            missingTex.data[idx + 2] = isMagenta ? 255 : 0;
            missingTex.data[idx + 3] = 255;
        }
    }
    images.push_back(missingTex);
    
    std::cerr << "Total images loaded: " << images.size() << "\n";

    if (atlasTextureId) glDeleteTextures(1, &atlasTextureId);
    for (auto& [name, tex] : guiTextures) glDeleteTextures(1, &tex);
    guiTextures.clear();
    
    glGenTextures(1, &atlasTextureId);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTextureId);

    // Filter images into block atlas (must be square tileable) and GUI (standalone)
    std::vector<ImageData> atlasImages;
    for (auto& img : images) {
        if (img.isGui) {
            unsigned int tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data);
            guiTextures[img.name] = tex;
            stbi_image_free(img.data);
        } else {
            atlasImages.push_back(img);
        }
    }

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, maxTileExt, maxTileExt, atlasImages.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    textureLayers.clear();
    
    for (size_t i = 0; i < atlasImages.size(); ++i) {
        auto& img = atlasImages[i];
        if (img.width != maxTileExt || img.height != maxTileExt) {
            std::vector<unsigned char> scaled(maxTileExt * maxTileExt * 4);
            for (int y = 0; y < maxTileExt; ++y) {
                for (int x = 0; x < maxTileExt; ++x) {
                    int srcX = (x * img.width) / maxTileExt;
                    int srcY = (y * img.height) / maxTileExt;
                    int srcIdx = (srcY * img.width + srcX) * 4;
                    int dstIdx = (y * maxTileExt + x) * 4;
                    scaled[dstIdx + 0] = img.data[srcIdx + 0];
                    scaled[dstIdx + 1] = img.data[srcIdx + 1];
                    scaled[dstIdx + 2] = img.data[srcIdx + 2];
                    scaled[dstIdx + 3] = img.data[srcIdx + 3];
                }
            }
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, maxTileExt, maxTileExt, 1, GL_RGBA, GL_UNSIGNED_BYTE, scaled.data());
        } else {
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, maxTileExt, maxTileExt, 1, GL_RGBA, GL_UNSIGNED_BYTE, img.data);
        }
        textureLayers[img.name] = static_cast<float>(i);
        stbi_image_free(img.data);
    }
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    if (isZip) fs::remove_all(tempExtractDir);
    return true;
}

unsigned int TextureAtlas::getGuiTexture(const std::string& name) {
    if (guiTextures.count(name)) return guiTextures[name];
    std::cerr << "GUI Texture not found: " << name << ". Available GUI textures:\n";
    for (auto& [n, t] : guiTextures) std::cerr << "  " << n << "\n";
    return 0;
}

void TextureAtlas::bind(unsigned int unit) {
    if (atlasTextureId) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTextureId);
    }
}

float TextureAtlas::getUV(const std::string& textureName) {
    auto it = textureLayers.find(textureName);
    if (it != textureLayers.end()) {
        return it->second;
    }
    return textureLayers["missing_texture"]; 
}

unsigned int TextureAtlas::getTextureID() {
    return atlasTextureId;
}

float TextureAtlas::getUVForBlock(uint8_t blockType, int faceDir) {
    auto& block = BlockRegistry::getBlock(blockType);
    std::string request = block.name; // default

    if (faceDir == 2) request = block.texTop;
    else if (faceDir == 3) request = block.texBottom;
    else request = block.texSide;

    return getUV(request);
}
