#pragma once

#include "data/GameState.h"
#include "topdown/TopdownData.h"

TopdownRuntimeWindow TopdownBuildRuntimeWindowFromAuthored(
        const TopdownAuthoredWindow& authored);

bool TopdownGenerateWindowTextureAtlas(
        TopdownRuntimeWindow& window,
        int baseAssetScale);

void TopdownUnloadWindowResources(TopdownData& topdown);

void TopdownUpdateWindows(GameState& state, float dt);
void TopdownRenderWindows(const GameState& state);
void TopdownRenderWindowGlassParticles(const GameState& state);

bool RaycastClosestWindow(
        GameState& state,
        Vector2 origin,
        Vector2 dirNormalized,
        float maxDistance,
        TopdownRuntimeWindow*& outWindow,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance);

bool BreakWindow(
        GameState& state,
        TopdownRuntimeWindow& window,
        Vector2 hitPoint,
        Vector2 shotDir);
