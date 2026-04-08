#pragma once

#include "data/GameState.h"

void InstallDebugConsoleTraceLogHook();
void FlushPendingDebugConsoleTraceLog(GameState& state);
