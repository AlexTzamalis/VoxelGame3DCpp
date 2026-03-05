#pragma once

enum class GameMode {
    SURVIVAL,
    CREATIVE,
    SPECTATOR
};

enum class GameState {
    MAIN_MENU,
    WORLD_SELECT,
    CREATE_WORLD,
    SETTINGS,
    PLAYING,
    PAUSED,
    COMMAND_INPUT
};

namespace Config {
    // Window & Display Settings
    extern int windowWidth;
    extern int windowHeight;
    extern int fpsCap;

    // Player & Controls Settings
    extern float playerSpeed;
    extern float playerSprintSpeed;
    extern float mouseSensitivity;
    extern float cameraFov;

    // World & Rendering Settings
    extern int renderDistance;
    extern int renderDistanceY;
    extern bool frustumCulling;

    // Graphics & Shaders Settings
    extern bool enableShaders;
    extern bool enableShadows;
    extern float dayTimeSpeed;

    // Load & Save to disk
    void load();
    void save();
    
    extern float fogDensity;
    
    // Game State
    extern GameMode currentMode;
    extern GameState currentState;
    extern int currentWorldType; // 0=Default, 1=Flat, 2=Skyblock
    extern int currentSeed;
}
