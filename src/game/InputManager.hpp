#pragma once

struct GLFWwindow; // Forward declaration to avoid GLEW include order issues
#include <string>
#include <unordered_map>

// Named actions for configurable keybinds
namespace InputAction {
    constexpr const char* MOVE_FORWARD  = "moveForward";
    constexpr const char* MOVE_BACK     = "moveBack";
    constexpr const char* MOVE_LEFT     = "moveLeft";
    constexpr const char* MOVE_RIGHT    = "moveRight";
    constexpr const char* JUMP          = "jump";
    constexpr const char* DESCEND       = "descend";
    constexpr const char* SPRINT        = "sprint";
    constexpr const char* ZOOM          = "zoom";
    constexpr const char* INVENTORY     = "inventory";
    constexpr const char* COMMAND       = "command";
    constexpr const char* FLY_BOOST     = "flyBoost";
}

class InputManager {
public:
    static void init();

    // Key state queries
    static bool isPressed(const std::string& action, GLFWwindow* window);
    static bool wasJustPressed(const std::string& action, GLFWwindow* window);

    // Rebinding
    static void setKey(const std::string& action, int glfwKeyCode);
    static int  getKey(const std::string& action);
    static std::string getKeyName(const std::string& action);

    // All bindings for serialization
    static const std::unordered_map<std::string, int>& getAllBindings();

private:
    static std::unordered_map<std::string, int> keyBindings_;
    static std::unordered_map<std::string, bool> prevState_;
};
