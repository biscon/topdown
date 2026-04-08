#include "adventure/ItemDefinitionAsset.h"

#include <filesystem>
#include <fstream>

#include "raylib.h"
#include "resources/TextureAsset.h"
#include "utils/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static bool HasJsonExtension(const fs::path& p)
{
    return p.has_extension() && p.extension() == ".json";
}

bool LoadAllItemDefinitions(GameState& state)
{
    state.adventure.itemDefinitions.clear();

    const fs::path itemsDir = fs::path(ASSETS_PATH "items");
    if (!fs::exists(itemsDir) || !fs::is_directory(itemsDir)) {
        TraceLog(LOG_ERROR, "Items directory missing: %s", itemsDir.string().c_str());
        return false;
    }

    bool anyLoaded = false;

    for (const fs::directory_entry& entry : fs::directory_iterator(itemsDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path jsonPath = entry.path();
        if (!HasJsonExtension(jsonPath)) {
            continue;
        }

        json root;
        {
            std::ifstream in(jsonPath);
            if (!in.is_open()) {
                TraceLog(LOG_ERROR, "Failed to open item definition: %s", jsonPath.string().c_str());
                continue;
            }

            in >> root;
        }

        ItemDefinitionData item{};

        item.itemId = root.value("itemId", "");
        item.displayName = root.value("displayName", "");
        item.lookText = root.value("lookText", "");

        if (item.itemId.empty()) {
            TraceLog(LOG_ERROR, "Item missing itemId: %s", jsonPath.string().c_str());
            continue;
        }

        if (item.displayName.empty()) {
            TraceLog(LOG_ERROR, "Item missing displayName: %s", jsonPath.string().c_str());
            continue;
        }

        if (item.lookText.empty()) {
            TraceLog(LOG_ERROR, "Item missing lookText: %s", jsonPath.string().c_str());
            continue;
        }

        std::string iconRel = root.value("icon", "");
        if (iconRel.empty()) {
            iconRel = jsonPath.stem().string() + ".png";
        }

        const fs::path iconPath = (itemsDir / iconRel).lexically_normal();
        item.iconPath = NormalizePath(iconPath);
        item.iconTextureHandle = LoadTextureAsset(
                state.resources,
                item.iconPath.c_str(),
                ResourceScope::Global);

        if (item.iconTextureHandle < 0) {
            TraceLog(LOG_ERROR,
                     "Failed to load item icon for '%s': %s",
                     item.itemId.c_str(),
                     item.iconPath.c_str());
            continue;
        }

        bool duplicate = false;
        for (const ItemDefinitionData& existing : state.adventure.itemDefinitions) {
            if (existing.itemId == item.itemId) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            TraceLog(LOG_ERROR, "Duplicate itemId found: %s", item.itemId.c_str());
            continue;
        }

        state.adventure.itemDefinitions.push_back(item);
        anyLoaded = true;
    }

    TraceLog(LOG_INFO,
             "Loaded %d item definitions",
             static_cast<int>(state.adventure.itemDefinitions.size()));

    return anyLoaded;
}
