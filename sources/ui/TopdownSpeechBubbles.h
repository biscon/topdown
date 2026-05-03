#pragma once

#include <string>
#include "raylib.h"

struct GameState;

bool TopdownShowPlayerSpeechBubble(
        GameState& state,
        const std::string& text,
        float durationMs,
        Color color);

bool TopdownShowNpcSpeechBubble(
        GameState& state,
        const std::string& npcId,
        const std::string& text,
        float durationMs,
        Color color);

bool TopdownShowPropSpeechBubble(
        GameState& state,
        const std::string& propId,
        const std::string& text,
        float durationMs,
        Color color);

void TopdownUpdateSpeechBubbles(GameState& state, float dt);
void TopdownRenderSpeechBubbles(GameState& state);
