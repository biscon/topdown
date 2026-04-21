#pragma once

#include <string>

struct GameState;

bool TopdownQueueNarrationPopup(
        GameState& state,
        const std::string& title,
        const std::string& body,
        float durationSeconds = 0.0f);

void TopdownUpdateNarrationPopups(GameState& state, float dt);
void TopdownRenderNarrationPopups(GameState& state);