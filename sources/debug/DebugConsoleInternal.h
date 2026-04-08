#pragma once

#include <string>
#include <vector>

#include "data/GameState.h"
#include "raylib.h"

static constexpr int CONSOLE_PADDING = 16;
static constexpr int CONSOLE_LINE_HEIGHT = 24;
static constexpr int CONSOLE_TOP = 80;
static constexpr int CONSOLE_HEIGHT = 620;
static constexpr int CONSOLE_FONT_SIZE = 28;

static constexpr int CONSOLE_INPUT_INNER_PADDING_X = 10;
static constexpr int CONSOLE_INPUT_INNER_PADDING_Y = 8;
static constexpr int CONSOLE_MAX_VISIBLE_INPUT_LINES = 3;

Font DebugConsoleGetActiveFont();

void DebugConsoleResetBlink(DebugConsoleData& console);
void DebugConsoleClampCaret(DebugConsoleData& console);
void DebugConsoleClampState(DebugConsoleData& console);

bool ExecuteConsoleSlashCommand(GameState& state, const std::string& line);
void UpdateDebugConsoleInputInternal(GameState& state, float dt);
void RenderDebugConsoleInternal(const GameState& state);

std::vector<std::string> WrapConsoleDisplayText(const std::string& text,
                                                const std::string& firstLinePrefix = "",
                                                const std::string& continuationPrefix = "");
