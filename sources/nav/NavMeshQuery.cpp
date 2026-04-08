#include "NavMeshQuery.h"

#include <algorithm>
#include <cmath>
#include <limits>

static bool NearlyEqual(float a, float b, float eps = 0.0001f)
{
    return std::fabs(a - b) <= eps;
}

static bool SamePoint(Vector2 a, Vector2 b, float eps = 0.01f)
{
    return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps);
}

static float TriArea2(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static Vector2 ToMathSpace(Vector2 p)
{
    return Vector2{ p.x, -p.y };
}

static float AreaMath(Vector2 a, Vector2 b, Vector2 c)
{
    a = ToMathSpace(a);
    b = ToMathSpace(b);
    c = ToMathSpace(c);
    return TriArea2(a, b, c);
}

static int FindThirdVertexIndex(const NavTriangle& tri, int edgeV0, int edgeV1)
{
    const int verts[3] = { tri.vertexIndex0, tri.vertexIndex1, tri.vertexIndex2 };
    for (int v : verts) {
        if (v != edgeV0 && v != edgeV1) {
            return v;
        }
    }
    return -1;
}

static bool PointInTriangle(Vector2 p, Vector2 a, Vector2 b, Vector2 c)
{
    const float d1 = TriArea2(p, a, b);
    const float d2 = TriArea2(p, b, c);
    const float d3 = TriArea2(p, c, a);

    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);

    return !(hasNeg && hasPos);
}

static float DistSqr(Vector2 a, Vector2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static Vector2 ClosestPointOnSegment(Vector2 p, Vector2 a, Vector2 b)
{
    const Vector2 ab{ b.x - a.x, b.y - a.y };
    const float abLenSqr = ab.x * ab.x + ab.y * ab.y;
    if (abLenSqr <= 0.000001f) {
        return a;
    }

    const Vector2 ap{ p.x - a.x, p.y - a.y };
    float t = (ap.x * ab.x + ap.y * ab.y) / abLenSqr;
    t = std::clamp(t, 0.0f, 1.0f);

    return Vector2{
            a.x + ab.x * t,
            a.y + ab.y * t
    };
}

static bool GetSharedEdge(
        const NavTriangle& a,
        int neighborIndex,
        int& outV0,
        int& outV1)
{
    if (a.neighbor0 == neighborIndex) {
        outV0 = a.vertexIndex0;
        outV1 = a.vertexIndex1;
        return true;
    }
    if (a.neighbor1 == neighborIndex) {
        outV0 = a.vertexIndex1;
        outV1 = a.vertexIndex2;
        return true;
    }
    if (a.neighbor2 == neighborIndex) {
        outV0 = a.vertexIndex2;
        outV1 = a.vertexIndex0;
        return true;
    }
    return false;
}

static bool GetSharedEdgeMidpoint(
        const NavMeshData& navMesh,
        const NavTriangle& tri,
        int neighborIndex,
        Vector2& outMidpoint)
{
    int edgeV0 = -1;
    int edgeV1 = -1;
    if (!GetSharedEdge(tri, neighborIndex, edgeV0, edgeV1)) {
        return false;
    }

    const Vector2 a = navMesh.vertices[edgeV0];
    const Vector2 b = navMesh.vertices[edgeV1];

    outMidpoint = Vector2{
            (a.x + b.x) * 0.5f,
            (a.y + b.y) * 0.5f
    };
    return true;
}

int FindTriangleContainingPoint(const NavMeshData& navMesh, Vector2 p)
{
    if (!navMesh.built) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(navMesh.triangles.size()); ++i) {
        const NavTriangle& tri = navMesh.triangles[i];

        if (p.x < tri.bounds.x || p.x > tri.bounds.x + tri.bounds.width ||
            p.y < tri.bounds.y || p.y > tri.bounds.y + tri.bounds.height) {
            continue;
        }

        const Vector2 a = navMesh.vertices[tri.vertexIndex0];
        const Vector2 b = navMesh.vertices[tri.vertexIndex1];
        const Vector2 c = navMesh.vertices[tri.vertexIndex2];

        if (PointInTriangle(p, a, b, c)) {
            return i;
        }
    }

    return -1;
}

