#pragma once

#include <vector>

#include "raylib.h"
#include "topdown/TopdownData.h"

struct TopdownNpcRingSlotBuildConfig {
    float candidatePadding = 8.0f;
    int maxRings = 3;
    float minRadiusStep = 12.0f;
    float raycastEpsilon = 0.001f;
    bool includeOriginCandidate = true;
};

void TopdownBuildNpcBlockingSegments(
        const TopdownRuntimeData& runtime,
        std::vector<TopdownSegment>& outSegments);

bool TopdownCandidateOverlapsNpcBlockingGeometry(
        const TopdownRuntimeData& runtime,
        Vector2 candidate,
        float npcRadius,
        const std::vector<TopdownSegment>& blockingSegments,
        float candidatePadding,
        int ignoreNpcHandle = -1);

void TopdownCollectValidNpcRingSlots(
        const TopdownRuntimeData& runtime,
        Vector2 origin,
        float npcRadius,
        const TopdownNpcRingSlotBuildConfig& config,
        std::vector<Vector2>& outSlots,
        int ignoreNpcHandle = -1,
        int maxSlotCount = -1);
