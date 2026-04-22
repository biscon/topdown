#include "topdown/LevelDoors.h"

#include <cmath>
#include <algorithm>

#include "topdown/TopdownHelpers.h"
#include "topdown/LevelCollision.h"
#include "audio/Audio.h"
#include "topdown/NpcUpdate.h"
#include "raymath.h"

static constexpr float kDoorAngleEpsilon = 0.0001f;
static constexpr float kDoorNearClosedRadians = 6.0f * DEG2RAD;
static constexpr float kDoorOpenSoundMinAngularSpeed = 0.8f;
static constexpr float kDoorCloseSoundMinAngularSpeed = 0.5f;

static constexpr float kDoorPushTorqueScale = 0.00026f;
static constexpr float kDoorMotionPushPlayerScale = 0.55f;
static constexpr float kDoorMotionPushNpcScale = 1.5f;
static constexpr float kDoorNpcKnockbackScale = 0.18f;

static float ClampFloat(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float TopdownCross(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
}

static float ComputeDoorRelativeAngle(const TopdownRuntimeDoor& door)
{
    return NormalizeAngleRadians(door.angleRadians - door.closedAngleRadians);
}

static float ComputeDoorWorldMinAngle(const TopdownRuntimeDoor& door)
{
    return door.closedAngleRadians + door.swingMinRadians;
}

static float ComputeDoorWorldMaxAngle(const TopdownRuntimeDoor& door)
{
    return door.closedAngleRadians + door.swingMaxRadians;
}

static Vector2 ComputeDoorClosestPoint(
        const TopdownRuntimeDoor& door,
        Vector2 point,
        float* outLeverT = nullptr)
{
    const TopdownSegment seg = TopdownBuildDoorCenterSegment(door);
    const Vector2 ab = TopdownSub(seg.b, seg.a);
    const Vector2 ap = TopdownSub(point, seg.a);
    const float denom = TopdownDot(ab, ab);

    float t = 0.0f;
    if (denom > 0.000001f) {
        t = TopdownDot(ap, ab) / denom;
        t = Clamp(t, 0.0f, 1.0f);
    }

    if (outLeverT != nullptr) {
        *outLeverT = t;
    }

    return TopdownAdd(seg.a, TopdownMul(ab, t));
}

static bool ResolveCircleVsDoorCapsule(
        Vector2& position,
        Vector2& velocity,
        float radius,
        const TopdownRuntimeDoor& door,
        float* outLeverT = nullptr,
        Vector2* outNormal = nullptr,
        float* outPenetration = nullptr)
{
    const float doorRadius = door.thickness * 0.5f;
    const float minDist = radius + doorRadius;

    float leverT = 0.0f;
    const Vector2 closest = ComputeDoorClosestPoint(door, position, &leverT);

    const TopdownSegment seg = TopdownBuildDoorCenterSegment(door);
    Vector2 ab = TopdownSub(seg.b, seg.a);
    const float segLen = TopdownLength(ab);

    if (segLen <= 0.000001f) {
        return false;
    }

    ab = TopdownMul(ab, 1.0f / segLen);

    Vector2 delta = TopdownSub(position, closest);
    float dist = TopdownLength(delta);

    if (dist >= minDist) {
        return false;
    }

    const Vector2 rightNormal = TopdownPerpRight(ab);
    const Vector2 leftNormal = TopdownMul(rightNormal, -1.0f);

    Vector2 normal{};

    // Treat the end regions as round caps, not as sided slab contacts.
    static constexpr float kEndCapThreshold = 0.008f;

    const bool nearHinge = (leverT <= kEndCapThreshold);
    const bool nearTip = (leverT >= 1.0f - kEndCapThreshold);
    const bool useCapNormal = nearHinge || nearTip;

    if (useCapNormal) {
        if (dist > 0.000001f) {
            normal = TopdownMul(delta, 1.0f / dist);
        } else {
            if (TopdownLengthSqr(velocity) > 0.000001f) {
                normal = TopdownNormalizeOrZero(TopdownMul(velocity, -1.0f));
            } else {
                normal = rightNormal;
            }
            dist = 0.0f;
        }

        const float penetration = minDist - dist + 0.001f;

        position = TopdownAdd(closest, TopdownMul(normal, minDist + 0.001f));

        const float vn = TopdownDot(velocity, normal);
        if (vn < 0.0f) {
            velocity = TopdownSub(velocity, TopdownMul(normal, vn));
        }

        if (outLeverT != nullptr) {
            *outLeverT = leverT;
        }
        if (outNormal != nullptr) {
            *outNormal = normal;
        }
        if (outPenetration != nullptr) {
            *outPenetration = penetration;
        }

        return true;
    }

    // Body contact: keep actor on the side they approached from.
    const float signedDist = TopdownDot(delta, rightNormal);
    const float absDist = std::fabs(signedDist);

    if (signedDist > 0.0001f) {
        normal = rightNormal;
    } else if (signedDist < -0.0001f) {
        normal = leftNormal;
    } else {
        const float towardRight = TopdownDot(velocity, rightNormal);
        const float towardLeft = TopdownDot(velocity, leftNormal);

        if (towardRight < towardLeft) {
            normal = rightNormal;
        } else if (towardLeft < towardRight) {
            normal = leftNormal;
        } else if (TopdownLengthSqr(delta) > 0.000001f) {
            normal = TopdownNormalizeOrZero(delta);
        } else {
            normal = rightNormal;
        }
    }

    const float penetration = minDist - absDist + 0.001f;

    position = TopdownAdd(closest, TopdownMul(normal, minDist + 0.001f));

    const float vn = TopdownDot(velocity, normal);
    if (vn < 0.0f) {
        velocity = TopdownSub(velocity, TopdownMul(normal, vn));
    }

    if (outLeverT != nullptr) {
        *outLeverT = leverT;
    }
    if (outNormal != nullptr) {
        *outNormal = normal;
    }
    if (outPenetration != nullptr) {
        *outPenetration = penetration;
    }

    return true;
}

static void ApplyAngularImpulseFromActor(
        TopdownRuntimeDoor& door,
        Vector2 actorVelocity,
        Vector2 contactNormal,
        float leverT,
        float penetration)
{
    const Vector2 dir = TopdownGetDoorDirection(door.angleRadians);

    const float towardDoorSpeed = std::max(0.0f, -TopdownDot(actorVelocity, contactNormal));
    if (towardDoorSpeed <= 0.0f && penetration <= 0.0f) {
        return;
    }

    leverT = ClampFloat(leverT, 0.0f, 1.0f);

    const float leverFactor =
            0.45f + 0.55f * std::sqrt(leverT);

    const float leverArm = door.length * leverFactor;
    const float tangentialSign = -TopdownCross(dir, contactNormal);

    const float pushStrength =
            towardDoorSpeed +
            penetration * 35.0f;

    door.angularVelocity +=
            tangentialSign *
            leverArm *
            pushStrength *
            kDoorPushTorqueScale;
}

static Vector2 ComputeDoorPointVelocity(
        const TopdownRuntimeDoor& door,
        Vector2 worldPoint)
{
    const Vector2 r = TopdownSub(worldPoint, door.hinge);
    return Vector2{
            -door.angularVelocity * r.y,
            door.angularVelocity * r.x
    };
}

static bool RaycastDoorCapsuleDetailed(
        const TopdownRuntimeDoor& door,
        Vector2 origin,
        Vector2 dirNormalized,
        float maxDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance)
{
    const float radius = door.thickness * 0.5f;
    const TopdownSegment center = TopdownBuildDoorCenterSegment(door);

    Vector2 edge = TopdownSub(center.b, center.a);
    const float edgeLen = TopdownLength(edge);
    if (edgeLen <= 0.000001f) {
        return false;
    }

    edge = TopdownMul(edge, 1.0f / edgeLen);
    const Vector2 side = TopdownMul(TopdownPerpRight(edge), radius);

    bool foundHit = false;
    float bestDistance = maxDistance;
    Vector2 bestPoint{};
    Vector2 bestNormal{};

    auto testSegment = [&](const TopdownSegment& seg)
    {
        std::vector<TopdownSegment> single{ seg };

        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = bestDistance;

        if (!RaycastClosestSegmentWithNormal(
                origin,
                dirNormalized,
                single,
                bestDistance,
                hitPoint,
                hitNormal,
                hitDistance)) {
            return;
        }

        if (hitDistance < bestDistance) {
            foundHit = true;
            bestDistance = hitDistance;
            bestPoint = hitPoint;
            bestNormal = hitNormal;
        }
    };

    auto testCircle = [&](Vector2 centerPoint)
    {
        float hitDistance = 0.0f;
        Vector2 hitPoint{};
        Vector2 hitNormal{};

        if (!RaycastCircleDetailed(
                origin,
                dirNormalized,
                centerPoint,
                radius,
                bestDistance,
                hitDistance,
                hitPoint,
                hitNormal)) {
            return;
        }

        if (hitDistance < bestDistance) {
            foundHit = true;
            bestDistance = hitDistance;
            bestPoint = hitPoint;
            bestNormal = hitNormal;
        }
    };

    testSegment(TopdownSegment{
            TopdownAdd(center.a, side),
            TopdownAdd(center.b, side)
    });

    testSegment(TopdownSegment{
            TopdownSub(center.a, side),
            TopdownSub(center.b, side)
    });

    testCircle(center.a);
    testCircle(center.b);

    if (!foundHit) {
        return false;
    }

    outHitPoint = bestPoint;
    outHitNormal = bestNormal;
    outHitDistance = bestDistance;
    return true;
}

bool RaycastClosestDoor(
        GameState& state,
        Vector2 origin,
        Vector2 dirNormalized,
        float maxDistance,
        TopdownRuntimeDoor*& outDoor,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance)
{
    outDoor = nullptr;
    outHitDistance = maxDistance;

    bool foundHit = false;

    for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
        if (!door.visible) {
            continue;
        }

        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = outHitDistance;

        if (!RaycastDoorCapsuleDetailed(
                door,
                origin,
                dirNormalized,
                outHitDistance,
                hitPoint,
                hitNormal,
                hitDistance)) {
            continue;
        }

        if (hitDistance < outHitDistance) {
            foundHit = true;
            outDoor = &door;
            outHitPoint = hitPoint;
            outHitNormal = hitNormal;
            outHitDistance = hitDistance;
        }
    }

    return foundHit;
}

