#pragma once

#include <string>

#include "data/GameState.h"
#include "utils/json.hpp"

bool LoadTopdownItemDefinitions(GameState& state);
const TopdownItemDefinition* FindTopdownItemDefinition(
        const GameState& state,
        const std::string& id);

void ImportTopdownItemLayer(
        GameState& state,
        const nlohmann::json& layer,
        int baseAssetScale);

void BuildTopdownRuntimeItemsFromAuthored(TopdownData& topdown);
void TopdownUpdateItems(GameState& state, float dt);
void TopdownRenderItems(GameState& state);
