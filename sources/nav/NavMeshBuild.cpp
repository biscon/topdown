#include "NavMeshBuild.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>

#include "utils/earcut.h"
#include "clipper2/clipper.h"
#include "raylib.h"

using namespace Clipper2Lib;

namespace
{
    constexpr double CLIPPER_SCALE = 1000.0;
}

struct EdgeOwner {
    int triangleIndex = -1;
    int edgeSlot = -1;
};

struct EdgeKey {
    int a = -1;
    int b = -1;

    bool operator==(const EdgeKey& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const
    {
        const uint64_t ua = static_cast<uint32_t>(k.a);
        const uint64_t ub = static_cast<uint32_t>(k.b);
        return static_cast<size_t>((ua << 32u) ^ ub);
    }
};

static Rectangle ComputeTriangleBounds(const Vector2& a, const Vector2& b, const Vector2& c)
{
    const float minX = std::min(a.x, std::min(b.x, c.x));
    const float minY = std::min(a.y, std::min(b.y, c.y));
    const float maxX = std::max(a.x, std::max(b.x, c.x));
    const float maxY = std::max(a.y, std::max(b.y, c.y));

    Rectangle r{};
    r.x = minX;
    r.y = minY;
    r.width = maxX - minX;
    r.height = maxY - minY;
    return r;
}

static Vector2 ComputeTriangleCentroid(const Vector2& a, const Vector2& b, const Vector2& c)
{
    return Vector2{
            (a.x + b.x + c.x) / 3.0f,
            (a.y + b.y + c.y) / 3.0f
    };
}

static EdgeKey MakeEdgeKey(int a, int b)
{
    if (a < b) {
        return EdgeKey{a, b};
    }
    return EdgeKey{b, a};
}

static void RegisterTriangleEdge(
        std::unordered_map<EdgeKey, EdgeOwner, EdgeKeyHash>& edgeMap,
        std::vector<NavTriangle>& triangles,
        int triangleIndex,
        int edgeSlot,
        int v0,
        int v1)
{
    const EdgeKey key = MakeEdgeKey(v0, v1);

    auto it = edgeMap.find(key);
    if (it == edgeMap.end()) {
        edgeMap.emplace(key, EdgeOwner{triangleIndex, edgeSlot});
        return;
    }

    const EdgeOwner other = it->second;
    if (other.triangleIndex < 0 || other.edgeSlot < 0) {
        return;
    }

    NavTriangle& a = triangles[triangleIndex];
    NavTriangle& b = triangles[other.triangleIndex];

    switch (edgeSlot) {
        case 0: a.neighbor0 = other.triangleIndex; break;
        case 1: a.neighbor1 = other.triangleIndex; break;
        case 2: a.neighbor2 = other.triangleIndex; break;
        default: break;
    }

    switch (other.edgeSlot) {
        case 0: b.neighbor0 = triangleIndex; break;
        case 1: b.neighbor1 = triangleIndex; break;
        case 2: b.neighbor2 = triangleIndex; break;
        default: break;
    }
}

static Point64 ToPoint64(Vector2 p)
{
    return Point64(
            static_cast<int64_t>(std::llround(static_cast<double>(p.x) * CLIPPER_SCALE)),
            static_cast<int64_t>(std::llround(static_cast<double>(p.y) * CLIPPER_SCALE)));
}

static Vector2 ToVector2(const Point64& p)
{
    return Vector2{
            static_cast<float>(static_cast<double>(p.x) / CLIPPER_SCALE),
            static_cast<float>(static_cast<double>(p.y) / CLIPPER_SCALE)
    };
}

static Path64 ToPath64(const NavPolygon& poly)
{
    Path64 path;
    path.reserve(poly.vertices.size());

    for (const Vector2& v : poly.vertices) {
        path.push_back(ToPoint64(v));
    }

    return path;
}

static bool PointInPath64(const Path64& path, const Point64& p)
{
    bool inside = false;
    const int count = static_cast<int>(path.size());
    if (count < 3) {
        return false;
    }

    for (int i = 0, j = count - 1; i < count; j = i++) {
        const Point64& a = path[i];
        const Point64& b = path[j];

        const bool intersects =
                ((a.y > p.y) != (b.y > p.y)) &&
                (static_cast<double>(p.x) <
                 (static_cast<double>(b.x - a.x) * static_cast<double>(p.y - a.y)) /
                 ((b.y == a.y) ? 1.0 : static_cast<double>(b.y - a.y)) +
                 static_cast<double>(a.x));

        if (intersects) {
            inside = !inside;
        }
    }

    return inside;
}

struct TriangulationPolygon {
    Path64 outer;
    std::vector<Path64> holes;
};

static EndType GetClosedPolygonEndType()
{
    return EndType::Polygon;
}

static JoinType GetPolygonJoinType()
{
    return JoinType::Miter;
}

static Paths64 OffsetPaths64(
        const Paths64& input,
        double delta)
{
    if (input.empty()) {
        return {};
    }

    ClipperOffset co;
    co.AddPaths(input, GetPolygonJoinType(), GetClosedPolygonEndType());

    Paths64 output;
    co.Execute(delta, output);
    return output;
}

static Paths64 BuildAdjustedWalkPaths(
        const NavMeshData& navMesh,
        float agentRadius)
{
    Paths64 walkPaths;
    walkPaths.reserve(navMesh.sourcePolygons.size());

    for (const NavPolygon& poly : navMesh.sourcePolygons) {
        if (poly.vertices.size() >= 3) {
            walkPaths.push_back(ToPath64(poly));
        }
    }

    if (walkPaths.empty()) {
        return {};
    }

    if (agentRadius <= 0.0f) {
        return walkPaths;
    }

    const double delta = -static_cast<double>(agentRadius) * CLIPPER_SCALE;
    return OffsetPaths64(walkPaths, delta);
}

static Paths64 BuildAdjustedBlockerPaths(
        const NavMeshData& navMesh,
        float agentRadius)
{
    Paths64 blockerPaths;
    blockerPaths.reserve(navMesh.blockerPolygons.size());

    for (const NavPolygon& poly : navMesh.blockerPolygons) {
        if (poly.vertices.size() >= 3) {
            blockerPaths.push_back(ToPath64(poly));
        }
    }

    if (blockerPaths.empty()) {
        return {};
    }

    if (agentRadius <= 0.0f) {
        return blockerPaths;
    }

    const double delta = static_cast<double>(agentRadius) * CLIPPER_SCALE;
    return OffsetPaths64(blockerPaths, delta);
}

static std::vector<TriangulationPolygon> BuildFinalWalkablePolygons(
        const NavMeshData& navMesh,
        float agentRadius)
{
    const Paths64 adjustedWalkPaths = BuildAdjustedWalkPaths(navMesh, agentRadius);
    if (adjustedWalkPaths.empty()) {
        return {};
    }

    Paths64 mergedWalk = Union(adjustedWalkPaths, FillRule::NonZero);

    const Paths64 adjustedBlockerPaths = BuildAdjustedBlockerPaths(navMesh, agentRadius);

    Paths64 finalPaths = mergedWalk;
    if (!adjustedBlockerPaths.empty()) {
        const Paths64 mergedBlockers = Union(adjustedBlockerPaths, FillRule::NonZero);
        finalPaths = Difference(mergedWalk, mergedBlockers, FillRule::NonZero);
    }

    std::vector<Path64> outers;
    std::vector<Path64> holes;

    for (const Path64& path : finalPaths) {
        if (path.size() < 3) {
            continue;
        }

        if (IsPositive(path)) {
            outers.push_back(path);
        } else {
            holes.push_back(path);
        }
    }

    std::vector<TriangulationPolygon> result;
    result.reserve(outers.size());

    for (const Path64& outer : outers) {
        TriangulationPolygon poly;
        poly.outer = outer;
        result.push_back(poly);
    }

    // Assign each hole to the smallest containing outer.
    for (const Path64& hole : holes) {
        if (hole.empty()) {
            continue;
        }

        const Point64 sample = hole[0];
        int bestOuterIndex = -1;
        double bestArea = std::numeric_limits<double>::max();

        for (int i = 0; i < static_cast<int>(result.size()); ++i) {
            const Path64& outer = result[i].outer;
            if (!PointInPath64(outer, sample)) {
                continue;
            }

            const double area = std::fabs(Area(outer));
            if (area < bestArea) {
                bestArea = area;
                bestOuterIndex = i;
            }
        }

        if (bestOuterIndex >= 0) {
            result[bestOuterIndex].holes.push_back(hole);
        }
    }

    return result;
}

static void TriangulatePolygonWithHoles(
        const TriangulationPolygon& poly,
        NavMeshData& navMesh,
        std::unordered_map<EdgeKey, EdgeOwner, EdgeKeyHash>& edgeMap)
{
    if (poly.outer.size() < 3) {
        return;
    }

    std::vector<std::vector<std::array<double, 2>>> earcutInput;
    earcutInput.emplace_back();

    const int baseVertexIndex = static_cast<int>(navMesh.vertices.size());

    earcutInput[0].reserve(poly.outer.size());
    for (const Point64& p : poly.outer) {
        const Vector2 v = ToVector2(p);
        earcutInput[0].push_back(std::array<double, 2>{
                static_cast<double>(v.x),
                static_cast<double>(v.y)
        });
        navMesh.vertices.push_back(v);
    }

    for (const Path64& hole : poly.holes) {
        if (hole.size() < 3) {
            continue;
        }

        earcutInput.emplace_back();
        earcutInput.back().reserve(hole.size());

        for (const Point64& p : hole) {
            const Vector2 v = ToVector2(p);
            earcutInput.back().push_back(std::array<double, 2>{
                    static_cast<double>(v.x),
                    static_cast<double>(v.y)
            });
            navMesh.vertices.push_back(v);
        }
    }

    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(earcutInput);
    if (indices.size() % 3 != 0) {
        return;
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        const int ia = baseVertexIndex + static_cast<int>(indices[i + 0]);
        const int ib = baseVertexIndex + static_cast<int>(indices[i + 1]);
        const int ic = baseVertexIndex + static_cast<int>(indices[i + 2]);

        if (ia < 0 || ib < 0 || ic < 0 ||
            ia >= static_cast<int>(navMesh.vertices.size()) ||
            ib >= static_cast<int>(navMesh.vertices.size()) ||
            ic >= static_cast<int>(navMesh.vertices.size())) {
            continue;
        }

        const Vector2 a = navMesh.vertices[ia];
        const Vector2 b = navMesh.vertices[ib];
        const Vector2 c = navMesh.vertices[ic];

        const float area2 = std::fabs(
                (b.x - a.x) * (c.y - a.y) -
                (b.y - a.y) * (c.x - a.x));

        if (area2 <= 0.0001f) {
            continue;
        }

        NavTriangle tri;
        tri.vertexIndex0 = ia;
        tri.vertexIndex1 = ib;
        tri.vertexIndex2 = ic;
        tri.centroid = ComputeTriangleCentroid(a, b, c);
        tri.bounds = ComputeTriangleBounds(a, b, c);

        const int triangleIndex = static_cast<int>(navMesh.triangles.size());
        navMesh.triangles.push_back(tri);

        RegisterTriangleEdge(edgeMap, navMesh.triangles, triangleIndex, 0, ia, ib);
        RegisterTriangleEdge(edgeMap, navMesh.triangles, triangleIndex, 1, ib, ic);
        RegisterTriangleEdge(edgeMap, navMesh.triangles, triangleIndex, 2, ic, ia);
    }
}

bool BuildNavMesh(NavMeshData& navMesh, float agentRadius)
{
    navMesh.vertices.clear();
    navMesh.triangles.clear();
    navMesh.built = false;

    std::unordered_map<EdgeKey, EdgeOwner, EdgeKeyHash> edgeMap;

    const std::vector<TriangulationPolygon> finalPolys =
            BuildFinalWalkablePolygons(navMesh, std::max(0.0f, agentRadius));

    for (const TriangulationPolygon& poly : finalPolys) {
        TriangulatePolygonWithHoles(poly, navMesh, edgeMap);
    }

    navMesh.built = !navMesh.triangles.empty();
    return true;
}

bool BuildNavMesh(NavMeshData& navMesh)
{
    return BuildNavMesh(navMesh, 0.0f);
}