void ApplyDoorBallisticImpulse(
        TopdownRuntimeDoor& door,
        Vector2 hitPoint,
        Vector2 shotDir,
        float impulseMagnitude)
{
    if (impulseMagnitude <= 0.0f) {
        return;
    }

    shotDir = TopdownNormalizeOrZero(shotDir);
    if (TopdownLengthSqr(shotDir) <= 0.000001f) {
        return;
    }

    float leverT = 0.0f;
    (void)ComputeDoorClosestPoint(door, hitPoint, &leverT);

    // Same core angular response model as actor pushing,
    // but with a synthetic "impact velocity" along the shot direction.
    ApplyAngularImpulseFromActor(
            door,
            TopdownMul(shotDir, impulseMagnitude),
            TopdownMul(shotDir, -1.0f),
            leverT,
            0.0f);
}

static void UpdateSingleDoorSounds(GameState& state, TopdownRuntimeDoor& door)
{
    const float relativeAngle = ComputeDoorRelativeAngle(door);
    const bool nearClosed = std::fabs(relativeAngle) <= kDoorNearClosedRadians;

    if (!nearClosed &&
        door.wasNearClosed &&
        !door.openSoundPlayedThisSwing &&
        std::fabs(door.angularVelocity) >= kDoorOpenSoundMinAngularSpeed) {
        if (!door.openSoundId.empty()) {
            AudioPlaySoundAtPosition(
                    state,
                    door.openSoundId,
                    door.hinge,
                    AUDIO_RADIUS_DOOR,
                    RandomRangeFloat(0.97f, 1.03f));
        }
        door.openSoundPlayedThisSwing = true;
    }

    if (nearClosed &&
        !door.wasNearClosed &&
        std::fabs(door.angularVelocity) >= kDoorCloseSoundMinAngularSpeed) {
        if (!door.closeSoundId.empty()) {
            AudioPlaySoundAtPosition(
                    state,
                    door.closeSoundId,
                    door.hinge,
                    AUDIO_RADIUS_DOOR,
                    RandomRangeFloat(0.97f, 1.03f));
        }
        door.openSoundPlayedThisSwing = false;
    }

    door.wasNearClosed = nearClosed;
}