Vector2 ProjectPointToNavMesh(
        const NavMeshData& navMesh,
        Vector2 p,
        int* outTriangleIndex)
{
    if (outTriangleIndex != nullptr) {
        *outTriangleIndex = -1;
    }

    const int containing = FindTriangleContainingPoint(navMesh, p);
    if (containing >= 0) {
        if (outTriangleIndex != nullptr) {
            *outTriangleIndex = containing;
        }
        return p;
    }

    Vector2 bestPoint{};
    float bestDistSqr = std::numeric_limits<float>::max();
    int bestTri = -1;

    for (int i = 0; i < static_cast<int>(navMesh.triangles.size()); ++i) {
        const NavTriangle& tri = navMesh.triangles[i];

        const Vector2 a = navMesh.vertices[tri.vertexIndex0];
        const Vector2 b = navMesh.vertices[tri.vertexIndex1];
        const Vector2 c = navMesh.vertices[tri.vertexIndex2];

        const Vector2 candidates[3] = {
                ClosestPointOnSegment(p, a, b),
                ClosestPointOnSegment(p, b, c),
                ClosestPointOnSegment(p, c, a)
        };

        for (const Vector2& q : candidates) {
            const float d2 = DistSqr(p, q);
            if (d2 < bestDistSqr) {
                bestDistSqr = d2;
                bestPoint = q;
                bestTri = i;
            }
        }
    }

    if (outTriangleIndex != nullptr) {
        *outTriangleIndex = bestTri;
    }
    return bestPoint;
}

static bool BuildTrianglePathAStar(
        const NavMeshData& navMesh,
        int startTri,
        int endTri,
        Vector2 startPos,
        Vector2 endPos,
        std::vector<int>& outTrianglePath)
{
    outTrianglePath.clear();

    if (startTri < 0 || endTri < 0) {
        return false;
    }

    if (startTri == endTri) {
        outTrianglePath.push_back(startTri);
        return true;
    }

    const int triCount = static_cast<int>(navMesh.triangles.size());

    std::vector<float> gScore(triCount, std::numeric_limits<float>::max());
    std::vector<float> fScore(triCount, std::numeric_limits<float>::max());
    std::vector<int> cameFrom(triCount, -1);
    std::vector<bool> openSet(triCount, false);
    std::vector<bool> closedSet(triCount, false);

    gScore[startTri] = 0.0f;
    fScore[startTri] = std::sqrt(DistSqr(startPos, endPos));
    openSet[startTri] = true;

    for (;;) {
        int current = -1;
        float bestF = std::numeric_limits<float>::max();

        for (int i = 0; i < triCount; ++i) {
            if (openSet[i] && fScore[i] < bestF) {
                bestF = fScore[i];
                current = i;
            }
        }

        if (current < 0) {
            return false;
        }

        if (current == endTri) {
            std::vector<int> reversed;
            for (int n = current; n >= 0; n = cameFrom[n]) {
                reversed.push_back(n);
            }

            outTrianglePath.assign(reversed.rbegin(), reversed.rend());
            return true;
        }

        openSet[current] = false;
        closedSet[current] = true;

        const NavTriangle& tri = navMesh.triangles[current];
        const int neighbors[3] = { tri.neighbor0, tri.neighbor1, tri.neighbor2 };

        Vector2 currentPos = startPos;
        if (current != startTri) {
            const int parent = cameFrom[current];
            if (parent >= 0) {
                Vector2 entryMid{};
                if (GetSharedEdgeMidpoint(navMesh, tri, parent, entryMid)) {
                    currentPos = entryMid;
                } else {
                    currentPos = tri.centroid;
                }
            } else {
                currentPos = tri.centroid;
            }
        }

        for (int nb : neighbors) {
            if (nb < 0 || closedSet[nb]) {
                continue;
            }

            Vector2 nextPos = endPos;
            if (nb != endTri) {
                if (!GetSharedEdgeMidpoint(navMesh, tri, nb, nextPos)) {
                    nextPos = navMesh.triangles[nb].centroid;
                }
            }

            const float stepCost = std::sqrt(DistSqr(currentPos, nextPos));
            const float tentativeG = gScore[current] + stepCost;

            if (!openSet[nb] || tentativeG < gScore[nb]) {
                cameFrom[nb] = current;
                gScore[nb] = tentativeG;
                fScore[nb] = tentativeG + std::sqrt(DistSqr(nextPos, endPos));
                openSet[nb] = true;
            }
        }
    }
}


struct Portal {
    Vector2 left{};
    Vector2 right{};
};

