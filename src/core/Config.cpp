#include "core/Config.hpp"
#include "game/InputManager.hpp"
#include "world/WorldManager.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace Config {
    int windowWidth = 1280;
    int windowHeight = 720;
    bool isFullscreen = true;
    int fpsCap = 240;
    bool vsync = false;

    float playerSpeed = 4.317f;
    float playerSprintSpeed = 5.6f;
    float playerSneakSpeed = 1.3f;
    float mouseSensitivity = 0.1f;
    float cameraFov = 75.0f;
    float zoomBaseFov = 30.0f;

    int renderDistance = 12;
    int renderDistanceY = 4;
    bool frustumCulling = true;

    bool enableLOD = true;
    int lodDistance = 256; // 4km horizon
    int lodQuality = 2;   // High detail distant terrain (8x8 grid)
    
    // === Granular Shader Settings ===
    bool enableShaders = true;
    
    // Shadows
    bool enableShadows = true;
    float shadowIntensity = 0.45f;
    int shadowMapSize = 2048;
    float shadowSoftness = 1.0f;
    float shadowBias = 0.005f;
    
    // Water
    int waterMode = 1;
    float waterTransparency = 0.6f;
    float waterWaveSpeed = 1.0f;
    float waterWaveHeight = 0.2f;
    bool enableWaterReflections = true;
    
    // Lighting  
    bool enableFog = true;
    float fogDensity = 0.005f;
    glm::vec3 fogColor = glm::vec3(0.5f, 0.6f, 0.7f);
    float fogHeightFalloff = 0.02f;
    bool enableDirectionalFaceShading = true;
    float ambientBrightness = 0.35f;
    float dayTimeSpeed = 0.0021816f;
    
    // Wind effects
    bool enableLeafWind = true;
    
    // Ultra mode
    bool ultraMode = false;
    
    // Clouds
    bool enableClouds = true;
    float cloudHeight = 400.0f;
    float cloudScale = 0.00045f;
    float cloudSpeed = 0.012f;
    float cloudDensity = 0.5f;
    float cloudThickness = 120.0f;
    int cloudQuality = 16;
    
    float saturation = 0.85f;
    float contrast = 1.05f;

    // Atmosphere & Sun/Moon
    float sunSize = 0.012f; // ~0.7 degrees (Real is 0.5)
    float sunIntensity = 1.0f;
    glm::vec3 sunColor = glm::vec3(1.0f, 0.9f, 0.8f);
    float moonSize = 0.01f;
    float moonIntensity = 0.5f;
    float godRaysIntensity = 0.15f;
    float starDensity = 0.5f;
    float milkyWayIntensity = 0.3f;
    
    // Preset
    GraphicsPreset graphicsPreset = GraphicsPreset::MEDIUM;
    
    GameMode currentMode = GameMode::CREATIVE;
    GameState currentState = GameState::MAIN_MENU;
    int currentWorldType = 0;
    int currentSeed = 1337;

    void loadKeybinds() {
        std::ifstream file(WorldManager::getSavePath() + "/keybinds.ini");
        if (file.is_open()) {
            std::string action;
            int key;
            while (file >> action >> key) {
                InputManager::setKey(action, key);
            }
        }
    }

    void saveKeybinds() {
        std::ofstream file(WorldManager::getSavePath() + "/keybinds.ini");
        if (file.is_open()) {
            for (const auto& [action, key] : InputManager::getAllBindings()) {
                file << action << " " << key << "\n";
            }
        }
    }

    void load() {
        std::ifstream file(WorldManager::getSavePath() + "/settings.ini");
        if (file.is_open()) {
            std::string key;
            while(file >> key) {
                if (key == "RENDER_DISTANCE") file >> renderDistance;
                else if (key == "RENDER_DISTANCE_Y") file >> renderDistanceY;
                else if (key == "ENABLE_LOD") file >> enableLOD;
                else if (key == "LOD_DISTANCE") file >> lodDistance;
                else if (key == "LOD_QUALITY") file >> lodQuality;
                else if (key == "FOV") file >> cameraFov;
                else if (key == "MAX_FPS") file >> fpsCap;
                else if (key == "VSYNC") file >> vsync;
                else if (key == "IS_FULLSCREEN") file >> isFullscreen;
                else if (key == "ENABLE_SHADERS") file >> enableShaders;
                else if (key == "ENABLE_SHADOWS") file >> enableShadows;
                else if (key == "SHADOW_INTENSITY") file >> shadowIntensity;
                else if (key == "SHADOW_MAP_SIZE") file >> shadowMapSize;
                else if (key == "SHADOW_SOFTNESS") file >> shadowSoftness;
                else if (key == "SHADOW_BIAS") file >> shadowBias;
                else if (key == "WATER_MODE") file >> waterMode;
                else if (key == "WATER_TRANSPARENCY") file >> waterTransparency;
                else if (key == "WATER_WAVE_SPEED") file >> waterWaveSpeed;
                else if (key == "WATER_WAVE_HEIGHT") file >> waterWaveHeight;
                else if (key == "WATER_REFLECTIONS") file >> enableWaterReflections;
                else if (key == "ENABLE_FOG") file >> enableFog;
                else if (key == "FOG_DENSITY") file >> fogDensity;
                else if (key == "FOG_COLOR_R") file >> fogColor.r;
                else if (key == "FOG_COLOR_G") file >> fogColor.g;
                else if (key == "FOG_COLOR_B") file >> fogColor.b;
                else if (key == "FOG_HEIGHT_FALLOFF") file >> fogHeightFalloff;
                else if (key == "FACE_SHADING") file >> enableDirectionalFaceShading;
                else if (key == "AMBIENT_BRIGHTNESS") file >> ambientBrightness;
                else if (key == "DAY_SPEED") file >> dayTimeSpeed;
                else if (key == "LEAF_WIND") file >> enableLeafWind;
                else if (key == "ULTRA_MODE") file >> ultraMode;
                else if (key == "ENABLE_CLOUDS") file >> enableClouds;
                else if (key == "CLOUD_HEIGHT") file >> cloudHeight;
                else if (key == "CLOUD_SCALE") file >> cloudScale;
                else if (key == "CLOUD_SPEED") file >> cloudSpeed;
                else if (key == "CLOUD_DENSITY") file >> cloudDensity;
                else if (key == "CLOUD_THICKNESS") file >> cloudThickness;
                else if (key == "CLOUD_QUALITY") file >> cloudQuality;
                else if (key == "SATURATION") file >> saturation;
                else if (key == "CONTRAST") file >> contrast;
                else if (key == "SUN_SIZE") file >> sunSize;
                else if (key == "SUN_INTENSITY") file >> sunIntensity;
                else if (key == "SUN_COLOR_R") file >> sunColor.r;
                else if (key == "SUN_COLOR_G") file >> sunColor.g;
                else if (key == "SUN_COLOR_B") file >> sunColor.b;
                else if (key == "MOON_SIZE") file >> moonSize;
                else if (key == "MOON_INTENSITY") file >> moonIntensity;
                else if (key == "GOD_RAYS_INTENSITY") file >> godRaysIntensity;
                else if (key == "STAR_DENSITY") file >> starDensity;
                else if (key == "MILKY_WAY_INTENSITY") file >> milkyWayIntensity;
                else if (key == "GRAPHICS_PRESET") {
                    int p; file >> p;
                    graphicsPreset = static_cast<GraphicsPreset>(p);
                }
                else if (key == "PLAYER_SPEED") file >> playerSpeed;
                else if (key == "SPRINT_SPEED") file >> playerSprintSpeed;
                else if (key == "SENSITIVITY") file >> mouseSensitivity;
                else if (key == "ZOOM_FOV") file >> zoomBaseFov;
            }
        }
        loadKeybinds();
    }

    void save() {
        std::ofstream file(WorldManager::getSavePath() + "/settings.ini");
        if (file.is_open()) {
            file << "RENDER_DISTANCE " << renderDistance << "\n"
                 << "RENDER_DISTANCE_Y " << renderDistanceY << "\n"
                 << "ENABLE_LOD " << enableLOD << "\n"
                 << "LOD_DISTANCE " << lodDistance << "\n"
                 << "LOD_QUALITY " << lodQuality << "\n"
                 << "FOV " << cameraFov << "\n"
                 << "MAX_FPS " << fpsCap << "\n"
                 << "VSYNC " << vsync << "\n"
                 << "IS_FULLSCREEN " << isFullscreen << "\n"
                 << "ENABLE_SHADERS " << enableShaders << "\n"
                 << "ENABLE_SHADOWS " << enableShadows << "\n"
                 << "SHADOW_INTENSITY " << shadowIntensity << "\n"
                 << "SHADOW_MAP_SIZE " << shadowMapSize << "\n"
                 << "SHADOW_SOFTNESS " << shadowSoftness << "\n"
                 << "SHADOW_BIAS " << shadowBias << "\n"
                 << "WATER_MODE " << waterMode << "\n"
                 << "WATER_TRANSPARENCY " << waterTransparency << "\n"
                 << "WATER_WAVE_SPEED " << waterWaveSpeed << "\n"
                 << "WATER_WAVE_HEIGHT " << waterWaveHeight << "\n"
                 << "WATER_REFLECTIONS " << enableWaterReflections << "\n"
                 << "ENABLE_FOG " << enableFog << "\n"
                 << "FOG_DENSITY " << fogDensity << "\n"
                 << "FOG_COLOR_R " << fogColor.r << "\n"
                 << "FOG_COLOR_G " << fogColor.g << "\n"
                 << "FOG_COLOR_B " << fogColor.b << "\n"
                 << "FOG_HEIGHT_FALLOFF " << fogHeightFalloff << "\n"
                 << "FACE_SHADING " << enableDirectionalFaceShading << "\n"
                 << "AMBIENT_BRIGHTNESS " << ambientBrightness << "\n"
                 << "DAY_SPEED " << dayTimeSpeed << "\n"
                 << "LEAF_WIND " << enableLeafWind << "\n"
                 << "ULTRA_MODE " << ultraMode << "\n"
                 << "ENABLE_CLOUDS " << enableClouds << "\n"
                 << "CLOUD_HEIGHT " << cloudHeight << "\n"
                 << "CLOUD_SCALE " << cloudScale << "\n"
                 << "CLOUD_SPEED " << cloudSpeed << "\n"
                 << "CLOUD_DENSITY " << cloudDensity << "\n"
                 << "CLOUD_THICKNESS " << cloudThickness << "\n"
                 << "CLOUD_QUALITY " << cloudQuality << "\n"
                 << "SATURATION " << saturation << "\n"
                 << "CONTRAST " << contrast << "\n"
                 << "SUN_SIZE " << sunSize << "\n"
                 << "SUN_INTENSITY " << sunIntensity << "\n"
                 << "SUN_COLOR_R " << sunColor.r << "\n"
                 << "SUN_COLOR_G " << sunColor.g << "\n"
                 << "SUN_COLOR_B " << sunColor.b << "\n"
                 << "MOON_SIZE " << moonSize << "\n"
                 << "MOON_INTENSITY " << moonIntensity << "\n"
                 << "GOD_RAYS_INTENSITY " << godRaysIntensity << "\n"
                 << "STAR_DENSITY " << starDensity << "\n"
                 << "MILKY_WAY_INTENSITY " << milkyWayIntensity << "\n"
                 << "GRAPHICS_PRESET " << static_cast<int>(graphicsPreset) << "\n"
                 << "PLAYER_SPEED " << playerSpeed << "\n"
                 << "SPRINT_SPEED " << playerSprintSpeed << "\n"
                 << "SENSITIVITY " << mouseSensitivity << "\n"
                 << "ZOOM_FOV " << zoomBaseFov << "\n";
        }
        saveKeybinds();
    }
}
