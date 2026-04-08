#pragma once

#include <string>

enum class MoveInterpolation {
    Linear,
    Accelerate,
    Decelerate,
    AccelerateDecelerate,
    Overshoot
};

bool ParseInterpolation(const std::string& name,MoveInterpolation& outInterpolation);
float ApplyInterpolation(MoveInterpolation interpolation, float t);