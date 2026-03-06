#pragma once

#include <vector>
#include <cstdint>

struct InventorySlot {
    uint8_t itemID = 0; // 0 represents Empty/Air
    int count = 0;
};

class Inventory {
public:
    Inventory();

    // Hotbar is indices 0-8. Main inventory is 9-35.
    std::vector<InventorySlot> slots;
    int selectedHotbarIndex = 0;

    uint8_t getSelectedBlock() const;
    void setSelectedHotbar(int index);
    
    // UI Helpers
    bool isVisible = false;
};