static void BuildPortals(
        const NavMeshData& navMesh,
        const std::vector<int>& trianglePath,
        Vector2 startPos,
        Vector2 endPos,
        std::vector<Portal>& outPortals)
{
    outPortals.clear();

    Portal startPortal{};
    startPortal.left = startPos;
    startPortal.right = startPos;
    outPortals.push_back(startPortal);

    for (int i = 0; i + 1 < static_cast<int>(trianglePath.size()); ++i) {
        const int triAIndex = trianglePath[i];
        const int triBIndex = trianglePath[i + 1];

        const NavTriangle& triA = navMesh.triangles[triAIndex];
        const NavTriangle& triB = navMesh.triangles[triBIndex];

        int edgeV0 = -1;
        int edgeV1 = -1;
        if (!GetSharedEdge(triA, triBIndex, edgeV0, edgeV1)) {
            continue;
        }

        const int aOtherIndex = FindThirdVertexIndex(triA, edgeV0, edgeV1);
        const int bOtherIndex = FindThirdVertexIndex(triB, edgeV0, edgeV1);

        if (aOtherIndex < 0 || bOtherIndex < 0) {
            continue;
        }

        const Vector2 p0 = navMesh.vertices[edgeV0];
        const Vector2 p1 = navMesh.vertices[edgeV1];
        const Vector2 aOther = navMesh.vertices[aOtherIndex];
        const Vector2 bOther = navMesh.vertices[bOtherIndex];

        const float sideA = AreaMath(p0, p1, aOther);
        const float sideB = AreaMath(p0, p1, bOther);

        Portal portal{};

        // Correct portal orientation should put triA's unique vertex on the right
        // of the portal, and triB's unique vertex on the left.

        if (sideA < 0.0f && sideB > 0.0f) {
            portal.left = p1;
            portal.right = p0;
        } else if (sideA > 0.0f && sideB < 0.0f) {
            portal.left = p0;
            portal.right = p1;
        } else {
            // Fallback for degenerate / nearly-collinear cases:
            const float centroidSide = AreaMath(triA.centroid, triB.centroid, p0);
            if (centroidSide >= 0.0f) {
                portal.left = p1;
                portal.right = p0;
            } else {
                portal.left = p0;
                portal.right = p1;
            }
        }

        outPortals.push_back(portal);
    }

    Portal endPortal{};
    endPortal.left = endPos;
    endPortal.right = endPos;
    outPortals.push_back(endPortal);
}

static void RunFunnel(
        const std::vector<Portal>& portals,
        std::vector<Vector2>& outPoints)
{
    outPoints.clear();
    if (portals.empty()) {
        return;
    }

    Vector2 portalApex = portals[0].left;
    Vector2 portalLeft = portals[0].left;
    Vector2 portalRight = portals[0].right;

    int apexIndex = 0;
    int leftIndex = 0;
    int rightIndex = 0;

    outPoints.push_back(portalApex);

    for (int i = 1; i < static_cast<int>(portals.size()); ++i) {
        const Vector2 left = portals[i].left;
        const Vector2 right = portals[i].right;

        // Tighten right side
        if (AreaMath(portalApex, portalRight, right) <= 0.0f) {
            if (SamePoint(portalApex, portalRight) ||
                AreaMath(portalApex, portalLeft, right) > 0.0f) {
                portalRight = right;
                rightIndex = i;
            } else {
                outPoints.push_back(portalLeft);

                portalApex = portalLeft;
                apexIndex = leftIndex;

                portalLeft = portalApex;
                portalRight = portalApex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;

                i = apexIndex;
                continue;
            }
        }

        // Tighten left side
        if (AreaMath(portalApex, portalLeft, left) >= 0.0f) {
            if (SamePoint(portalApex, portalLeft) ||
                AreaMath(portalApex, portalRight, left) < 0.0f) {
                portalLeft = left;
                leftIndex = i;
            } else {
                outPoints.push_back(portalRight);

                portalApex = portalRight;
                apexIndex = rightIndex;

                portalLeft = portalApex;
                portalRight = portalApex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;

                i = apexIndex;
                continue;
            }
        }
    }

    const Vector2 endPoint = portals.back().left;
    if (outPoints.empty() || !SamePoint(outPoints.back(), endPoint)) {
        outPoints.push_back(endPoint);
    }
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

    if (!navMesh.built || navMesh.triangles.empty()) {
        return false;
    }

    int startTri = FindTriangleContainingPoint(navMesh, startPos);
    if (startTri < 0) {
        startPos = ProjectPointToNavMesh(navMesh, startPos, &startTri);
    }

    int endTri = FindTriangleContainingPoint(navMesh, endPos);
    if (endTri < 0) {
        endPos = ProjectPointToNavMesh(navMesh, endPos, &endTri);
    }

    if (outResolvedEndPos != nullptr) {
        *outResolvedEndPos = endPos;
    }

    if (startTri < 0 || endTri < 0) {
        return false;
    }

    std::vector<int> trianglePath;
    if (!BuildTrianglePathAStar(navMesh, startTri, endTri, startPos, endPos, trianglePath)) {
        return false;
    }

    if (outTrianglePath != nullptr) {
        *outTrianglePath = trianglePath;
    }

    if (trianglePath.size() == 1) {
        outPathPoints.push_back(endPos);
        return true;
    }

    std::vector<Portal> portals;
    BuildPortals(navMesh, trianglePath, startPos, endPos, portals);

    RunFunnel(portals, outPathPoints);

    if (!outPathPoints.empty() && SamePoint(outPathPoints.front(), startPos)) {
        outPathPoints.erase(outPathPoints.begin());
    }

    if (outPathPoints.empty() || !SamePoint(outPathPoints.back(), endPos)) {
        outPathPoints.push_back(endPos);
    }

    return true;
}
