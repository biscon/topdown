#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "resources/ResourceData.h"

enum class TopdownItemKind {
    Unknown,
    Ammo,
    Health
};

struct TopdownItemDefinition {
    std::string id;
    std::string displayName;
    TopdownItemKind kind = TopdownItemKind::Unknown;

    std::string ammoType;
    int amount = 0;

    float healAmount = 0.0f;
    float consumeMs = 0.0f;

    std::string texturePath;
    TextureHandle textureHandle = -1;
};

struct TopdownItemRegistry {
    bool loaded = false;
    std::vector<TopdownItemDefinition> definitions;
};

struct TopdownAuthoredItem {
    int tiledObjectId = -1;
    std::string id;
    std::string itemId;
    Vector2 position{};
    bool visible = true;
};

struct TopdownRuntimeItem {
    int authoredIndex = -1;
    int tiledObjectId = -1;
    std::string id;
    std::string itemId;
    Vector2 position{};
    bool active = true;
    bool visible = true;
};
