// BlockRegistry.hpp
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct BlockData {
    uint8_t id;
    std::string name;
    
    // Default textures. We can expand this with BlockStates later depending on what JSON provides.
    std::string texTop;
    std::string texSide;
    std::string texBottom;
    
    bool isTransparent;
    bool isSolid;

    BlockData() : id(0), name("air"), texTop(""), texSide(""), texBottom(""), isTransparent(true), isSolid(false) {}
    BlockData(uint8_t pid, std::string pname, std::string ptex, bool ptrans, bool psolid)
        : id(pid), name(pname), texTop(ptex), texSide(ptex), texBottom(ptex), isTransparent(ptrans), isSolid(psolid) {}
    BlockData(uint8_t pid, std::string pname, std::string pt, std::string ps, std::string pb, bool ptrans, bool psolid)
        : id(pid), name(pname), texTop(pt), texSide(ps), texBottom(pb), isTransparent(ptrans), isSolid(psolid) {}
};

class BlockRegistry {
public:
    static void init();
    static const BlockData& getBlock(uint8_t id);
    static uint8_t getBlockId(const std::string& name);
    
    // Add dynamically parsed block states here
    static void loadBlockStates(const std::string& blockstatesDir);
    
private:
    static std::unordered_map<uint8_t, BlockData> blocks_;
    static std::unordered_map<std::string, uint8_t> nameToIdMap_;
    static void registerBlock(const BlockData& data);
};
