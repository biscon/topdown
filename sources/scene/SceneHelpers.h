#pragma once

#include "raylib.h"
#include "SceneData.h"

bool PointInPolygon(Vector2 p, const ScenePolygon& poly);
float Clamp01(float v);
float ComputeDepthScale(const SceneData& scene, float feetY);