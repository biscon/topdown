#include "topdown/BloodStampGeneration.h"

#include <cmath>
#include <algorithm>

#include "raylib.h"

static float RandomRangeFloatLocal(float minValue, float maxValue)
{
    const float t = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
    return minValue + (maxValue - minValue) * t;
}

static Vector2 RotateVectorLocal(Vector2 v, float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    return Vector2{
            v.x * c - v.y * s,
            v.x * s + v.y * c
    };
}

static void ClearImageTransparent(Image& image)
{
    ImageClearBackground(&image, Color{0, 0, 0, 0});
}

static void PremultiplyAndUpload(Image& image, TopdownBloodStamp& outStamp)
{
    ImageAlphaPremultiply(&image);

    outStamp.texture = LoadTextureFromImage(image);
    outStamp.loaded = (outStamp.texture.id != 0);

    if (outStamp.loaded) {
        SetTextureFilter(outStamp.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(outStamp.texture, TEXTURE_WRAP_CLAMP);
    }
}

static void AddDetachedDroplets(
        Image& image,
        Vector2 center,
        int count,
        float minDistance,
        float maxDistance,
        float minRadius,
        float maxRadius)
{
    for (int i = 0; i < count; ++i) {
        const float angle = RandomRangeFloatLocal(0.0f, 2.0f * PI);
        const float dist = RandomRangeFloatLocal(minDistance, maxDistance);
        const float radius = RandomRangeFloatLocal(minRadius, maxRadius);

        const Vector2 p{
                center.x + std::cos(angle) * dist,
                center.y + std::sin(angle) * dist
        };

        ImageDrawCircleV(
                &image,
                p,
                static_cast<int>(std::round(radius)),
                WHITE);
    }
}

static Image GenerateBloodSplatImage(int size)
{
    Image image = GenImageColor(size, size, Color{0, 0, 0, 0});
    ClearImageTransparent(image);

    const Vector2 center{
            size * 0.5f + RandomRangeFloatLocal(-5.0f, 5.0f),
            size * 0.5f + RandomRangeFloatLocal(-5.0f, 5.0f)
    };

    const int blobCount = GetRandomValue(5, 9);
    const float baseRadius = RandomRangeFloatLocal(size * 0.10f, size * 0.18f);

    for (int i = 0; i < blobCount; ++i) {
        const float angle = RandomRangeFloatLocal(0.0f, 2.0f * PI);
        const float dist = RandomRangeFloatLocal(0.0f, size * 0.18f);
        const float radius = baseRadius * RandomRangeFloatLocal(0.60f, 1.30f);

        const Vector2 p{
                center.x + std::cos(angle) * dist,
                center.y + std::sin(angle) * dist
        };

        ImageDrawCircleV(
                &image,
                p,
                static_cast<int>(std::round(radius)),
                WHITE);
    }

    for (int i = 0; i < 2; ++i) {
        const float angle = RandomRangeFloatLocal(0.0f, 2.0f * PI);
        const float dist = RandomRangeFloatLocal(size * 0.14f, size * 0.26f);
        const float radius = baseRadius * RandomRangeFloatLocal(0.35f, 0.75f);

        const Vector2 p{
                center.x + std::cos(angle) * dist,
                center.y + std::sin(angle) * dist
        };

        ImageDrawCircleV(
                &image,
                p,
                static_cast<int>(std::round(radius)),
                WHITE);
    }

    AddDetachedDroplets(
            image,
            center,
            GetRandomValue(1, 4),
            size * 0.18f,
            size * 0.34f,
            size * 0.020f,
            size * 0.055f);

    ImageBlurGaussian(&image, 3);

    Image noise = GenImagePerlinNoise(
            size,
            size,
            GetRandomValue(0, 10000),
            GetRandomValue(0, 10000),
            RandomRangeFloatLocal(18.0f, 32.0f));

    Color* pixels = LoadImageColors(image);
    Color* noisePixels = LoadImageColors(noise);

    if (pixels != nullptr && noisePixels != nullptr) {
        const float maxDist = size * 0.42f;

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                Color& c = pixels[y * size + x];
                if (c.a == 0) {
                    continue;
                }

                const Color noiseC = noisePixels[y * size + x];

                const float dx = x - center.x;
                const float dy = y - center.y;
                const float d = std::sqrt(dx * dx + dy * dy);
                const float radial01 = std::clamp(d / maxDist, 0.0f, 1.0f);

                const float baseAlpha = static_cast<float>(c.a) / 255.0f;
                const float noise01 = static_cast<float>(noiseC.r) / 255.0f;

                const float densityNoise = 0.72f + noise01 * 0.45f;
                const float edgeFade =
                        1.0f - radial01 * RandomRangeFloatLocal(0.18f, 0.34f);
                const float centerBoost =
                        1.05f - radial01 * 0.10f;

                float alpha01 = baseAlpha;
                alpha01 *= densityNoise;
                alpha01 *= edgeFade;
                alpha01 *= centerBoost;

                if (alpha01 < 0.06f) {
                    c = Color{0, 0, 0, 0};
                    continue;
                }

                alpha01 = std::clamp(alpha01, 0.0f, 1.0f);

                c.r = 255;
                c.g = 255;
                c.b = 255;
                c.a = static_cast<unsigned char>(std::round(alpha01 * 255.0f));
            }
        }

        UnloadImage(image);
        image = GenImageColor(size, size, Color{0, 0, 0, 0});

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Color c = pixels[y * size + x];
                if (c.a == 0) {
                    continue;
                }

                ImageDrawPixel(&image, x, y, c);
            }
        }
    }

    if (pixels != nullptr) {
        UnloadImageColors(pixels);
    }
    if (noisePixels != nullptr) {
        UnloadImageColors(noisePixels);
    }

    UnloadImage(noise);

    ImageBlurGaussian(&image, 2);

    return image;
}

