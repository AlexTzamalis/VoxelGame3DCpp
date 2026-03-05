#include "WorldManager.hpp"
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
