#pragma once

#include <string>

#include "data/GameState.h"
#include "resources/ResourceData.h"
#include "topdown/TopdownData.h"

TopdownRuntimeImageLayer* TopdownFindRuntimeImageLayerByName(
        GameState& state,
        const std::string& name);
TopdownRuntimeEffectRegion* TopdownFindRuntimeEffectRegionById(
        GameState& state,
        const std::string& id);
TopdownRuntimeTrigger* TopdownFindRuntimeTriggerById(
        GameState& state,
        const std::string& id);
const TopdownRuntimeTrigger* TopdownFindRuntimeTriggerById(
        const GameState& state,
        const std::string& id);
TopdownNpcRuntime* TopdownFindActiveNpcById(
        GameState& state,
        const std::string& npcId);
const TopdownNpcRuntime* TopdownFindActiveNpcById(
        const GameState& state,
        const std::string& npcId);
TopdownRuntimeProp* TopdownFindActivePropById(
        GameState& state,
        const std::string& propId);
const TopdownRuntimeProp* TopdownFindActivePropById(
        const GameState& state,
        const std::string& propId);
const TopdownAuthoredSpawn* TopdownFindAuthoredSpawnById(
        const GameState& state,
        const std::string& spawnId);
const SpriteClip* TopdownFindSpriteClipByName(
        const SpriteAssetResource& sprite,
        const std::string& clipName);