static Image GenerateBloodStreakImage(int size)
{
    Image image = GenImageColor(size, size, Color{0, 0, 0, 0});
    ClearImageTransparent(image);

    const Vector2 center{
            size * 0.5f + RandomRangeFloatLocal(-4.0f, 4.0f),
            size * 0.5f + RandomRangeFloatLocal(-4.0f, 4.0f)
    };

    const float rotation =
            RandomRangeFloatLocal(-28.0f * DEG2RAD, 28.0f * DEG2RAD);

    const float totalLength =
            RandomRangeFloatLocal(size * 0.28f, size * 0.48f);

    const float bodyHalfWidth =
            RandomRangeFloatLocal(size * 0.07f, size * 0.11f);

    const int steps = GetRandomValue(18, 26);

    {
        const Vector2 sourceOffset =
                RotateVectorLocal(Vector2{-totalLength * 0.24f, 0.0f}, rotation);

        const Vector2 sourceCenter{
                center.x + sourceOffset.x,
                center.y + sourceOffset.y
        };

        const int sourceBlobCount = GetRandomValue(3, 5);
        for (int i = 0; i < sourceBlobCount; ++i) {
            const float angle = RandomRangeFloatLocal(0.0f, 2.0f * PI);
            const float dist = RandomRangeFloatLocal(0.0f, bodyHalfWidth * 0.85f);
            const float radius = RandomRangeFloatLocal(
                    bodyHalfWidth * 0.75f,
                    bodyHalfWidth * 1.20f);

            const Vector2 p{
                    sourceCenter.x + std::cos(angle) * dist,
                    sourceCenter.y + std::sin(angle) * dist
            };

            ImageDrawCircleV(
                    &image,
                    p,
                    static_cast<int>(std::round(radius)),
                    WHITE);
        }
    }

    const float wobbleFreq = RandomRangeFloatLocal(1.1f, 1.8f);
    const float wobbleAmp = RandomRangeFloatLocal(size * 0.012f, size * 0.032f);

    for (int i = 0; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps - 1);

        const float localX = (t - 0.5f) * totalLength;

        const float localY =
                std::sin(t * PI * wobbleFreq) * wobbleAmp +
                RandomRangeFloatLocal(-1.2f, 1.2f);

        Vector2 localPos{ localX, localY };
        localPos = RotateVectorLocal(localPos, rotation);

        const Vector2 p{
                center.x + localPos.x,
                center.y + localPos.y
        };

        float widthTaper = 1.0f - t * RandomRangeFloatLocal(0.40f, 0.62f);
        widthTaper = std::max(0.28f, widthTaper);

        const float radius =
                std::max(1.0f,
                         bodyHalfWidth * widthTaper * RandomRangeFloatLocal(0.90f, 1.14f));

        ImageDrawCircleV(
                &image,
                p,
                static_cast<int>(std::round(radius)),
                WHITE);
    }

    {
        const int sideBlobCount = GetRandomValue(2, 4);

        for (int i = 0; i < sideBlobCount; ++i) {
            const float t = RandomRangeFloatLocal(0.18f, 0.82f);
            const float localX = (t - 0.5f) * totalLength;
            const float localY = RandomRangeFloatLocal(-bodyHalfWidth * 1.0f, bodyHalfWidth * 1.0f);

            Vector2 localPos{ localX, localY };
            localPos = RotateVectorLocal(localPos, rotation);

            const Vector2 p{
                    center.x + localPos.x,
                    center.y + localPos.y
            };

            const float radius = RandomRangeFloatLocal(
                    size * 0.020f,
                    size * 0.045f);

            ImageDrawCircleV(
                    &image,
                    p,
                    static_cast<int>(std::round(radius)),
                    WHITE);
        }
    }

    {
        const int dropletCount = GetRandomValue(1, 3);

        for (int i = 0; i < dropletCount; ++i) {
            const float t = RandomRangeFloatLocal(0.76f, 1.02f);
            const float localX = (t - 0.5f) * totalLength;
            const float localY = RandomRangeFloatLocal(-bodyHalfWidth * 0.8f, bodyHalfWidth * 0.8f);

            Vector2 localPos{ localX, localY };
            localPos = RotateVectorLocal(localPos, rotation);

            const Vector2 p{
                    center.x + localPos.x,
                    center.y + localPos.y
            };

            const float radius = RandomRangeFloatLocal(
                    size * 0.014f,
                    size * 0.032f);

            ImageDrawCircleV(
                    &image,
                    p,
                    static_cast<int>(std::round(radius)),
                    WHITE);
        }
    }

    ImageBlurGaussian(&image, 4);

    Image noise = GenImagePerlinNoise(
            size,
            size,
            GetRandomValue(0, 10000),
            GetRandomValue(0, 10000),
            RandomRangeFloatLocal(22.0f, 36.0f));

    Color* pixels = LoadImageColors(image);
    Color* noisePixels = LoadImageColors(noise);

    if (pixels != nullptr && noisePixels != nullptr) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                Color& c = pixels[y * size + x];
                if (c.a == 0) {
                    continue;
                }

                const float nx = static_cast<float>(x) / static_cast<float>(size - 1);
                const float tailFade =
                        1.0f - std::clamp((nx - 0.55f) / 0.45f, 0.0f, 1.0f) * 0.35f;

                const float baseAlpha = static_cast<float>(c.a) / 255.0f;
                const float noise01 = static_cast<float>(noisePixels[y * size + x].r) / 255.0f;

                const float densityNoise = 0.82f + noise01 * 0.24f;

                float alpha01 = baseAlpha * densityNoise * tailFade;

                if (alpha01 < 0.06f) {
                    c = Color{0, 0, 0, 0};
                    continue;
                }

                alpha01 = std::clamp(alpha01, 0.0f, 1.0f);

                c.r = 255;
                c.g = 255;
                c.b = 255;
                c.a = static_cast<unsigned char>(std::round(alpha01 * 255.0f));
            }
        }

        UnloadImage(image);
        image = GenImageColor(size, size, Color{0, 0, 0, 0});

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Color c = pixels[y * size + x];
                if (c.a == 0) {
                    continue;
                }

                ImageDrawPixel(&image, x, y, c);
            }
        }
    }

    if (pixels != nullptr) {
        UnloadImageColors(pixels);
    }
    if (noisePixels != nullptr) {
        UnloadImageColors(noisePixels);
    }

    UnloadImage(noise);

    ImageBlurGaussian(&image, 2);

    return image;
}

