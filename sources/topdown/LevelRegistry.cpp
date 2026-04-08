#include "topdown/LevelRegistry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "topdown/LevelLoad.h"
#include "utils/json.hpp"
#include "topdown/NpcRegistry.h"


using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

const TopdownLevelRegistryEntry* FindTopdownLevelRegistryEntryById(const GameState& state, const char* levelId)
{
    if (levelId == nullptr || levelId[0] == '\0') {
        return nullptr;
    }

    for (const TopdownLevelRegistryEntry& entry : state.topdown.levelRegistry) {
        if (entry.levelId == levelId) {
            return &entry;
        }
    }

    return nullptr;
}

bool TopdownScanLevelRegistry(GameState& state)
{
    state.topdown.levelRegistry.clear();
    TopdownScanNpcRegistry(state);

    const fs::path levelsDir = fs::path(ASSETS_PATH "levels");
    if (!fs::exists(levelsDir) || !fs::is_directory(levelsDir)) {
        TraceLog(LOG_ERROR, "Topdown levels directory missing: %s", levelsDir.string().c_str());
        return false;
    }

    for (const auto& entry : fs::directory_iterator(levelsDir)) {
        if (!entry.is_directory()) {
            continue;
        }

        const fs::path levelDir = entry.path();
        const std::string levelId = levelDir.filename().string();

        const fs::path metadataPath = (levelDir / (levelId + ".json")).lexically_normal();
        const fs::path tiledPath = (levelDir / (levelId + ".tmj")).lexically_normal();

        if (!fs::exists(metadataPath)) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown level '%s': missing metadata file %s",
                     levelId.c_str(),
                     metadataPath.string().c_str());
            continue;
        }

        if (!fs::exists(tiledPath)) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown level '%s': missing tmj file %s",
                     levelId.c_str(),
                     tiledPath.string().c_str());
            continue;
        }

        json root;
        {
            std::ifstream in(metadataPath);
            if (!in.is_open()) {
                TraceLog(LOG_WARNING,
                         "Skipping topdown level '%s': failed opening metadata file",
                         levelId.c_str());
                continue;
            }
            in >> root;
        }

        TopdownLevelRegistryEntry reg;
        reg.levelId = root.value("levelId", levelId);
        reg.saveName = root.value("saveName", reg.levelId);
        reg.baseAssetScale = root.value("baseAssetScale", 1);
        reg.metadataFilePath = NormalizePath(metadataPath);
        reg.tiledFilePath = NormalizePath(tiledPath);
        reg.levelDirectoryPath = NormalizePath(levelDir);

        if (reg.levelId != levelId) {
            TraceLog(LOG_WARNING,
                     "Topdown level metadata id '%s' does not match directory '%s'; using directory name",
                     reg.levelId.c_str(),
                     levelId.c_str());
            reg.levelId = levelId;
        }

        state.topdown.levelRegistry.push_back(reg);
    }

    std::sort(
            state.topdown.levelRegistry.begin(),
            state.topdown.levelRegistry.end(),
            [](const TopdownLevelRegistryEntry& a, const TopdownLevelRegistryEntry& b) {
                return a.levelId < b.levelId;
            });

    TraceLog(LOG_INFO,
             "Scanned topdown level registry: %d levels",
             static_cast<int>(state.topdown.levelRegistry.size()));

    for (const TopdownLevelRegistryEntry& entry : state.topdown.levelRegistry) {
        TraceLog(LOG_INFO,
                 "  level=%s saveName=%s scale=%d tmj=%s",
                 entry.levelId.c_str(),
                 entry.saveName.c_str(),
                 entry.baseAssetScale,
                 entry.tiledFilePath.c_str());
    }

    return true;
}

bool TopdownLoadLevelById(GameState& state, const char* levelId, const char* spawnId)
{
    const TopdownLevelRegistryEntry* entry = FindTopdownLevelRegistryEntryById(state, levelId);
    if (entry == nullptr) {
        TraceLog(LOG_ERROR, "Unknown topdown level id: %s", levelId != nullptr ? levelId : "<null>");
        return false;
    }

    state.topdown.pendingSpawnId = (spawnId != nullptr) ? spawnId : "";
    return TopdownLoadLevel(state, entry->tiledFilePath.c_str(), entry->baseAssetScale);
}

bool TopdownLoadLevelById(GameState& state, const char* levelId)
{
    return TopdownLoadLevelById(state, levelId, "");
}

bool TopdownReloadCurrentLevel(GameState& state)
{
    if (state.topdown.currentLevelId.empty()) {
        TraceLog(LOG_WARNING, "No current topdown level to reload");
        return false;
    }

    return TopdownLoadLevelById(state, state.topdown.currentLevelId.c_str(), "");
}

bool TopdownHasActiveOrResumableLevel(const GameState& state)
{
    return !state.topdown.currentLevelId.empty();
}
