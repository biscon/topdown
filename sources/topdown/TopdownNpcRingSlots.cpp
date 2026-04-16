#include "topdown/TopdownNpcRingSlots.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"

static void AppendDoorCollisionSegments(
        const TopdownRuntimeData& runtime,
        std::vector<TopdownSegment>& outSegments)
{
    for (const TopdownRuntimeDoor& door : runtime.doors) {
        if (!door.visible) {
            continue;
        }

        Vector2 a{};
        Vector2 b{};
        Vector2 c{};
        Vector2 d{};
        TopdownBuildDoorCorners(door, a, b, c, d);

        outSegments.push_back(TopdownSegment{a, b});
        outSegments.push_back(TopdownSegment{b, c});
        outSegments.push_back(TopdownSegment{c, d});
        outSegments.push_back(TopdownSegment{d, a});
    }
}

static void AppendWindowCollisionSegments(
        const TopdownRuntimeData& runtime,
        std::vector<TopdownSegment>& outSegments)
{
    for (const TopdownRuntimeWindow& window : runtime.windows) {
        if (!window.visible || window.broken) {
            continue;
        }

        for (const TopdownSegment& edge : window.edges) {
            outSegments.push_back(edge);
        }
    }
}

void TopdownBuildNpcBlockingSegments(
        const TopdownRuntimeData& runtime,
        std::vector<TopdownSegment>& outSegments)
{
    outSegments = runtime.collision.movementSegments;
    outSegments.reserve(
            outSegments.size() +
            runtime.doors.size() * 4 +
            runtime.windows.size() * 4);

    AppendDoorCollisionSegments(runtime, outSegments);
    AppendWindowCollisionSegments(runtime, outSegments);
}

bool TopdownCandidateOverlapsNpcBlockingGeometry(
        const TopdownRuntimeData& runtime,
        Vector2 candidate,
        float npcRadius,
        const std::vector<TopdownSegment>& blockingSegments,
        float candidatePadding,
        int ignoreNpcHandle)
{
    const float clearance = npcRadius + candidatePadding;
    const float clearanceSqr = clearance * clearance;

    for (const TopdownNpcRuntime& npc : runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (ignoreNpcHandle >= 0 && npc.handle == ignoreNpcHandle) {
            continue;
        }

        const float minDist = npc.collisionRadius + clearance;
        const float minDistSqr = minDist * minDist;
        const Vector2 delta = TopdownSub(candidate, npc.position);
        if (TopdownLengthSqr(delta) < minDistSqr) {
            return true;
        }
    }

    for (const TopdownSegment& segment : blockingSegments) {
        const Vector2 closest = TopdownClosestPointOnSegment(candidate, segment);
        const Vector2 delta = TopdownSub(candidate, closest);
        if (TopdownLengthSqr(delta) < clearanceSqr) {
            return true;
        }
    }

    return false;
}

void TopdownCollectValidNpcRingSlots(
        const TopdownRuntimeData& runtime,
        Vector2 origin,
        float npcRadius,
        const TopdownNpcRingSlotBuildConfig& config,
        std::vector<Vector2>& outSlots,
        int ignoreNpcHandle,
        int maxSlotCount)
{
    outSlots.clear();

    if (maxSlotCount == 0) {
        return;
    }

    std::vector<TopdownSegment> blockingSegments;
    TopdownBuildNpcBlockingSegments(runtime, blockingSegments);

    if (config.includeOriginCandidate) {
        if (!TopdownCandidateOverlapsNpcBlockingGeometry(
                    runtime,
                    origin,
                    npcRadius,
                    blockingSegments,
                    config.candidatePadding,
                    ignoreNpcHandle)) {
            outSlots.push_back(origin);
            if (maxSlotCount > 0 && static_cast<int>(outSlots.size()) >= maxSlotCount) {
                return;
            }
        }
    }

    const float radiusStep =
            std::max(config.minRadiusStep, npcRadius * 2.0f + config.candidatePadding);
    const float slotArcLength =
            std::max(config.minRadiusStep, npcRadius * 2.0f + config.candidatePadding);

    for (int ringIndex = 1; ringIndex <= config.maxRings; ++ringIndex) {
        const float ringRadius = radiusStep * static_cast<float>(ringIndex);
        const float circumference = 2.0f * PI * ringRadius;
        const int slotCount = std::max(6, static_cast<int>(std::ceil(circumference / slotArcLength)));

        for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex) {
            const float t = static_cast<float>(slotIndex) / static_cast<float>(slotCount);
            const float radians = t * 2.0f * PI;
            const Vector2 offset{
                    std::cos(radians) * ringRadius,
                    std::sin(radians) * ringRadius
            };
            const Vector2 candidate = TopdownAdd(origin, offset);

            if (TopdownCandidateOverlapsNpcBlockingGeometry(
                        runtime,
                        candidate,
                        npcRadius,
                        blockingSegments,
                        config.candidatePadding,
                        ignoreNpcHandle)) {
                continue;
            }

            const Vector2 toCandidate = TopdownSub(candidate, origin);
            const float rayDistance = TopdownLength(toCandidate);
            if (rayDistance > 0.000001f) {
                const Vector2 rayDir = TopdownMul(toCandidate, 1.0f / rayDistance);
                Vector2 hitPoint{};
                const bool blocked = TopdownRaycastSegments(
                        origin,
                        rayDir,
                        blockingSegments,
                        rayDistance - config.raycastEpsilon,
                        hitPoint,
                        nullptr);
                if (blocked) {
                    continue;
                }
            }

            outSlots.push_back(candidate);
            if (maxSlotCount > 0 && static_cast<int>(outSlots.size()) >= maxSlotCount) {
                return;
            }
        }
    }
}
