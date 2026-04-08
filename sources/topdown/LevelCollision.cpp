#include <cmath>
#include "LevelCollision.h"
#include "TopdownHelpers.h"

static constexpr float kCollisionEpsilon = 0.001f;


Vector2 MoveTowardsVector(Vector2 current, Vector2 target, float maxDelta)
{
    const Vector2 delta = TopdownSub(target, current);
    const float dist = TopdownLength(delta);

    if (dist <= maxDelta || dist <= 0.000001f) {
        return target;
    }

    const Vector2 dir = TopdownMul(delta, 1.0f / dist);
    return TopdownAdd(current, TopdownMul(dir, maxDelta));
}

float MoveTowardsFloat(float current, float target, float maxDelta)
{
    if (current < target) {
        return std::min(current + maxDelta, target);
    }
    return std::max(current - maxDelta, target);
}

float NormalizeAngleRadians(float angle)
{
    static constexpr float kPi = 3.14159265358979323846f;

    while (angle <= -kPi) {
        angle += 2.0f * kPi;
    }
    while (angle > kPi) {
        angle -= 2.0f * kPi;
    }
    return angle;
}

float MoveTowardsAngle(float current, float target, float maxDelta)
{
    float delta = NormalizeAngleRadians(target - current);

    if (std::fabs(delta) <= maxDelta) {
        return target;
    }

    if (delta > 0.0f) {
        return NormalizeAngleRadians(current + maxDelta);
    }

    return NormalizeAngleRadians(current - maxDelta);
}

void ResolveCircleVsCircle(
        Vector2& position,
        Vector2& velocity,
        float radius,
        Vector2 otherPosition,
        float otherRadius,
        Vector2 preferredSeparationDir)
{
    Vector2 delta = TopdownSub(position, otherPosition);
    float dist = TopdownLength(delta);
    const float minDist = radius + otherRadius;

    if (dist >= minDist || minDist <= 0.0f) {
        return;
    }

    Vector2 normal{};

    if (dist > 0.000001f) {
        normal = TopdownMul(delta, 1.0f / dist);

        // soft directional bias:
        // if caller provided a preferred separation direction and it points
        // roughly away from the other body, blend a little toward it.
        if (TopdownLengthSqr(preferredSeparationDir) > 0.000001f) {
            Vector2 preferred = TopdownNormalizeOrZero(preferredSeparationDir);

            if (TopdownDot(normal, preferred) > -0.25f) {
                normal = TopdownNormalizeOrZero(
                        TopdownAdd(
                                TopdownMul(normal, 0.75f),
                                TopdownMul(preferred, 0.25f)));
            }
        }
    } else {
        if (TopdownLengthSqr(preferredSeparationDir) > 0.000001f) {
            normal = TopdownNormalizeOrZero(preferredSeparationDir);
        } else if (TopdownLengthSqr(velocity) > 0.000001f) {
            // sideways bias instead of forward bias looks better for crowds
            normal = Vector2{ -velocity.y, velocity.x };
            normal = TopdownNormalizeOrZero(normal);

            if (TopdownLengthSqr(normal) <= 0.000001f) {
                normal = Vector2{1.0f, 0.0f};
            }
        } else {
            normal = Vector2{1.0f, 0.0f};
        }

        dist = 0.0f;
    }

    const float push = minDist - dist + kCollisionEpsilon;
    position = TopdownAdd(position, TopdownMul(normal, push));

    const float vn = TopdownDot(velocity, normal);
    if (vn < 0.0f) {
        velocity = TopdownSub(velocity, TopdownMul(normal, vn));
    }
}

void ResolveCircleVsSegment(
        Vector2& position,
        Vector2& velocity,
        float radius,
        const TopdownSegment& seg)
{
    const Vector2 closest = TopdownClosestPointOnSegment(position, seg);
    Vector2 delta = TopdownSub(position, closest);
    float dist = TopdownLength(delta);

    if (dist >= radius) {
        return;
    }

    Vector2 normal{};

    if (dist > 0.000001f) {
        normal = TopdownMul(delta, 1.0f / dist);
    } else {
        const Vector2 ab = TopdownSub(seg.b, seg.a);
        const Vector2 perp{-ab.y, ab.x};
        normal = TopdownNormalizeOrZero(perp);

        if (TopdownLengthSqr(normal) <= 0.000001f) {
            if (TopdownLengthSqr(velocity) > 0.000001f) {
                normal = TopdownNormalizeOrZero(velocity);
            } else {
                normal = Vector2{1.0f, 0.0f};
            }
        }
    }

    position = TopdownAdd(closest, TopdownMul(normal, radius + kCollisionEpsilon));

    const float vn = TopdownDot(velocity, normal);
    if (vn < 0.0f) {
        velocity = TopdownSub(velocity, TopdownMul(normal, vn));
    }
}

