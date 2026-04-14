#pragma once

#include <string>
#include "data/GameState.h"

bool TopdownScanNpcRegistry(GameState& state);

const TopdownNpcAssetDefinition* FindTopdownNpcAssetDefinition(
        const GameState& state,
        const std::string& assetId);

TopdownNpcAssetRuntime* FindTopdownNpcAssetRuntime(
        GameState& state,
        const std::string& assetId);

const TopdownNpcAssetRuntime* FindTopdownNpcAssetRuntime(
        const GameState& state,
        const std::string& assetId);

bool EnsureTopdownNpcAssetLoaded(GameState& state, const std::string& assetId);

bool TopdownNpcClipRefIsValid(const TopdownNpcClipRef& clipRef);

void TopdownSetNpcAutomaticLoopAnimation(
        TopdownNpcRuntime& npc,
        const TopdownNpcClipRef& clipRef);

void TopdownSetNpcScriptLoopAnimation(
        TopdownNpcRuntime& npc,
        const TopdownNpcClipRef& clipRef);

void TopdownClearNpcScriptLoopAnimation(TopdownNpcRuntime& npc);

void TopdownPlayNpcOneShotAnimation(
        TopdownNpcRuntime& npc,
        const TopdownNpcClipRef& clipRef);

void TopdownClearNpcOneShotAnimation(TopdownNpcRuntime& npc);

const TopdownNpcClipRef* TopdownGetResolvedNpcAnimationClip(const TopdownNpcRuntime& npc);
std::string TopdownGetResolvedNpcAnimationName(const TopdownNpcRuntime& npc);

TopdownNpcClipRef TopdownMakeNpcClipRef(
        SpriteAssetHandle spriteHandle,
        int clipIndex,
        const char* clipName);

TopdownNpcClipRef FindTopdownNpcClipByName(
        const GameState& state,
        const TopdownNpcAssetRuntime& asset,
        const std::string& clipName);

bool TopdownSpawnNpcRuntime(
        GameState& state,
        const std::string& npcId,
        const std::string& assetId,
        Vector2 position,
        float orientationDegrees,
        bool visible,
        bool persistentChase = false,
        bool smartPlacement = false);
