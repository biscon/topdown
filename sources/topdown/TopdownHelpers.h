#pragma once

#include <vector>
#include <functional>
#include "raylib.h"
#include "topdown/TopdownData.h"
#include "data/GameState.h"

float TopdownSignedPolygonArea(const std::vector<Vector2>& points);
bool TopdownIsClockwise(const std::vector<Vector2>& points);
void TopdownEnsureClockwise(std::vector<Vector2>& points);
void TopdownEnsureCounterClockwise(std::vector<Vector2>& points);

Rectangle TopdownComputePolygonBounds(const std::vector<Vector2>& points);
std::vector<TopdownSegment> TopdownBuildSegmentsFromPolygon(const std::vector<Vector2>& points);

std::vector<Vector2> TopdownBuildRectPolygon(float x, float y, float width, float height, float scale);

float TopdownDot(Vector2 a, Vector2 b);
float TopdownLengthSqr(Vector2 v);
float TopdownLength(Vector2 v);
Vector2 TopdownAdd(Vector2 a, Vector2 b);
Vector2 TopdownSub(Vector2 a, Vector2 b);
Vector2 TopdownMul(Vector2 v, float s);
Vector2 TopdownNormalizeOrZero(Vector2 v);

Vector2 TopdownClosestPointOnSegment(Vector2 p, const TopdownSegment& seg);
bool TopdownPointInPolygon(Vector2 p, const std::vector<Vector2>& polygon);
Vector2 TopdownComputeEffectRegionOcclusionOrigin(
        const TopdownAuthoredEffectRegion& effect);

Vector2 TopdownWorldToScreen(const GameState& state, Vector2 worldPos);

bool TopdownRaycastSegments(
        Vector2 origin,
        Vector2 dirNormalized,
        const std::vector<TopdownSegment>& segments,
        float maxDistance,
        Vector2& outHitPoint,
        float* outHitDistance = nullptr);

Vector2 GetMouseWorldPosition(const GameState& state);
float RandomRangeFloat(float minValue, float maxValue);
Vector2 RotateVector(Vector2 v, float radians);

Vector2 TopdownPerpRight(Vector2 v);

Vector2 TopdownGetDoorDirection(float angleRadians);
TopdownSegment TopdownBuildDoorCenterSegment(const TopdownRuntimeDoor& door);

void TopdownBuildDoorCorners(
        const TopdownRuntimeDoor& door,
        Vector2& outA,
        Vector2& outB,
        Vector2& outC,
        Vector2& outD);

Vector2 TopdownBuildNpcPathSteeringTarget(
        const TopdownNpcRuntime& npc,
        float lookaheadDistance);

void TopdownPushWorldEvent(
        GameState& state,
        TopdownWorldEventType type,
        Vector2 position,
        float radius,
        TopdownWorldEventSourceType sourceType = TopdownWorldEventSourceType::None,
        int sourceNpcHandle = -1,
        float ttl = 150.0f);

void TopdownForEachWorldEventOfType(
        const GameState& state,
        TopdownWorldEventType type,
        const std::function<void(const TopdownWorldEvent&)>& fn);