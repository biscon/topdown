#pragma once

#include <string>
#include <vector>
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
bool TopdownScriptSpawnNpcSmart(
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
bool TopdownScriptAssignNpcPatrolRoute(
        GameState& state,
        const std::string& npcId,
        const std::vector<std::string>& spawnIds,
        bool loop = true,
        bool running = false,
        float waitMs = 0.0f);
bool TopdownScriptClearNpcPatrol(GameState& state, const std::string& npcId);
bool TopdownScriptPauseNpcPatrol(GameState& state, const std::string& npcId);
bool TopdownScriptResumeNpcPatrol(GameState& state, const std::string& npcId);

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

// Audio
bool TopdownScriptPlaySound(GameState& state, const std::string& audioId);
bool TopdownScriptStopSound(GameState& state, const std::string& audioId);
bool TopdownScriptPlayMusic(GameState& state, const std::string& audioId, float fadeMs = 0.0f);
bool TopdownScriptStopMusic(GameState& state, float fadeMs = 0.0f);

bool TopdownScriptSetSoundEmitterEnabled(GameState& state, const std::string& emitterId, bool enabled);
bool TopdownScriptGetSoundEmitterEnabled(const GameState& state, const std::string& emitterId, bool& outEnabled);
bool TopdownScriptSetSoundEmitterVolume(GameState& state, const std::string& emitterId, float volume);
bool TopdownScriptPlayEmitter(GameState& state, const std::string& emitterId);
bool TopdownScriptStopEmitter(GameState& state, const std::string& emitterId);

bool TopdownScriptShakeScreen(GameState& state,
                              float durationMs,
                              float strengthPx,
                              float frequencyHz = 30.0f,
                              bool smooth = false);