void TopdownUpdateDoors(GameState& state, float dt)
{
    for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
        if (!door.visible) {
            continue;
        }

        if (door.locked) {
            door.angleRadians = door.closedAngleRadians;
            door.angularVelocity = 0.0f;
            door.wasNearClosed = true;
            door.openSoundPlayedThisSwing = false;
            continue;
        }

        if (door.autoClose) {
            const float angleError =
                    NormalizeAngleRadians(door.closedAngleRadians - door.angleRadians);

            door.angularVelocity += angleError * door.autoCloseStrength * dt;
        }

        door.angularVelocity *= std::max(0.0f, 1.0f - door.damping * dt);
        door.angleRadians += door.angularVelocity * dt;

        const float minAngle = ComputeDoorWorldMinAngle(door);
        const float maxAngle = ComputeDoorWorldMaxAngle(door);

        if (door.angleRadians < minAngle) {
            door.angleRadians = minAngle;
            if (door.angularVelocity < 0.0f) {
                door.angularVelocity = 0.0f;
            }
        } else if (door.angleRadians > maxAngle) {
            door.angleRadians = maxAngle;
            if (door.angularVelocity > 0.0f) {
                door.angularVelocity = 0.0f;
            }
        }

        if (std::fabs(door.angularVelocity) < 0.0001f) {
            door.angularVelocity = 0.0f;
        }

        UpdateSingleDoorSounds(state, door);
    }
}

