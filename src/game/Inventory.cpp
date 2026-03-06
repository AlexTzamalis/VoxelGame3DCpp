#include "game/Inventory.hpp"
#include <algorithm>

Inventory::Inventory() {
    slots.resize(36);
    // Initialize default starter hotbar
    slots[0] = {2, 64}; // Grass
    slots[1] = {3, 64}; // Dirt
    slots[2] = {4, 64}; // Stone
    slots[3] = {6, 64}; // Wood Trunk
    slots[4] = {7, 64}; // Leaves
    slots[5] = {5, 64}; // Water (if placeable)
    slots[6] = {8, 64}; // Coal Ore
}

uint8_t Inventory::getSelectedBlock() const {
    if (selectedHotbarIndex >= 0 && selectedHotbarIndex < 9) {
        return slots[selectedHotbarIndex].itemID;
    }
    return 0; // Air
}

void Inventory::setSelectedHotbar(int index) {
    if (index >= 0 && index < 9) {
        selectedHotbarIndex = index;
    }
}