static Image GenerateBloodParticleImage(int size)
{
    Image image = GenImageColor(size, size, Color{0, 0, 0, 0});
    ClearImageTransparent(image);

    const Vector2 center{
            size * 0.5f + RandomRangeFloatLocal(-2.0f, 2.0f),
            size * 0.5f + RandomRangeFloatLocal(-2.0f, 2.0f)
    };

    const float rotation =
            RandomRangeFloatLocal(-40.0f * DEG2RAD, 40.0f * DEG2RAD);

    const float length =
            RandomRangeFloatLocal(size * 0.10f, size * 0.22f);

    const float halfWidth =
            RandomRangeFloatLocal(size * 0.05f, size * 0.09f);

    const int blobCount = GetRandomValue(2, 4);

    for (int i = 0; i < blobCount; ++i) {
        const float t =
                (blobCount == 1)
                ? 0.5f
                : static_cast<float>(i) / static_cast<float>(blobCount - 1);

        const float localX = (t - 0.5f) * length;
        const float localY = RandomRangeFloatLocal(-halfWidth * 0.6f, halfWidth * 0.6f);

        Vector2 localPos{ localX, localY };
        localPos = RotateVectorLocal(localPos, rotation);

        const Vector2 p{
                center.x + localPos.x,
                center.y + localPos.y
        };

        const float radius = RandomRangeFloatLocal(
                halfWidth * 0.65f,
                halfWidth * 1.15f);

        ImageDrawCircleV(
                &image,
                p,
                static_cast<int>(std::round(std::max(1.0f, radius))),
                WHITE);
    }

    if (GetRandomValue(0, 100) < 45) {
        const Vector2 tailOffset = RotateVectorLocal(
                Vector2{
                        RandomRangeFloatLocal(length * 0.25f, length * 0.60f),
                        RandomRangeFloatLocal(-halfWidth * 0.5f, halfWidth * 0.5f)
                },
                rotation);

        const Vector2 p{
                center.x + tailOffset.x,
                center.y + tailOffset.y
        };

        const float radius = RandomRangeFloatLocal(
                size * 0.018f,
                size * 0.040f);

        ImageDrawCircleV(
                &image,
                p,
                static_cast<int>(std::round(std::max(1.0f, radius))),
                WHITE);
    }

    ImageBlurGaussian(&image, 1);

    Image noise = GenImagePerlinNoise(
            size,
            size,
            GetRandomValue(0, 10000),
            GetRandomValue(0, 10000),
            RandomRangeFloatLocal(10.0f, 18.0f));

    Color* pixels = LoadImageColors(image);
    Color* noisePixels = LoadImageColors(noise);

    if (pixels != nullptr && noisePixels != nullptr) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                Color& c = pixels[y * size + x];
                if (c.a == 0) {
                    continue;
                }

                const float baseAlpha = static_cast<float>(c.a) / 255.0f;
                const float noise01 = static_cast<float>(noisePixels[y * size + x].r) / 255.0f;

                float alpha01 = baseAlpha * (0.86f + noise01 * 0.18f);

                if (alpha01 < 0.10f) {
                    c = Color{0, 0, 0, 0};
                    continue;
                }

                alpha01 = std::clamp(alpha01, 0.0f, 1.0f);

                c.r = 255;
                c.g = 255;
                c.b = 255;
                c.a = static_cast<unsigned char>(std::round(alpha01 * 255.0f));
            }
        }

        UnloadImage(image);
        image = GenImageColor(size, size, Color{0, 0, 0, 0});

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Color c = pixels[y * size + x];
                if (c.a == 0) {
                    continue;
                }

                ImageDrawPixel(&image, x, y, c);
            }
        }
    }

    if (pixels != nullptr) {
        UnloadImageColors(pixels);
    }
    if (noisePixels != nullptr) {
        UnloadImageColors(noisePixels);
    }

    UnloadImage(noise);

    ImageBlurGaussian(&image, 1);

    return image;
}