void ResolvePlayerVsDoors(GameState& state)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;

    for (int iteration = 0; iteration < kCollisionIterations; ++iteration) {
        for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
            if (!door.visible || door.locked) {
                continue;
            }

            float leverT = 0.0f;
            Vector2 normal{};
            float penetration = 0.0f;

            if (ResolveCircleVsDoorCapsule(
                    player.position,
                    player.velocity,
                    player.radius,
                    door,
                    &leverT,
                    &normal,
                    &penetration)) {
                ApplyAngularImpulseFromActor(
                        door,
                        player.velocity,
                        normal,
                        leverT,
                        penetration);
            }
        }
    }
}

void ResolveNpcVsDoors(GameState& state, TopdownNpcRuntime& npc)
{
    for (int iteration = 0; iteration < kCollisionIterations; ++iteration) {
        for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
            if (!door.visible || door.locked) {
                continue;
            }

            float leverT = 0.0f;
            Vector2 normal{};
            float penetration = 0.0f;

            if (ResolveCircleVsDoorCapsule(
                    npc.position,
                    npc.currentVelocity,
                    npc.collisionRadius,
                    door,
                    &leverT,
                    &normal,
                    &penetration)) {
                ApplyAngularImpulseFromActor(
                        door,
                        npc.currentVelocity,
                        normal,
                        leverT,
                        penetration);
            }
        }
    }
}

void ResolveNpcKnockbackVsDoors(GameState& state, TopdownNpcRuntime& npc)
{
    for (int iteration = 0; iteration < kCollisionIterations; ++iteration) {
        for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
            if (!door.visible || door.locked) {
                continue;
            }

            float leverT = 0.0f;
            Vector2 normal{};
            float penetration = 0.0f;

            if (ResolveCircleVsDoorCapsule(
                    npc.position,
                    npc.knockbackVelocity,
                    npc.collisionRadius,
                    door,
                    &leverT,
                    &normal,
                    &penetration)) {
                ApplyAngularImpulseFromActor(
                        door,
                        npc.knockbackVelocity,
                        normal,
                        leverT,
                        penetration);
            }
        }
    }
}

