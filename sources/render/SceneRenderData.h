#pragma once

#include "raylib.h"
#include "data/GameState.h"

enum class SceneRenderWorldDrawItemType {
    Actor,
    Prop,
    EffectSprite,
    EffectRegion
};

struct SceneRenderWorldDrawItem {
    float sortY = 0.0f;
    int sortOrder = 0;
    SceneRenderWorldDrawItemType type = SceneRenderWorldDrawItemType::Actor;
    int actorIndex = -1;
    int propIndex = -1;
    int effectIndex = -1;
    int effectRegionIndex = -1;
};
