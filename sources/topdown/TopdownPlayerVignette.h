#pragma once

#include "data/GameState.h"

bool InitTopdownPlayerVignetteSystem();
void ShutdownTopdownPlayerVignetteSystem();

bool ApplyTopdownPlayerVignette(
        GameState& state,
        const RenderTexture2D& source,
        RenderTexture2D& dest);