static Vector2 ComputeDoorPushNormal(
        const TopdownRuntimeDoor& door,
        Vector2 actorPosition,
        Vector2 closestPoint)
{
    Vector2 delta = TopdownSub(actorPosition, closestPoint);
    const float distSqr = TopdownLengthSqr(delta);

    if (distSqr > 0.000001f) {
        return TopdownNormalizeOrZero(delta);
    }

    const TopdownSegment seg = TopdownBuildDoorCenterSegment(door);
    Vector2 ab = TopdownSub(seg.b, seg.a);
    ab = TopdownNormalizeOrZero(ab);

    Vector2 n = TopdownPerpRight(ab);
    if (TopdownLengthSqr(n) <= 0.000001f) {
        return Vector2{1.0f, 0.0f};
    }

    return n;
}

void ApplyDoorMotionPushToPlayer(GameState& state, float dt)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;

    for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
        if (!door.visible || door.locked) {
            continue;
        }

        if (std::fabs(door.angularVelocity) <= 0.0001f) {
            continue;
        }

        float leverT = 0.0f;
        const Vector2 closest = ComputeDoorClosestPoint(door, player.position, &leverT);
        const Vector2 delta = TopdownSub(player.position, closest);

        const float minDist = player.radius + door.thickness * 0.5f + 2.0f;
        const float distSqr = TopdownLengthSqr(delta);

        if (distSqr > minDist * minDist) {
            continue;
        }

        const Vector2 normal = ComputeDoorPushNormal(door, player.position, closest);
        const Vector2 pointVelocity = ComputeDoorPointVelocity(door, closest);

        const float outwardSpeed = TopdownDot(pointVelocity, normal);
        if (outwardSpeed <= 0.0f) {
            continue;
        }

        static constexpr float kMaxDoorPushPerFrame = 24.0f;

        const float pushDistance = std::min(
                outwardSpeed * kDoorMotionPushPlayerScale * dt,
                kMaxDoorPushPerFrame);

        player.position = TopdownAdd(player.position, TopdownMul(normal, pushDistance));
    }
}

void ApplyDoorMotionPushToNpcs(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible || npc.corpse) {
            continue;
        }

        for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
            if (!door.visible || door.locked) {
                continue;
            }

            if (std::fabs(door.angularVelocity) <= 0.0001f) {
                continue;
            }

            float leverT = 0.0f;
            const Vector2 closest = ComputeDoorClosestPoint(door, npc.position, &leverT);
            const Vector2 delta = TopdownSub(npc.position, closest);

            const float minDist = npc.collisionRadius + door.thickness * 0.5f + 2.0f;
            const float distSqr = TopdownLengthSqr(delta);

            if (distSqr > minDist * minDist) {
                continue;
            }

            const Vector2 normal = ComputeDoorPushNormal(door, npc.position, closest);
            const Vector2 pointVelocity = ComputeDoorPointVelocity(door, closest);

            const float outwardSpeed = TopdownDot(pointVelocity, normal);
            if (outwardSpeed <= 0.0f) {
                continue;
            }

            static constexpr float kMaxDoorPushPerFrame = 24.0f;

            const float pushDistance = std::min(
                    outwardSpeed * kDoorMotionPushNpcScale * dt,
                    kMaxDoorPushPerFrame);

            const Vector2 push = TopdownMul(normal, pushDistance);
            npc.position = TopdownAdd(npc.position, push);

            if (outwardSpeed > 1.0f) {
                const float knockDistance =
                        std::min(outwardSpeed * kDoorNpcKnockbackScale * dt, 12.0f);

                if (knockDistance > 0.0f) {
                    StartNpcKnockback(npc, normal, knockDistance);
                }
            }
        }
    }
}
