#include <cmath>
#include "SceneHelpers.h"

bool PointInPolygon(Vector2 p, const ScenePolygon& poly)
{
    bool inside = false;
    const int count = static_cast<int>(poly.vertices.size());
    if (count < 3) {
        return false;
    }

    for (int i = 0, j = count - 1; i < count; j = i++) {
        const Vector2 a = poly.vertices[i];
        const Vector2 b = poly.vertices[j];

        const float denom = (b.y - a.y);
        const float safeDenom = (std::fabs(denom) < 0.000001f) ? 0.000001f : denom;

        const bool intersects =
                ((a.y > p.y) != (b.y > p.y)) &&
                (p.x < (b.x - a.x) * (p.y - a.y) / safeDenom + a.x);

        if (intersects) {
            inside = !inside;
        }
    }

    return inside;
}

float Clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float ComputeDepthScale(const SceneData& scene, float feetY)
{
    const float nearY = scene.scaleConfig.nearY;
    const float farY = scene.scaleConfig.farY;
    const float nearScale = scene.scaleConfig.nearScale;
    const float farScale = scene.scaleConfig.farScale;

    const float denom = farY - nearY;
    float t = 0.0f;

    if (std::fabs(denom) > 0.0001f) {
        t = (feetY - nearY) / denom;
    }

    t = Clamp01(t);
    const float scale = nearScale + (farScale - nearScale) * t;
    return std::min(scale, 1.0f);
}