static bool GenerateSplats(TopdownBloodStampLibrary& library)
{
    static constexpr int kStampSize = 128;
    static constexpr int kSplatCount = 8;

    library.splats.clear();
    library.splats.reserve(kSplatCount);

    for (int i = 0; i < kSplatCount; ++i) {
        Image image = GenerateBloodSplatImage(kStampSize);

        TopdownBloodStamp stamp;
        stamp.isStreak = false;

        PremultiplyAndUpload(image, stamp);
        UnloadImage(image);

        if (!stamp.loaded) {
            return false;
        }

        library.splats.push_back(stamp);
    }

    return true;
}

static bool GenerateStreaks(TopdownBloodStampLibrary& library)
{
    static constexpr int kStampSize = 128;
    static constexpr int kStreakCount = 6;

    library.streaks.clear();
    library.streaks.reserve(kStreakCount);

    for (int i = 0; i < kStreakCount; ++i) {
        Image image = GenerateBloodStreakImage(kStampSize);

        TopdownBloodStamp stamp;
        stamp.isStreak = true;

        PremultiplyAndUpload(image, stamp);
        UnloadImage(image);

        if (!stamp.loaded) {
            return false;
        }

        library.streaks.push_back(stamp);
    }

    return true;
}

static bool GenerateParticles(TopdownBloodStampLibrary& library)
{
    static constexpr int kStampSize = 64;
    static constexpr int kParticleCount = 10;

    library.particles.clear();
    library.particles.reserve(kParticleCount);

    for (int i = 0; i < kParticleCount; ++i) {
        Image image = GenerateBloodParticleImage(kStampSize);

        TopdownBloodStamp stamp;
        stamp.isStreak = false;

        PremultiplyAndUpload(image, stamp);
        UnloadImage(image);

        if (!stamp.loaded) {
            return false;
        }

        library.particles.push_back(stamp);
    }

    return true;
}

