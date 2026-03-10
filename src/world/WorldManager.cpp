#include "world/WorldManager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <lz4.h>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstdlib>
#endif

namespace fs = std::filesystem;

long long getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string WorldManager::getSavePath() {
    if (!basePath.empty()) return basePath;
    
#ifdef _WIN32
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        basePath = std::string(appDataPath) + "/VoxelGame3D";
    } else {
        basePath = "saves";
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        basePath = std::string(homeDir) + "/.local/share/VoxelGame3D";
    } else {
        basePath = "saves";
    }
#endif
    return basePath;
}

void WorldManager::init() {
    std::string path = getSavePath();
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
    if (!fs::exists(path + "/saves")) {
        fs::create_directories(path + "/saves");
    }
}

std::vector<WorldMetadata> WorldManager::getSavedWorlds() {
    std::vector<WorldMetadata> worlds;
    std::string savesPath = getSavePath() + "/saves";
    if (!fs::exists(savesPath)) return worlds;
    
    for (const auto& entry : fs::directory_iterator(savesPath)) {
        if (entry.is_directory()) {
            std::string metaPath = entry.path().string() + "/meta.txt";
            if (fs::exists(metaPath)) {
                std::ifstream file(metaPath);
                WorldMetadata meta;
                meta.folderName = entry.path().filename().string();
                std::getline(file, meta.displayName);
                file >> meta.seed;
                int modeInt; file >> modeInt; meta.mode = static_cast<GameMode>(modeInt);
                file >> meta.worldType;
                file >> meta.creationDate;
                file >> meta.lastPlayed;
                file >> meta.timePlayedSeconds;
                worlds.push_back(meta);
            }
        }
    }
    return worlds;
}

bool WorldManager::createWorld(const WorldMetadata& meta) {
    std::string path = getSavePath() + "/saves/" + meta.folderName;
    if (fs::exists(path)) return false;
    fs::create_directories(path);
    fs::create_directories(path + "/chunks");
    saveWorldMetadata(meta);
    return true;
}

bool WorldManager::saveWorldMetadata(const WorldMetadata& meta) {
    std::string path = getSavePath() + "/saves/" + meta.folderName + "/meta.txt";
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << meta.displayName << "\n"
         << meta.seed << "\n"
         << static_cast<int>(meta.mode) << "\n"
         << meta.worldType << "\n"
         << meta.creationDate << "\n"
         << meta.lastPlayed << "\n"
         << meta.timePlayedSeconds << "\n";
    return true;
}

void WorldManager::loadWorld(const std::string& folderName) {
    auto worlds = getSavedWorlds();
    for (const auto& w : worlds) {
        if (w.folderName == folderName) {
            currentWorld = w;
            currentWorld.lastPlayed = getCurrentTimeMs();
            sessionStartTime = getCurrentTimeMs();
            sessionStartPlayTime = currentWorld.timePlayedSeconds;
            
            Config::currentMode = currentWorld.mode;
            Config::currentWorldType = currentWorld.worldType;
            Config::currentSeed = currentWorld.seed;
            
            saveWorldMetadata(currentWorld);
            return;
        }
    }
}

void WorldManager::updatePlayTime() {
    if (sessionStartTime == 0) return;
    long long now = getCurrentTimeMs();
    long long diffSeconds = (now - sessionStartTime) / 1000;
    currentWorld.timePlayedSeconds = sessionStartPlayTime + diffSeconds;
    currentWorld.lastPlayed = now;
    saveWorldMetadata(currentWorld);
}

bool WorldManager::saveChunk(glm::ivec3 pos, const std::vector<uint8_t>& voxelData) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = getSavePath() + "/saves/" + currentWorld.folderName + "/chunks/" + 
                           std::to_string(pos.x) + "_" + std::to_string(pos.y) + "_" + std::to_string(pos.z) + ".lz4";
    
    // LZ4 Compression
    int srcSize = voxelData.size();
    if (srcSize == 0) return false;

    int maxDstSize = LZ4_compressBound(srcSize);
    std::vector<char> compressedData(maxDstSize);
    int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(voxelData.data()), 
                                             compressedData.data(), srcSize, maxDstSize);
    
    if (compressedSize <= 0) return false;

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    // Header
    file.write(reinterpret_cast<const char*>(&srcSize), sizeof(int));
    file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(int));
    file.write(compressedData.data(), compressedSize);
    
    return true;
}

bool WorldManager::loadChunk(glm::ivec3 pos, std::vector<uint8_t>& voxelData) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = getSavePath() + "/saves/" + currentWorld.folderName + "/chunks/" + 
                           std::to_string(pos.x) + "_" + std::to_string(pos.y) + "_" + std::to_string(pos.z) + ".lz4";
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    int originalLength, compressedLength;
    file.read(reinterpret_cast<char*>(&originalLength), sizeof(int));
    file.read(reinterpret_cast<char*>(&compressedLength), sizeof(int));
    
    if (file.fail()) return false;

    std::vector<char> compressedBuffer(compressedLength);
    file.read(compressedBuffer.data(), compressedLength);
    
    voxelData.resize(originalLength);
    int decompressedSize = LZ4_decompress_safe(compressedBuffer.data(), 
                                               reinterpret_cast<char*>(voxelData.data()), 
                                               compressedLength, originalLength);
    
    return decompressedSize == originalLength;
}

bool WorldManager::savePlayer(glm::vec3 pos, float pitch, float yaw) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = getSavePath() + "/saves/" + currentWorld.folderName + "/player.txt";
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    file << pos.x << " " << pos.y << " " << pos.z << "\n" << pitch << " " << yaw << "\n";
    return true;
}

bool WorldManager::loadPlayer(glm::vec3& pos, float& pitch, float& yaw) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = getSavePath() + "/saves/" + currentWorld.folderName + "/player.txt";
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    file >> pos.x >> pos.y >> pos.z >> pitch >> yaw;
    return true;
}
