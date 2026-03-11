#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

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

enum class GraphicsPreset {
    LOW,
    MEDIUM,
    HIGH,
    ULTRA,
    CUSTOM
};

namespace Config {
    // Window & Display Settings
    extern int windowWidth;
    extern int windowHeight;
    extern bool isFullscreen;
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

    // LOD & Distant Horizons
    extern bool enableLOD;
    extern int lodDistance;
    extern int lodQuality; // Step size (1 = High, 2 = Med, 4 = Low, 8 = Lowest)

    // === Granular Shader Settings ===
    // Master toggle
    extern bool enableShaders;
    
    // Shadows
    extern bool enableShadows;
    extern float shadowIntensity;      // 0.0 = no shadow, 1.0 = pitch black
    extern int shadowMapSize;          // 1024, 2048, 4096
    extern float shadowSoftness;
    extern float shadowBias;
    
    // Water
    extern int waterMode;              // 0 = Basic flat, 1 = Advanced (waves, reflections)
    extern float waterTransparency;
    extern float waterWaveSpeed;
    extern float waterWaveHeight;
    extern bool enableWaterReflections;
    
    // Lighting  
    extern bool enableFog;
    extern float fogDensity;
    extern glm::vec3 fogColor;
    extern float fogHeightFalloff;
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
    extern float cloudDensity;
    extern float cloudThickness;
    extern int cloudQuality;
    
    extern float saturation;
    extern float contrast;

    // Atmosphere & Sun/Moon
    extern float sunSize;
    extern float sunIntensity;
    extern glm::vec3 sunColor;
    extern float moonSize;
    extern float moonIntensity;
    extern float godRaysIntensity;
    extern float starDensity;
    extern float milkyWayIntensity;
    
    // Preset
    extern GraphicsPreset graphicsPreset;

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
