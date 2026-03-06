#include "world/WorldManager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;

long long getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void WorldManager::init() {
    if (!fs::exists("saves")) {
        fs::create_directory("saves");
    }
}

std::vector<WorldMetadata> WorldManager::getSavedWorlds() {
    std::vector<WorldMetadata> worlds;
    if (!fs::exists("saves")) return worlds;
    
    for (const auto& entry : fs::directory_iterator("saves")) {
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
    std::string path = "saves/" + meta.folderName;
    if (fs::exists(path)) return false;
    fs::create_directory(path);
    fs::create_directory(path + "/chunks");
    saveWorldMetadata(meta);
    return true;
}

bool WorldManager::saveWorldMetadata(const WorldMetadata& meta) {
    std::string path = "saves/" + meta.folderName + "/meta.txt";
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
    std::string filename = "saves/" + currentWorld.folderName + "/chunks/" + 
                           std::to_string(pos.x) + "_" + std::to_string(pos.y) + "_" + std::to_string(pos.z) + ".bin";
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(voxelData.data()), voxelData.size());
    return true;
}

bool WorldManager::loadChunk(glm::ivec3 pos, std::vector<uint8_t>& voxelData) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = "saves/" + currentWorld.folderName + "/chunks/" + 
                           std::to_string(pos.x) + "_" + std::to_string(pos.y) + "_" + std::to_string(pos.z) + ".bin";
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size == 0) {
        voxelData.clear();
        return true;
    }
    
    file.read(reinterpret_cast<char*>(voxelData.data()), voxelData.size());
    return true;
}

bool WorldManager::savePlayer(glm::vec3 pos, float pitch, float yaw) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = "saves/" + currentWorld.folderName + "/player.txt";
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    file << pos.x << " " << pos.y << " " << pos.z << "\n" << pitch << " " << yaw << "\n";
    return true;
}

bool WorldManager::loadPlayer(glm::vec3& pos, float& pitch, float& yaw) {
    if (currentWorld.folderName.empty()) return false;
    std::string filename = "saves/" + currentWorld.folderName + "/player.txt";
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    file >> pos.x >> pos.y >> pos.z >> pitch >> yaw;
    return true;
}