bool RaycastCircleDetailed(
        Vector2 rayOrigin,
        Vector2 rayDir,
        Vector2 circleCenter,
        float radius,
        float maxDistance,
        float& outDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal)
{
    const Vector2 originToCenter = TopdownSub(circleCenter, rayOrigin);
    const float radiusSqr = radius * radius;
    const float originDistSqr = TopdownLengthSqr(originToCenter);

    // Special case: shot starts inside the circle.
    if (originDistSqr <= radiusSqr) {
        outDistance = 0.0f;
        outHitPoint = rayOrigin;
        outHitNormal = TopdownMul(rayDir, -1.0f);

        if (TopdownLengthSqr(outHitNormal) <= 0.000001f) {
            outHitNormal = Vector2{1.0f, 0.0f};
        }

        return true;
    }

    const float tClosest = TopdownDot(originToCenter, rayDir);
    if (tClosest < 0.0f || tClosest > maxDistance) {
        return false;
    }

    const Vector2 closestPoint = TopdownAdd(rayOrigin, TopdownMul(rayDir, tClosest));
    const Vector2 diff = TopdownSub(circleCenter, closestPoint);

    const float distSqr = TopdownLengthSqr(diff);
    if (distSqr > radiusSqr) {
        return false;
    }

    float thc = std::sqrt(radiusSqr - distSqr);
    float tHit = tClosest - thc;

    if (tHit < 0.0f) {
        tHit = tClosest + thc;
    }

    if (tHit < 0.0f || tHit > maxDistance) {
        return false;
    }

    outDistance = tHit;
    outHitPoint = TopdownAdd(rayOrigin, TopdownMul(rayDir, tHit));
    outHitNormal = TopdownNormalizeOrZero(TopdownSub(outHitPoint, circleCenter));

    if (TopdownLengthSqr(outHitNormal) <= 0.000001f) {
        outHitNormal = TopdownMul(rayDir, -1.0f);
    }

    return true;
}

static Vector2 ComputeSegmentHitNormal(
        const TopdownSegment& seg,
        Vector2 rayDir)
{
    const Vector2 edge = TopdownSub(seg.b, seg.a);

    Vector2 n0{ -edge.y, edge.x };
    Vector2 n1{ edge.y, -edge.x };

    n0 = TopdownNormalizeOrZero(n0);
    n1 = TopdownNormalizeOrZero(n1);

    if (TopdownLengthSqr(n0) <= 0.000001f) {
        return TopdownMul(rayDir, -1.0f);
    }

    // Pick the normal that faces against the incoming shot direction.
    if (TopdownDot(n0, rayDir) < TopdownDot(n1, rayDir)) {
        return n0;
    }

    return n1;
}


bool RaycastClosestSegmentWithNormal(
        Vector2 origin,
        Vector2 dir,
        const std::vector<TopdownSegment>& segments,
        float maxDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance)
{
    Vector2 bestPoint{};
    Vector2 bestNormal{};
    float bestDistance = maxDistance;
    bool hit = false;

    for (const TopdownSegment& seg : segments) {
        Vector2 point{};
        float distance = maxDistance;

        std::vector<TopdownSegment> single{ seg };
        if (!TopdownRaycastSegments(origin, dir, single, bestDistance, point, &distance)) {
            continue;
        }

        if (distance < bestDistance) {
            bestDistance = distance;
            bestPoint = point;
            bestNormal = ComputeSegmentHitNormal(seg, dir);
            hit = true;
        }
    }

    if (!hit) {
        return false;
    }

    outHitPoint = bestPoint;
    outHitNormal = bestNormal;
    outHitDistance = bestDistance;
    return true;
}


