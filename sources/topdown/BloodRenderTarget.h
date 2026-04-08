#pragma once

#include "data/GameState.h"

bool InitTopdownBloodRenderTargetSystem();
void ShutdownTopdownBloodRenderTargetSystem();

bool EnsureTopdownBloodRenderTarget(GameState& state, int width, int height);
void UnloadTopdownBloodRenderTarget(GameState& state);

void MarkTopdownBloodRenderTargetDirty(GameState& state);
void RebuildTopdownBloodRenderTargetIfNeeded(GameState& state);

void DrawTopdownBloodRenderTargetToWorld(GameState& state);