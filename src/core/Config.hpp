#pragma once

#include <string>
#include <unordered_map>

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
    extern bool vsync;

    // Player & Controls Settings
    extern float playerSpeed;
    extern float playerSprintSpeed;
    extern float playerSneakSpeed;
    extern float mouseSensitivity;
    extern float cameraFov;
    extern float zoomBaseFov;        // Default zoom FOV target (adjustable via scroll)

    // World & Rendering Settings
    extern int renderDistance;
    extern int renderDistanceY;
    extern bool frustumCulling;

    // === Granular Shader Settings ===
    // Master toggle
    extern bool enableShaders;
    
    // Shadows
    extern bool enableShadows;
    extern float shadowIntensity;      // 0.0 = no shadow, 1.0 = pitch black
    extern int shadowMapSize;          // 1024, 2048, 4096
    
    // Water
    extern int waterMode;              // 0 = Basic flat, 1 = Advanced (waves, reflections)
    
    // Lighting  
    extern bool enableFog;
    extern float fogDensity;
    extern bool enableDirectionalFaceShading; // Darkens N/S/E/W faces
    extern float ambientBrightness;    // Min ambient light (brighter nights)
    extern float dayTimeSpeed;
    
    // Wind effects
    extern bool enableLeafWind;
    
    // Ultra mode (Photon-like maximum quality)
    extern bool ultraMode;
    
    // Clouds
    extern bool enableClouds;
    extern float cloudHeight;
    extern float cloudScale;
    extern float cloudSpeed;
    extern float saturation;
    extern float contrast;

    // Keybind save/load helpers
    void loadKeybinds();
    void saveKeybinds();

    // Load & Save to disk
    void load();
    void save();
    
    // Game State
    extern GameMode currentMode;
    extern GameState currentState;
    extern int currentWorldType; // 0=Default, 1=Flat, 2=Skyblock
    extern int currentSeed;
}
