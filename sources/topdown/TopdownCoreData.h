#pragma once

#include "raylib.h"

using TopdownObstacleHandle = int;
using TopdownImageLayerHandle = int;
using TopdownCharacterHandle = int;

enum class TopdownBloodDecalKind {
    Spatter,
    Pool
};

enum class TopdownAttackType {
    None,
    Melee,
    Ranged
};

enum class TopdownAttackInput {
    Primary,
    Secondary
};

enum class TopdownTracerStyle {
    None,
    Handgun,
    Shotgun,
    Rifle
};

enum class TopdownFireMode {
    SemiAuto,
    FullAuto,
    Burst
};

struct TopdownSegment {
    Vector2 a{};
    Vector2 b{};
};
