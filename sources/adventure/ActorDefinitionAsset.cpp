#include "ActorDefinitionAsset.h"

#include <filesystem>
#include <fstream>

#include "utils/json.hpp"
#include "resources/AsepriteAsset.h"
#include "ui/TalkColors.h"
#include "raylib.h"
#include "adventure/AdventureHelpers.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static std::string NormalizePath(const fs::path& p)
    {
        return p.lexically_normal().string();
    }
}

bool EnsureActorDefinitionLoaded(GameState& state, const std::string& actorId, int* outDefIndex)
{
    if (outDefIndex != nullptr) {
        *outDefIndex = -1;
    }

    const int existingIndex = FindActorDefinitionIndexById(state, actorId);
    if (existingIndex >= 0) {
        if (outDefIndex != nullptr) {
            *outDefIndex = existingIndex;
        }
        return true;
    }

    const fs::path actorDir = fs::path(ASSETS_PATH "actors") / actorId;
    const fs::path actorFile = actorDir / "actor.json";
    const std::string actorFileNorm = NormalizePath(actorFile);

    json root;
    {
        std::ifstream in(actorFileNorm);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open actor definition: %s", actorFileNorm.c_str());
            return false;
        }
        in >> root;
    }

    ActorDefinitionData def;
    def.actorId = root.value("actorId", actorId);
    def.displayName = root.value("displayName", def.actorId);
    def.walkSpeed = root.value("walkSpeed", 240.0f);
    def.fastMoveMultiplier = root.value("fastMoveMultiplier", 2.0f);
    def.idleDelayMs = root.value("idleDelayMs", 200.0f);
    def.controllable = root.value("controllable", false);
    def.horizHitboxScale = root.value("horizHitboxScale", 1.0f);
    def.vertHitboxScale = root.value("vertHitboxScale", 1.0f);

    const std::string talkColorName = root.value("talkColor", "");
    if (!talkColorName.empty()) {
        Color talkColor{};
        if (TryGetTalkColorByName(talkColorName, talkColor)) {
            def.talkColor = talkColor;
        } else {
            TraceLog(LOG_WARNING,
                     "Unknown talkColor '%s' in actor definition: %s",
                     talkColorName.c_str(),
                     actorFileNorm.c_str());
        }
    }

    const std::string spriteAssetRel = root.value("spriteAsset", "");
    if (spriteAssetRel.empty()) {
        TraceLog(LOG_ERROR, "Actor definition missing spriteAsset: %s", actorFileNorm.c_str());
        return false;
    }

    const fs::path spriteAssetPath = (actorDir / spriteAssetRel).lexically_normal();
    def.spriteAssetHandle = LoadSpriteAsset(state.resources, spriteAssetPath.string().c_str());
    if (def.spriteAssetHandle < 0) {
        TraceLog(LOG_ERROR, "Failed to load actor sprite asset: %s", spriteAssetPath.string().c_str());
        return false;
    }

    const int index = static_cast<int>(state.adventure.actorDefinitions.size());
    state.adventure.actorDefinitions.push_back(def);

    if (outDefIndex != nullptr) {
        *outDefIndex = index;
    }

    TraceLog(LOG_INFO, "Loaded actor definition: %s", def.actorId.c_str());
    return true;
}
