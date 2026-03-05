#pragma once

enum class GameMode {
    SURVIVAL,
    CREATIVE,
    SPECTATOR
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
    extern float fogDensity;
    
    // Game State
    extern GameMode currentMode;
}
