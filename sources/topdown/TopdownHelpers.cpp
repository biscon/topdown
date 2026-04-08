#include "topdown/TopdownHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "raymath.h"



float TopdownDot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

float TopdownLengthSqr(Vector2 v)
{
    return v.x * v.x + v.y * v.y;
}

float TopdownLength(Vector2 v)
{
    return std::sqrt(TopdownLengthSqr(v));
}

Vector2 TopdownAdd(Vector2 a, Vector2 b)
{
    return Vector2{a.x + b.x, a.y + b.y};
}

Vector2 TopdownSub(Vector2 a, Vector2 b)
{
    return Vector2{a.x - b.x, a.y - b.y};
}

Vector2 TopdownMul(Vector2 v, float s)
{
    return Vector2{v.x * s, v.y * s};
}

Vector2 TopdownNormalizeOrZero(Vector2 v)
{
    const float len = TopdownLength(v);
    if (len <= 0.000001f) {
        return Vector2{0.0f, 0.0f};
    }

    return Vector2{v.x / len, v.y / len};
}

float TopdownSignedPolygonArea(const std::vector<Vector2>& points)
{
    if (points.size() < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        const size_t next = (i + 1) % points.size();
        area += points[i].x * points[next].y;
        area -= points[next].x * points[i].y;
    }

    return area * 0.5f;
}

bool TopdownIsClockwise(const std::vector<Vector2>& points)
{
    return TopdownSignedPolygonArea(points) < 0.0f;
}

void TopdownEnsureClockwise(std::vector<Vector2>& points)
{
    if (!TopdownIsClockwise(points)) {
        std::reverse(points.begin(), points.end());
    }
}

void TopdownEnsureCounterClockwise(std::vector<Vector2>& points)
{
    if (TopdownIsClockwise(points)) {
        std::reverse(points.begin(), points.end());
    }
}

Rectangle TopdownComputePolygonBounds(const std::vector<Vector2>& points)
{
    Rectangle bounds{};

    if (points.empty()) {
        return bounds;
    }

    float minX = points[0].x;
    float minY = points[0].y;
    float maxX = points[0].x;
    float maxY = points[0].y;

    for (const Vector2& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    bounds.x = minX;
    bounds.y = minY;
    bounds.width = maxX - minX;
    bounds.height = maxY - minY;
    return bounds;
}

std::vector<TopdownSegment> TopdownBuildSegmentsFromPolygon(const std::vector<Vector2>& points)
{
    std::vector<TopdownSegment> segments;

    if (points.size() < 2) {
        return segments;
    }

    segments.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i) {
        const size_t next = (i + 1) % points.size();
        segments.push_back({points[i], points[next]});
    }

    return segments;
}

std::vector<Vector2> TopdownBuildRectPolygon(float x, float y, float width, float height, float scale)
{
    return {
            {x * scale,             y * scale},
            {(x + width) * scale,   y * scale},
            {(x + width) * scale,  (y + height) * scale},
            {x * scale,            (y + height) * scale},
    };
}

Vector2 TopdownClosestPointOnSegment(Vector2 p, const TopdownSegment& seg)
{
    const Vector2 ab = TopdownSub(seg.b, seg.a);
    const Vector2 ap = TopdownSub(p, seg.a);
    const float denom = TopdownDot(ab, ab);

    if (denom <= 0.000001f) {
        return seg.a;
    }

    float t = TopdownDot(ap, ab) / denom;
    t = Clamp(t, 0.0f, 1.0f);

    return TopdownAdd(seg.a, TopdownMul(ab, t));
}

bool TopdownPointInPolygon(Vector2 p, const std::vector<Vector2>& polygon)
{
    if (polygon.size() < 3) {
        return false;
    }

    bool inside = false;

    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vector2& a = polygon[i];
        const Vector2& b = polygon[j];

        const bool intersects =
                ((a.y > p.y) != (b.y > p.y)) &&
                (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) + 0.000001f) + a.x);

        if (intersects) {
            inside = !inside;
        }
    }

    return inside;
}

Vector2 TopdownComputeEffectRegionOcclusionOrigin(
        const TopdownAuthoredEffectRegion& effect)
{
    if (effect.hasOcclusionOriginOverride) {
        return effect.occlusionOrigin;
    }

    if (effect.usePolygon && !effect.polygon.empty()) {
        float sumX = 0.0f;
        float sumY = 0.0f;

        for (const Vector2& p : effect.polygon) {
            sumX += p.x;
            sumY += p.y;
        }

        const float invCount = 1.0f / static_cast<float>(effect.polygon.size());
        return Vector2{ sumX * invCount, sumY * invCount };
    }

    return Vector2{
            effect.worldRect.x + effect.worldRect.width * 0.5f,
            effect.worldRect.y + effect.worldRect.height * 0.5f
    };
}

Vector2 TopdownWorldToScreen(const GameState& state, Vector2 worldPos)
{
    return Vector2{
            worldPos.x - state.topdown.runtime.camera.position.x,
            worldPos.y - state.topdown.runtime.camera.position.y
    };
}

bool TopdownRaycastSegments(
        Vector2 origin,
        Vector2 dirNormalized,
        const std::vector<TopdownSegment>& segments,
        float maxDistance,
        Vector2& outHitPoint,
        float* outHitDistance)
{
    const float epsilon = 0.000001f;

    bool foundHit = false;
    float bestT = maxDistance;
    Vector2 bestPoint{
            origin.x + dirNormalized.x * maxDistance,
            origin.y + dirNormalized.y * maxDistance
    };

    auto cross2D = [](Vector2 a, Vector2 b) -> float {
        return a.x * b.y - a.y * b.x;
    };

    for (const TopdownSegment& seg : segments) {
        const Vector2 v1{
                seg.a.x - origin.x,
                seg.a.y - origin.y
        };

        const Vector2 v2{
                seg.b.x - seg.a.x,
                seg.b.y - seg.a.y
        };

        const float denom = cross2D(dirNormalized, v2);
        if (std::fabs(denom) <= epsilon) {
            continue;
        }

        const float t = cross2D(v1, v2) / denom;
        const float u = cross2D(v1, dirNormalized) / denom;

        if (t < 0.0f || t > bestT) {
            continue;
        }

        if (u < 0.0f || u > 1.0f) {
            continue;
        }

        foundHit = true;
        bestT = t;
        bestPoint = Vector2{
                origin.x + dirNormalized.x * t,
                origin.y + dirNormalized.y * t
        };
    }

    outHitPoint = bestPoint;
    if (outHitDistance != nullptr) {
        *outHitDistance = bestT;
    }

    return foundHit;
}

Vector2 GetMouseWorldPosition(const GameState& state)
{
    const Vector2 mouseScreen = GetMousePosition();
    return Vector2{
            mouseScreen.x + state.topdown.runtime.camera.position.x,
            mouseScreen.y + state.topdown.runtime.camera.position.y
    };
}

float RandomRangeFloat(float minValue, float maxValue)
{
    const float t = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
    return minValue + (maxValue - minValue) * t;
}

Vector2 RotateVector(Vector2 v, float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    return Vector2{
            v.x * c - v.y * s,
            v.x * s + v.y * c
    };
}