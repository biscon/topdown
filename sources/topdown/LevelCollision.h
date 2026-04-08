#pragma once
#include "raylib.h"
#include "TopdownHelpers.h"

static constexpr int kCollisionIterations = 4;

Vector2 MoveTowardsVector(Vector2 current, Vector2 target, float maxDelta);
float MoveTowardsFloat(float current, float target, float maxDelta);
float NormalizeAngleRadians(float angle);
float MoveTowardsAngle(float current, float target, float maxDelta);
void ResolveCircleVsCircle(
        Vector2& position,
        Vector2& velocity,
        float radius,
        Vector2 otherPosition,
        float otherRadius,
        Vector2 preferredSeparationDir = Vector2{});
void ResolveCircleVsSegment(
        Vector2& position,
        Vector2& velocity,
        float radius,
        const TopdownSegment& seg);

bool RaycastCircleDetailed(
        Vector2 rayOrigin,
        Vector2 rayDir,
        Vector2 circleCenter,
        float radius,
        float maxDistance,
        float& outDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal);

bool RaycastClosestSegmentWithNormal(
        Vector2 origin,
        Vector2 dir,
        const std::vector<TopdownSegment>& segments,
        float maxDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance);