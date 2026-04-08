#include "Interpolation.h"
#include "raymath.h"

bool ParseInterpolation(const std::string& name,MoveInterpolation& outInterpolation)
{
    if (name == "linear") {
        outInterpolation = MoveInterpolation::Linear;
        return true;
    }
    if (name == "accelerate") {
        outInterpolation = MoveInterpolation::Accelerate;
        return true;
    }
    if (name == "decelerate") {
        outInterpolation = MoveInterpolation::Decelerate;
        return true;
    }
    if (name == "accelerateDecelerate") {
        outInterpolation = MoveInterpolation::AccelerateDecelerate;
        return true;
    }
    if (name == "overshoot") {
        outInterpolation = MoveInterpolation::Overshoot;
        return true;
    }
    return false;
}

float ApplyInterpolation(MoveInterpolation interpolation, float t)
{
    t = Clamp(t, 0.0f, 1.0f);

    switch (interpolation) {
        case MoveInterpolation::Accelerate:
            return t * t;

        case MoveInterpolation::Decelerate:
            return 1.0f - (1.0f - t) * (1.0f - t);

        case MoveInterpolation::AccelerateDecelerate:
            return t * t * (3.0f - 2.0f * t);

        case MoveInterpolation::Overshoot:
        {
            const float s = 1.70158f;
            const float x = t - 1.0f;
            return x * x * ((s + 1.0f) * x + s) + 1.0f;
        }

        case MoveInterpolation::Linear:
        default:
            return t;
    }
}
