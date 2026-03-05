#pragma once

#include "Config.hpp"
#include <string>
#include <vector>

long long getCurrentTimeMs();

struct WorldMetadata {
    std::string folderName;
    std::string displayName;
    int seed;
    GameMode mode;
    int worldType; // 0=Default, 1=Flat, 2=Skyblock
    long long creationDate;
    long long lastPlayed;
    long long timePlayedSeconds;
};

class WorldManager {
public:
    static void init();
    static std::vector<WorldMetadata> getSavedWorlds();
    static bool createWorld(const WorldMetadata& meta);
    static bool saveWorldMetadata(const WorldMetadata& meta);
    static void loadWorld(const std::string& folderName);
    static void updatePlayTime();
    
    inline static WorldMetadata currentWorld;
    inline static long long sessionStartTime = 0;
    inline static long long sessionStartPlayTime = 0;
};
