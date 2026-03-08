#include "world/BlockRegistry.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

std::unordered_map<uint8_t, BlockData> BlockRegistry::blocks_;
std::unordered_map<std::string, uint8_t> BlockRegistry::nameToIdMap_;

void BlockRegistry::init() {
    blocks_.clear();
    nameToIdMap_.clear();

    // ID 0: Air
    registerBlock(BlockData(0, "air", "missing_texture", true, false));
    // ID 1: Debug filler
    registerBlock(BlockData(1, "debug", "missing_texture", true, false));

    // Vanilla blocks mappings from Chunk.cpp
    registerBlock(BlockData(2, "grass_block", "grass_block_top", "grass_block_side", "dirt", false, true));
    registerBlock(BlockData(3, "dirt", "dirt", false, true));
    registerBlock(BlockData(4, "stone", "stone", false, true));
    registerBlock(BlockData(5, "water", "water_still", true, false));
    registerBlock(BlockData(6, "oak_log", "oak_log_top", "oak_log", "oak_log_top", false, true));
    registerBlock(BlockData(7, "oak_leaves", "oak_leaves", true, true));
    registerBlock(BlockData(8, "coal_ore", "coal_ore", false, true));
    registerBlock(BlockData(9, "sand", "sand", false, true));
    registerBlock(BlockData(10, "snow", "snow", false, true));
    registerBlock(BlockData(11, "snowy_grass", "snow", "grass_block_snow", "dirt", false, true));
    registerBlock(BlockData(12, "bedrock", "bedrock", false, true));
    registerBlock(BlockData(13, "iron_ore", "iron_ore", false, true));
    registerBlock(BlockData(14, "diamond_ore", "diamond_ore", false, true));
    registerBlock(BlockData(15, "gold_ore", "gold_ore", false, true));
    registerBlock(BlockData(16, "lapis_ore", "lapis_ore", false, true));
    registerBlock(BlockData(17, "deepslate", "deepslate", false, true));
    registerBlock(BlockData(18, "redstone_ore", "redstone_ore", false, true));

    // Additional Blocks
    registerBlock(BlockData(19, "cobblestone", "cobblestone", false, true));
    registerBlock(BlockData(20, "oak_planks", "oak_planks", false, true));
    registerBlock(BlockData(21, "glass", "glass", true, true));
    
    // Hard stone and mountain blocks
    registerBlock(BlockData(22, "diorite", "diorite", false, true));
    registerBlock(BlockData(23, "granite", "granite", false, true));
    registerBlock(BlockData(24, "andesite", "andesite", false, true));
    registerBlock(BlockData(25, "tuff", "tuff", false, true));
    registerBlock(BlockData(26, "calcite", "calcite", false, true));
    registerBlock(BlockData(27, "gravel", "gravel", false, true));

    // Foliage and ground cover
    registerBlock(BlockData(28, "moss_block", "moss_block", false, true));
    registerBlock(BlockData(29, "podzol", "podzol_top", "podzol_side", "dirt", false, true));
    registerBlock(BlockData(30, "coarse_dirt", "coarse_dirt", false, true));
    registerBlock(BlockData(31, "tall_grass", "short_grass", true, false));
    registerBlock(BlockData(32, "fern", "fern", true, false));
    registerBlock(BlockData(33, "dead_bush", "dead_bush", true, false));
    registerBlock(BlockData(34, "kelp", "kelp", true, false));
}

void BlockRegistry::registerBlock(const BlockData& data) {
    blocks_[data.id] = data;
    nameToIdMap_[data.name] = data.id;
}

const BlockData& BlockRegistry::getBlock(uint8_t id) {
    auto it = blocks_.find(id);
    if (it != blocks_.end()) return it->second;
    return blocks_[0];
}

uint8_t BlockRegistry::getBlockId(const std::string& name) {
    auto it = nameToIdMap_.find(name);
    if (it != nameToIdMap_.end()) return it->second;
    return 0; 
}

void BlockRegistry::loadBlockStates(const std::string& assetsDir) {
    std::string modelsDir = assetsDir + "/models/block";
    if (!fs::exists(modelsDir)) return;
    
    // We already initialized our internal BlockRegistry.
    // Let's do a pass over our known blocks and see if there is a modern `.json` model
    // that overrides their texture mapping!
    for (auto& [id, block] : blocks_) {
        if (id == 0 || id == 1) continue; // Skip air/debug
        
        std::string jsonPath = modelsDir + "/" + block.name + ".json";
        if (fs::exists(jsonPath)) {
            try {
                std::ifstream f(jsonPath);
                json data = json::parse(f);
                if (data.contains("textures")) {
                    auto texNode = data["textures"];
                    
                    auto stripNS = [](std::string s) {
                        size_t pos = s.find("block/");
                        if (pos != std::string::npos) return s.substr(pos + 6);
                        pos = s.find("minecraft:block/");
                        if (pos != std::string::npos) return s.substr(pos + 16);
                        return s;
                    };

                    // Simple cube (all sides)
                    if (texNode.contains("all")) {
                        std::string allTex = stripNS(texNode["all"].get<std::string>());
                        block.texTop = allTex;
                        block.texBottom = allTex;
                        block.texSide = allTex;
                    } 
                    // Cross diagonal plant geometry!
                    else if (texNode.contains("cross")) {
                        std::string crossTex = stripNS(texNode["cross"].get<std::string>());
                        block.texTop = crossTex;
                        block.texBottom = crossTex;
                        block.texSide = crossTex;
                    }
                    // Column (logs) or Grass blocks
                    else {
                        if (texNode.contains("top")) block.texTop = stripNS(texNode["top"].get<std::string>());
                        if (texNode.contains("bottom")) block.texBottom = stripNS(texNode["bottom"].get<std::string>());
                        if (texNode.contains("side")) block.texSide = stripNS(texNode["side"].get<std::string>());
                        if (texNode.contains("end")) { // "end" is often used for logs
                            std::string endTex = stripNS(texNode["end"].get<std::string>());
                            block.texTop = endTex;
                            block.texBottom = endTex;
                        }
                    }
                    std::cout << "Advanced JSON Map Overrode Block: " << block.name << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse json model for " << block.name << ": " << e.what() << "\n";
            }
        }
    }
}
