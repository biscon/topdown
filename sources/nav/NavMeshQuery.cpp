#include "NavMeshQuery.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "detour/DetourNavMesh.h"
#include "detour/DetourNavMeshQuery.h"

namespace
{
    constexpr float kNearestPolyHalfExtents[3] = { 96.0f, 32.0f, 96.0f };
    constexpr int kMaxPathPolys = 4096;
    constexpr int kMaxStraightPathPoints = 4096;
    constexpr float kPointEqualEps = 0.01f;
}

static bool NearlyEqual(float a, float b, float eps = 0.0001f)
{
    return std::fabs(a - b) <= eps;
}

static bool SamePoint(Vector2 a, Vector2 b, float eps = kPointEqualEps)
{
    return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps);
}

static void ToDetourPos(Vector2 p, float outPos[3])
{
    outPos[0] = p.x;
    outPos[1] = 0.0f;
    outPos[2] = p.y;
}

static Vector2 FromDetourPos(const float p[3])
{
    return Vector2{ p[0], p[2] };
}

static int FindTriangleIndexForPolyRef(
        const NavMeshData& navMesh,
        dtPolyRef ref)
{
    if (ref == 0 || navMesh.detourNavMesh == nullptr) {
        return -1;
    }

    const dtMeshTile* tile = nullptr;
    const dtPoly* poly = nullptr;

    if (dtStatusFailed(navMesh.detourNavMesh->getTileAndPolyByRef(ref, &tile, &poly))) {
        return -1;
    }

    if (tile == nullptr || poly == nullptr || tile->polys == nullptr || tile->header == nullptr) {
        return -1;
    }

    const ptrdiff_t polyIndex = poly - tile->polys;
    if (polyIndex < 0 || polyIndex >= tile->header->polyCount) {
        return -1;
    }

    const int triIndex = static_cast<int>(polyIndex);
    if (triIndex < 0 || triIndex >= static_cast<int>(navMesh.triangles.size())) {
        return -1;
    }

    return triIndex;
}

static bool FindNearestPolyRef(
        const NavMeshData& navMesh,
        Vector2 p,
        dtPolyRef& outRef,
        float outNearest[3])
{
    outRef = 0;
    outNearest[0] = 0.0f;
    outNearest[1] = 0.0f;
    outNearest[2] = 0.0f;

    if (!navMesh.built ||
        navMesh.detourNavMesh == nullptr ||
        navMesh.detourQuery == nullptr) {
        return false;
    }

    float pos[3];
    ToDetourPos(p, pos);

    dtQueryFilter filter;
    if (dtStatusFailed(navMesh.detourQuery->findNearestPoly(
            pos,
            kNearestPolyHalfExtents,
            &filter,
            &outRef,
            outNearest))) {
        return false;
    }

    return outRef != 0;
}

int FindTriangleContainingPoint(const NavMeshData& navMesh, Vector2 p)
{
    dtPolyRef ref = 0;
    float nearest[3]{};

    if (!FindNearestPolyRef(navMesh, p, ref, nearest)) {
        return -1;
    }

    return FindTriangleIndexForPolyRef(navMesh, ref);
}

Vector2 ProjectPointToNavMesh(
        const NavMeshData& navMesh,
        Vector2 p,
        int* outTriangleIndex)
{
    if (outTriangleIndex != nullptr) {
        *outTriangleIndex = -1;
    }

    dtPolyRef ref = 0;
    float nearest[3]{};

    if (!FindNearestPolyRef(navMesh, p, ref, nearest)) {
        return p;
    }

    float closest[3]{};
    bool posOverPoly = false;

    if (dtStatusFailed(navMesh.detourQuery->closestPointOnPoly(
            ref,
            nearest,
            closest,
            &posOverPoly))) {
        if (outTriangleIndex != nullptr) {
            *outTriangleIndex = FindTriangleIndexForPolyRef(navMesh, ref);
        }
        return FromDetourPos(nearest);
    }

    if (outTriangleIndex != nullptr) {
        *outTriangleIndex = FindTriangleIndexForPolyRef(navMesh, ref);
    }

    (void)posOverPoly;
    return FromDetourPos(closest);
}

