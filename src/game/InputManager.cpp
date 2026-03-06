#include "game/InputManager.hpp"
#include <GLFW/glfw3.h>

std::unordered_map<std::string, int> InputManager::keyBindings_;
std::unordered_map<std::string, bool> InputManager::prevState_;

void InputManager::init() {
    // Minecraft-like defaults
    keyBindings_[InputAction::MOVE_FORWARD]  = GLFW_KEY_W;
    keyBindings_[InputAction::MOVE_BACK]     = GLFW_KEY_S;
    keyBindings_[InputAction::MOVE_LEFT]     = GLFW_KEY_A;
    keyBindings_[InputAction::MOVE_RIGHT]    = GLFW_KEY_D;
    keyBindings_[InputAction::JUMP]          = GLFW_KEY_SPACE;
    keyBindings_[InputAction::DESCEND]       = GLFW_KEY_LEFT_SHIFT;
    keyBindings_[InputAction::SPRINT]        = GLFW_KEY_LEFT_CONTROL;
    keyBindings_[InputAction::ZOOM]          = GLFW_KEY_C;
    keyBindings_[InputAction::INVENTORY]     = GLFW_KEY_E;
    keyBindings_[InputAction::COMMAND]       = GLFW_KEY_SLASH;
    keyBindings_[InputAction::FLY_BOOST]     = GLFW_KEY_LEFT_CONTROL;
}

bool InputManager::isPressed(const std::string& action, GLFWwindow* window) {
    auto it = keyBindings_.find(action);
    if (it == keyBindings_.end()) return false;
    return glfwGetKey(window, it->second) == GLFW_PRESS;
}

bool InputManager::wasJustPressed(const std::string& action, GLFWwindow* window) {
    bool current = isPressed(action, window);
    bool prev = prevState_[action];
    prevState_[action] = current;
    if (current && !prev) return true;
    return false;
}

void InputManager::setKey(const std::string& action, int glfwKeyCode) {
    keyBindings_[action] = glfwKeyCode;
}

int InputManager::getKey(const std::string& action) {
    auto it = keyBindings_.find(action);
    if (it != keyBindings_.end()) return it->second;
    return GLFW_KEY_UNKNOWN;
}

std::string InputManager::getKeyName(const std::string& action) {
    int key = getKey(action);
    if (key == GLFW_KEY_UNKNOWN) return "???";
    
    const char* name = glfwGetKeyName(key, 0);
    if (name) return std::string(name);
    
    // Handle special keys that glfwGetKeyName doesn't cover
    switch (key) {
        case GLFW_KEY_SPACE:         return "Space";
        case GLFW_KEY_LEFT_SHIFT:    return "L-Shift";
        case GLFW_KEY_RIGHT_SHIFT:   return "R-Shift";
        case GLFW_KEY_LEFT_CONTROL:  return "L-Ctrl";
        case GLFW_KEY_RIGHT_CONTROL: return "R-Ctrl";
        case GLFW_KEY_LEFT_ALT:      return "L-Alt";
        case GLFW_KEY_RIGHT_ALT:     return "R-Alt";
        case GLFW_KEY_TAB:           return "Tab";
        case GLFW_KEY_ESCAPE:        return "Escape";
        case GLFW_KEY_ENTER:         return "Enter";
        case GLFW_KEY_BACKSPACE:     return "Backspace";
        case GLFW_KEY_DELETE:        return "Delete";
        case GLFW_KEY_UP:            return "Up";
        case GLFW_KEY_DOWN:          return "Down";
        case GLFW_KEY_LEFT:          return "Left";
        case GLFW_KEY_RIGHT:         return "Right";
        case GLFW_KEY_F1:            return "F1";
        case GLFW_KEY_F2:            return "F2";
        case GLFW_KEY_F3:            return "F3";
        case GLFW_KEY_F4:            return "F4";
        case GLFW_KEY_F5:            return "F5";
        default:                     return "Key" + std::to_string(key);
    }
}

const std::unordered_map<std::string, int>& InputManager::getAllBindings() {
    return keyBindings_;
}
