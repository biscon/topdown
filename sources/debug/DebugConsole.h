#pragma once

#include "data/GameState.h"

void DebugConsoleInit(GameState& state);
void DebugConsoleShutdown();
void UpdateDebugConsole(GameState& state, float dt);
void RenderDebugConsole(const GameState& state);
void DebugConsoleAddLine(GameState& state, const std::string& text, Color color = LIGHTGRAY);
