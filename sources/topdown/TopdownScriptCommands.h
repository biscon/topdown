#pragma once

#include <string>
#include "data/GameState.h"

// Controls
bool TopdownScriptSetControlsEnabled(GameState& state, bool enabled);

// Scripted player movement
bool TopdownScriptStartWalkTo(GameState& state, Vector2 target);
bool TopdownScriptStartRunTo(GameState& state, Vector2 target);
bool TopdownScriptStartWalkToSpawn(GameState& state, const std::string& spawnId);
bool TopdownScriptStartRunToSpawn(GameState& state, const std::string& spawnId);

// NPC lifecycle
bool TopdownScriptSpawnNpc(
        GameState& state,
        const std::string& npcId,
        const std::string& assetId,
        const std::string& spawnId,
        bool persistentChase = false);
bool TopdownScriptRemoveNpc(GameState& state, const std::string& npcId);

// NPC scripted movement
bool TopdownScriptStartWalkNpcTo(GameState& state, const std::string& npcId, Vector2 target);
bool TopdownScriptStartRunNpcTo(GameState& state, const std::string& npcId, Vector2 target);
bool TopdownScriptStartWalkNpcToSpawn(GameState& state, const std::string& npcId, const std::string& spawnId);
bool TopdownScriptStartRunNpcToSpawn(GameState& state, const std::string& npcId, const std::string& spawnId);

// NPC animation
bool TopdownScriptSetNpcAnimation(GameState& state, const std::string& npcId, const std::string& animationName);
bool TopdownScriptClearNpcAnimation(GameState& state, const std::string& npcId);
bool TopdownScriptPlayNpcAnimation(GameState& state, const std::string& npcId, const std::string& animationName);

// Image layers
bool TopdownScriptSetImageLayerVisible(GameState& state, const std::string& name, bool visible);
bool TopdownScriptIsImageLayerVisible(GameState& state, const std::string& name, bool& outVisible);

bool TopdownScriptSetImageLayerOpacity(GameState& state, const std::string& name, float opacity);
bool TopdownScriptGetImageLayerOpacity(GameState& state, const std::string& name, float& outOpacity);

// Effect regions
bool TopdownScriptSetEffectRegionVisible(GameState& state, const std::string& id, bool visible);
bool TopdownScriptIsEffectRegionVisible(GameState& state, const std::string& id, bool& outVisible);

bool TopdownScriptSetEffectRegionOpacity(GameState& state, const std::string& id, float opacity);
bool TopdownScriptGetEffectRegionOpacity(GameState& state, const std::string& id, float& outOpacity);

// Triggers
bool TopdownScriptSetTriggerEnabled(GameState& state, const std::string& triggerId, bool enabled);
bool TopdownScriptSetTriggerRepeat(GameState& state, const std::string& triggerId, bool repeat);
