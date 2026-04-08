#pragma once

#include <string>
#include <variant>
#include <vector>
#include "raylib.h"

enum class AdventureActionType {
    WalkToPoint,
    UseHotspot,
    LookHotspot,
    UseExit,
    LookExit,
    UseActor,
    LookActor
};

struct WalkToPointAction {
    Vector2 clickWorld{};
    Vector2 walkTarget{};
    bool fastMove = false;
};

struct UseHotspotAction {
    int hotspotIndex = -1;
    Vector2 clickWorld{};
    bool fastMove = false;
};

struct LookHotspotAction {
    int hotspotIndex = -1;
    Vector2 clickWorld{};
};

struct UseExitAction {
    int exitIndex = -1;
    Vector2 clickWorld{};
    bool fastMove = false;
};

struct LookExitAction {
    int exitIndex = -1;
    Vector2 clickWorld{};
};

struct UseActorAction {
    int actorIndex = -1;
    Vector2 clickWorld{};
    bool fastMove = false;
};

struct LookActorAction {
    int actorIndex = -1;
    Vector2 clickWorld{};
};

using AdventureActionPayload = std::variant<
        WalkToPointAction,
        UseHotspotAction,
        LookHotspotAction,
        UseExitAction,
        LookExitAction,
        UseActorAction,
        LookActorAction
>;

struct AdventureAction {
    AdventureActionType type;
    AdventureActionPayload payload;
};

struct AdventureActionQueue {
    std::vector<AdventureAction> actions;

    void push(AdventureAction&& a) {
        actions.push_back(std::move(a));
    }

    bool pop(AdventureAction& out) {
        if (actions.empty()) return false;
        out = std::move(actions.front());
        actions.erase(actions.begin());
        return true;
    }

    void clear() {
        actions.clear();
    }
};