bool EnsureTopdownBloodStampLibraryGenerated(TopdownBloodStampLibrary& library)
{
    if (library.generated) {
        return true;
    }

    if (!GenerateSplats(library)) {
        UnloadTopdownBloodStampLibrary(library);
        return false;
    }

    if (!GenerateStreaks(library)) {
        UnloadTopdownBloodStampLibrary(library);
        return false;
    }

    if (!GenerateParticles(library)) {
        UnloadTopdownBloodStampLibrary(library);
        return false;
    }

    library.generated = true;
    return true;
}

void UnloadTopdownBloodStampLibrary(TopdownBloodStampLibrary& library)
{
    for (TopdownBloodStamp& stamp : library.splats) {
        if (stamp.loaded && stamp.texture.id != 0) {
            UnloadTexture(stamp.texture);
        }
    }

    for (TopdownBloodStamp& stamp : library.streaks) {
        if (stamp.loaded && stamp.texture.id != 0) {
            UnloadTexture(stamp.texture);
        }
    }

    for (TopdownBloodStamp& stamp : library.particles) {
        if (stamp.loaded && stamp.texture.id != 0) {
            UnloadTexture(stamp.texture);
        }
    }

    library.splats.clear();
    library.streaks.clear();
    library.particles.clear();
    library.generated = false;
}