bool BuildNavPath(
        const NavMeshData& navMesh,
        Vector2 startPos,
        Vector2 endPos,
        std::vector<Vector2>& outPathPoints,
        std::vector<int>* outTrianglePath,
        Vector2* outResolvedEndPos)
{
    outPathPoints.clear();

    if (outTrianglePath != nullptr) {
        outTrianglePath->clear();
    }

    if (outResolvedEndPos != nullptr) {
        *outResolvedEndPos = endPos;
    }

    if (!navMesh.built ||
        navMesh.detourNavMesh == nullptr ||
        navMesh.detourQuery == nullptr) {
        return false;
    }

    dtPolyRef startRef = 0;
    dtPolyRef endRef = 0;

    float startNearest[3]{};
    float endNearest[3]{};

    if (!FindNearestPolyRef(navMesh, startPos, startRef, startNearest)) {
        return false;
    }

    if (!FindNearestPolyRef(navMesh, endPos, endRef, endNearest)) {
        return false;
    }

    float startClosest[3]{};
    float endClosest[3]{};
    bool startOverPoly = false;
    bool endOverPoly = false;

    if (dtStatusFailed(navMesh.detourQuery->closestPointOnPoly(
            startRef,
            startNearest,
            startClosest,
            &startOverPoly))) {
        return false;
    }

    if (dtStatusFailed(navMesh.detourQuery->closestPointOnPoly(
            endRef,
            endNearest,
            endClosest,
            &endOverPoly))) {
        return false;
    }

    const Vector2 resolvedEndPos = FromDetourPos(endClosest);
    if (outResolvedEndPos != nullptr) {
        *outResolvedEndPos = resolvedEndPos;
    }

    dtQueryFilter filter;

    std::vector<dtPolyRef> polyPath(static_cast<size_t>(kMaxPathPolys), 0);
    int polyPathCount = 0;

    if (dtStatusFailed(navMesh.detourQuery->findPath(
            startRef,
            endRef,
            startClosest,
            endClosest,
            &filter,
            polyPath.data(),
            &polyPathCount,
            static_cast<int>(polyPath.size())))) {
        return false;
    }

    if (polyPathCount <= 0) {
        return false;
    }

    if (outTrianglePath != nullptr) {
        outTrianglePath->reserve(static_cast<size_t>(polyPathCount));

        for (int i = 0; i < polyPathCount; ++i) {
            const int triIndex =
                    FindTriangleIndexForPolyRef(navMesh, polyPath[static_cast<size_t>(i)]);

            if (triIndex >= 0) {
                if (outTrianglePath->empty() || outTrianglePath->back() != triIndex) {
                    outTrianglePath->push_back(triIndex);
                }
            }
        }
    }

    std::vector<float> straightPath(static_cast<size_t>(kMaxStraightPathPoints) * 3u, 0.0f);
    std::vector<unsigned char> straightFlags(static_cast<size_t>(kMaxStraightPathPoints), 0);
    std::vector<dtPolyRef> straightRefs(static_cast<size_t>(kMaxStraightPathPoints), 0);
    int straightCount = 0;

    if (dtStatusFailed(navMesh.detourQuery->findStraightPath(
            startClosest,
            endClosest,
            polyPath.data(),
            polyPathCount,
            straightPath.data(),
            straightFlags.data(),
            straightRefs.data(),
            &straightCount,
            kMaxStraightPathPoints,
            0))) {
        return false;
    }

    if (straightCount <= 0) {
        outPathPoints.push_back(resolvedEndPos);
        return true;
    }

    outPathPoints.reserve(static_cast<size_t>(straightCount));

    for (int i = 0; i < straightCount; ++i) {
        Vector2 p{
                straightPath[static_cast<size_t>(i) * 3u + 0u],
                straightPath[static_cast<size_t>(i) * 3u + 2u]
        };

        if (i == 0 && SamePoint(p, startPos)) {
            continue;
        }

        if (!outPathPoints.empty() && SamePoint(outPathPoints.back(), p)) {
            continue;
        }

        outPathPoints.push_back(p);
    }

    if (outPathPoints.empty() || !SamePoint(outPathPoints.back(), resolvedEndPos)) {
        outPathPoints.push_back(resolvedEndPos);
    }

    (void)startOverPoly;
    (void)endOverPoly;
    return true;
}
