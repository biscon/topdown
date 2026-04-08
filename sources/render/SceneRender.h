#pragma once

#include "data/GameState.h"

bool ApplySceneSampleEffectRegionPass(
        const GameState& state,
        int effectRegionIndex,
        const RenderTexture2D& sourceTarget,
        RenderTexture2D& destTarget);

void RenderAdventureSceneComposited(
        const GameState& state,
        RenderTexture2D& worldTarget,
        RenderTexture2D& tempTarget);

void RenderAdventureSceneFadeOverlay(const GameState& state);
