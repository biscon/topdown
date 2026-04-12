#include <algorithm>
#include "LevelEffects.h"
#include "TopdownHelpers.h"
#include "raymath.h"
#include "LevelCollision.h"
#include "BloodRenderTarget.h"

static constexpr int kMaxBloodDecals = 900;
static constexpr int kMaxBloodImpactParticles = 512;

void AppendPlayerTracerEffect(
        GameState& state,
        Vector2 start,
        Vector2 end,
        TopdownTracerStyle style)
{
    TopdownTracerEffect tracer;
    tracer.active = true;
    tracer.start = start;
    tracer.end = end;
    tracer.style = style;

    switch (style) {
        case TopdownTracerStyle::Handgun:
            tracer.lifetimeMs = 70.0f;
            tracer.thickness = 4.0f;
            break;

        case TopdownTracerStyle::Shotgun:
            tracer.lifetimeMs = 110.0f;
            tracer.thickness = 5.0f;
            break;

        case TopdownTracerStyle::Rifle:
            tracer.lifetimeMs = 80.0f;
            tracer.thickness = 5.0f;
            break;

        default:
            tracer.lifetimeMs = 50.0f;
            tracer.thickness = 3.0f;
            break;
    }

    const Vector2 playerPos = state.topdown.runtime.player.position;

    tracer.anchorStartToPlayer = true;
    tracer.localStartOffset = TopdownSub(start, playerPos);

    // IMPORTANT: end stays world-space
    tracer.end = end;

    state.topdown.runtime.render.tracers.push_back(tracer);
}

void SpawnWallImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 hitNormal,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    const int particleCount = weaponConfig.wallImpactParticleCount;

    if (particleCount <= 0) {
        return;
    }

    const float halfSpreadRadians =
            weaponConfig.wallImpactSpreadDegrees * 0.5f * DEG2RAD;

    for (int i = 0; i < particleCount; ++i) {
        TopdownWallImpactParticle particle;
        particle.active = true;
        particle.position = hitPoint;

        const float spreadRadians =
                RandomRangeFloat(-halfSpreadRadians, halfSpreadRadians);

        const float speed =
                RandomRangeFloat(
                        weaponConfig.wallImpactParticleSpeedMin,
                        weaponConfig.wallImpactParticleSpeedMax);

        const float size =
                RandomRangeFloat(
                        weaponConfig.wallImpactParticleSizeMin,
                        weaponConfig.wallImpactParticleSizeMax);

        const float lifetimeMs =
                RandomRangeFloat(
                        weaponConfig.wallImpactParticleLifetimeMsMin,
                        weaponConfig.wallImpactParticleLifetimeMsMax);

        Vector2 dir = RotateVector(hitNormal, spreadRadians);
        dir = TopdownNormalizeOrZero(dir);

        particle.velocity = TopdownMul(dir, speed);
        particle.size = size;
        particle.lifetimeMs = lifetimeMs;
        particle.ageMs = 0.0f;

        switch (GetRandomValue(0, 2)) {
            case 0: particle.color = Color{160, 160, 160, 255}; break;
            case 1: particle.color = Color{190, 190, 190, 255}; break;
            default: particle.color = Color{220, 220, 220, 255}; break;
        }

        state.topdown.runtime.render.wallImpactParticles.push_back(particle);
    }
}

void SpawnMuzzleFlashEffect(
        GameState& state,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    if (weaponConfig.muzzleFlashLifetimeMs <= 0.0f ||
        weaponConfig.muzzleFlashForwardLength <= 0.0f) {
        return;
    }

    TopdownMuzzleFlashEffect flash;
    flash.active = true;
    flash.direction = TopdownNormalizeOrZero(shotDir);

    if (TopdownLengthSqr(flash.direction) <= 0.000001f) {
        flash.direction = Vector2{1.0f, 0.0f};
    }

    flash.position = muzzleWorld;

    // --- Anchor to player ---
    const Vector2 playerPos = state.topdown.runtime.player.position;
    flash.localOffset = TopdownSub(muzzleWorld, playerPos);
    flash.anchoredToPlayer = true;

    flash.ageMs = 0.0f;
    flash.lifetimeMs = weaponConfig.muzzleFlashLifetimeMs;
    flash.forwardLength = weaponConfig.muzzleFlashForwardLength;
    flash.sideWidth = weaponConfig.muzzleFlashSideWidth;

    state.topdown.runtime.render.muzzleFlashes.push_back(flash);
}

void SpawnMuzzleSmokeParticles(
        GameState& state,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    const int particleCount = weaponConfig.muzzleSmokeParticleCount;
    if (particleCount <= 0) {
        return;
    }

    shotDir = TopdownNormalizeOrZero(shotDir);
    if (TopdownLengthSqr(shotDir) <= 0.000001f) {
        shotDir = Vector2{1.0f, 0.0f};
    }

    const Vector2 right{ -shotDir.y, shotDir.x };
    const float halfSpreadRadians =
            weaponConfig.muzzleSmokeSpreadDegrees * 0.5f * DEG2RAD;

    static constexpr float kMuzzleSmokeBackOffset = 15.0f;

    const Vector2 smokeOrigin =
            TopdownAdd(muzzleWorld, TopdownMul(shotDir, -kMuzzleSmokeBackOffset));

    for (int i = 0; i < particleCount; ++i) {
        TopdownMuzzleSmokeParticle particle;
        particle.active = true;

        // Keep the existing local puff shaping,
        // just relative to a smoke origin moved back toward the gun.
        const float forwardOffset = RandomRangeFloat(5.0f, 12.0f);
        const float sideOffset = RandomRangeFloat(-4.0f, 4.0f);

        particle.position = TopdownAdd(
                smokeOrigin,
                TopdownAdd(
                        TopdownMul(shotDir, forwardOffset),
                        TopdownMul(right, sideOffset)));

        const float angle = RandomRangeFloat(-halfSpreadRadians, halfSpreadRadians);
        Vector2 spreadDir = RotateVector(shotDir, angle);
        spreadDir = TopdownNormalizeOrZero(spreadDir);

        Vector2 finalDir = TopdownNormalizeOrZero(
                TopdownAdd(
                        TopdownMul(spreadDir, 1.0f - weaponConfig.muzzleSmokeForwardBias),
                        TopdownMul(shotDir, weaponConfig.muzzleSmokeForwardBias)));

        if (TopdownLengthSqr(finalDir) <= 0.000001f) {
            finalDir = shotDir;
        }

        const float speed = RandomRangeFloat(
                weaponConfig.muzzleSmokeSpeedMin,
                weaponConfig.muzzleSmokeSpeedMax);

        particle.velocity = TopdownMul(finalDir, speed);
        particle.lifetimeMs = RandomRangeFloat(
                weaponConfig.muzzleSmokeLifetimeMsMin,
                weaponConfig.muzzleSmokeLifetimeMsMax);

        particle.size = RandomRangeFloat(
                weaponConfig.muzzleSmokeSizeMin * 0.35f,
                weaponConfig.muzzleSmokeSizeMax * 0.50f);

        particle.ageMs = 0.0f;
        particle.alpha = 0.0f;

        switch (GetRandomValue(0, 2)) {
            case 0: particle.color = Color{220, 220, 220, 255}; break;
            case 1: particle.color = Color{232, 232, 232, 255}; break;
            default: particle.color = Color{245, 245, 245, 255}; break;
        }

        state.topdown.runtime.render.muzzleSmokeParticles.push_back(particle);
    }
}

static void UpdateTracerEffects(GameState& state, float dt)
{
    std::vector<TopdownTracerEffect>& tracers = state.topdown.runtime.render.tracers;
    const Vector2 playerPos = state.topdown.runtime.player.position;

    for (TopdownTracerEffect& tracer : tracers) {
        if (!tracer.active) {
            continue;
        }

        if (tracer.anchorStartToPlayer) {
            tracer.start = TopdownAdd(playerPos, tracer.localStartOffset);
        }

        tracer.ageMs += dt * 1000.0f;
        if (tracer.ageMs >= tracer.lifetimeMs) {
            tracer.active = false;
        }
    }

    tracers.erase(
            std::remove_if(
                    tracers.begin(),
                    tracers.end(),
                    [](const TopdownTracerEffect& tracer) {
                        return !tracer.active;
                    }),
            tracers.end());
}

static void UpdateWallImpactParticles(GameState& state, float dt)
{
    std::vector<TopdownWallImpactParticle>& particles =
            state.topdown.runtime.render.wallImpactParticles;

    for (TopdownWallImpactParticle& particle : particles) {
        if (!particle.active) {
            continue;
        }

        particle.ageMs += dt * 1000.0f;
        if (particle.ageMs >= particle.lifetimeMs) {
            particle.active = false;
            continue;
        }

        particle.position = TopdownAdd(particle.position, TopdownMul(particle.velocity, dt));

        const float alpha01 = 1.0f - Clamp(particle.ageMs / particle.lifetimeMs, 0.0f, 1.0f);
        particle.alpha = static_cast<unsigned char>(std::round(255.0f * alpha01));
    }

    particles.erase(
            std::remove_if(
                    particles.begin(),
                    particles.end(),
                    [](const TopdownWallImpactParticle& particle) {
                        return !particle.active;
                    }),
            particles.end());
}

static void UpdateMuzzleFlashEffects(GameState& state, float dt)
{
    std::vector<TopdownMuzzleFlashEffect>& flashes =
            state.topdown.runtime.render.muzzleFlashes;

    const Vector2 playerPos = state.topdown.runtime.player.position;

    for (TopdownMuzzleFlashEffect& flash : flashes) {
        if (!flash.active) {
            continue;
        }

        // --- FOLLOW PLAYER ---
        if (flash.anchoredToPlayer) {
            flash.position = TopdownAdd(playerPos, flash.localOffset);
        }

        flash.ageMs += dt * 1000.0f;
        if (flash.ageMs >= flash.lifetimeMs) {
            flash.active = false;
        }
    }

    flashes.erase(
            std::remove_if(
                    flashes.begin(),
                    flashes.end(),
                    [](const TopdownMuzzleFlashEffect& flash) {
                        return !flash.active;
                    }),
            flashes.end());
}

static void UpdateMuzzleSmokeParticles(GameState& state, float dt)
{
    std::vector<TopdownMuzzleSmokeParticle>& particles =
            state.topdown.runtime.render.muzzleSmokeParticles;

    for (TopdownMuzzleSmokeParticle& particle : particles) {
        if (!particle.active) {
            continue;
        }

        particle.ageMs += dt * 1000.0f;
        if (particle.ageMs >= particle.lifetimeMs) {
            particle.active = false;
            continue;
        }

        particle.position = TopdownAdd(
                particle.position,
                TopdownMul(particle.velocity, dt));

        particle.velocity = MoveTowardsVector(
                particle.velocity,
                Vector2{},
                18.0f * dt);

        const float life01 =
                Clamp(particle.ageMs / particle.lifetimeMs, 0.0f, 1.0f);

        // Grow slower.
        particle.size += 4.5f * dt;

        // Gentle fade in, then out.
        const float fadeIn = Clamp(life01 * 2.2f, 0.0f, 1.0f);
        const float fadeOut = 1.0f - life01;
        //particle.alpha = fadeIn * fadeOut;
        particle.alpha = (fadeIn * fadeOut) * 0.25f; // tweak this
    }

    particles.erase(
            std::remove_if(
                    particles.begin(),
                    particles.end(),
                    [](const TopdownMuzzleSmokeParticle& particle) {
                        return !particle.active;
                    }),
            particles.end());
}

static void UpdateBloodImpactParticles(GameState& state, float dt)
{
    std::vector<TopdownBloodImpactParticle>& particles =
            state.topdown.runtime.render.bloodImpactParticles;

    for (TopdownBloodImpactParticle& particle : particles) {
        if (!particle.active) {
            continue;
        }

        particle.ageMs += dt * 1000.0f;
        if (particle.ageMs >= particle.lifetimeMs) {
            particle.active = false;
            continue;
        }

        particle.position = TopdownAdd(
                particle.position,
                TopdownMul(particle.velocity, dt));

        particle.velocity = MoveTowardsVector(
                particle.velocity,
                Vector2{},
                180.0f * dt);

        const float life01 =
                Clamp(particle.ageMs / particle.lifetimeMs, 0.0f, 1.0f);

        particle.size += 2.0f * dt;

        particle.alpha = 1.0f - life01;
    }

    particles.erase(
            std::remove_if(
                    particles.begin(),
                    particles.end(),
                    [](const TopdownBloodImpactParticle& particle) {
                        return !particle.active;
                    }),
            particles.end());
}


static void EnforceBloodDecalCap(TopdownRenderWorld& renderWorld)
{
    std::vector<TopdownBloodDecal>& decals = renderWorld.bloodDecals;

    while (static_cast<int>(decals.size()) > kMaxBloodDecals) {
        decals.erase(decals.begin());
    }
}

static void BuildBloodDecalRaycastSegments(
        const GameState& state,
        std::vector<TopdownSegment>& outSegments)
{
    outSegments.clear();

    const std::vector<TopdownSegment>& vision =
            state.topdown.runtime.collision.visionSegments;
    const std::vector<TopdownSegment>& boundary =
            state.topdown.runtime.collision.boundarySegments;

    outSegments.reserve(vision.size() + boundary.size());

    for (const TopdownSegment& seg : vision) {
        outSegments.push_back(seg);
    }

    for (const TopdownSegment& seg : boundary) {
        outSegments.push_back(seg);
    }
}

static float BuildBiasedBloodDecalDistance(
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    float t = RandomRangeFloat(0.0f, 1.0f);
    t = t * t;

    return weaponConfig.bloodDecalDistanceMin +
           (weaponConfig.bloodDecalDistanceMax - weaponConfig.bloodDecalDistanceMin) * t;
}

static Vector2 BuildBloodSprayDirection(
        Vector2 baseDir,
        float halfSpreadRadians)
{
    const float spread =
            RandomRangeFloat(-halfSpreadRadians, halfSpreadRadians);

    Vector2 dir = RotateVector(baseDir, spread);
    return TopdownNormalizeOrZero(dir);
}

static int ChooseBloodStampIndex(int count)
{
    if (count <= 0) {
        return -1;
    }

    return GetRandomValue(0, count - 1);
}

static int ChooseBloodParticleStampIndex(const TopdownBloodStampLibrary& library)
{
    if (library.particles.empty()) {
        return -1;
    }

    return GetRandomValue(0, static_cast<int>(library.particles.size()) - 1);
}

static bool ShouldPreferStreakStamp(
        const TopdownPlayerWeaponConfig& weaponConfig,
        float dist01)
{
    float baseChance = 0.06f;
    float distanceBoost = dist01 * 0.22f;

    if (weaponConfig.equipmentSetId == "knife") {
        baseChance = 0.04f;
        distanceBoost = dist01 * 0.16f;
    } else if (weaponConfig.equipmentSetId == "handgun") {
        baseChance = 0.06f;
        distanceBoost = dist01 * 0.18f;
    } else if (weaponConfig.equipmentSetId == "rifle") {
        baseChance = 0.08f;
        distanceBoost = dist01 * 0.28f;
    } else if (weaponConfig.equipmentSetId == "shotgun") {
        baseChance = 0.07f;
        distanceBoost = dist01 * 0.22f;
    }

    const float streakChance = baseChance + distanceBoost;
    return RandomRangeFloat(0.0f, 1.0f) < streakChance;
}

static float ChooseBloodDecalStretch(bool preferStreakStamp)
{
    if (preferStreakStamp) {
        float base = RandomRangeFloat(1.45f, 2.35f);

        if (RandomRangeFloat(0.0f, 1.0f) < 0.35f) {
            base *= 0.75f;
        }

        return base;
    }

    return RandomRangeFloat(0.95f, 1.22f);
}

void SpawnBloodSpatterDecals(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    if (weaponConfig.bloodDecalCountMax <= 0) {
        return;
    }

    incomingShotDir = TopdownNormalizeOrZero(incomingShotDir);
    if (TopdownLengthSqr(incomingShotDir) <= 0.000001f) {
        incomingShotDir = Vector2{1.0f, 0.0f};
    }

    const Vector2 baseDir = TopdownMul(incomingShotDir, -1.0f);
    const Vector2 baseRight{ -baseDir.y, baseDir.x };

    const int decalCount =
            (weaponConfig.bloodDecalCountMin == weaponConfig.bloodDecalCountMax)
            ? weaponConfig.bloodDecalCountMin
            : GetRandomValue(
                    weaponConfig.bloodDecalCountMin,
                    weaponConfig.bloodDecalCountMax);

    if (decalCount <= 0) {
        return;
    }

    std::vector<TopdownSegment> segments;
    BuildBloodDecalRaycastSegments(state, segments);

    const float halfSpreadRadians =
            weaponConfig.bloodDecalSpreadDegrees * 0.5f * DEG2RAD;

    const TopdownBloodStampLibrary& library = state.topdown.bloodStampLibrary;

    // ------------------------------------------------------------
    // Build a few cluster centers near the spray direction.
    // Most decals will gather around these, which gives nicer
    // "bursts" instead of evenly distributed splats.
    // ------------------------------------------------------------
    std::vector<Vector2> clusterCenters;
    const int clusterCount = std::min(GetRandomValue(2, 4), std::max(1, decalCount));

    clusterCenters.reserve(clusterCount);

    for (int i = 0; i < clusterCount; ++i) {
        const float forward =
                RandomRangeFloat(
                        weaponConfig.bloodDecalDistanceMin * 0.25f,
                        weaponConfig.bloodDecalDistanceMax * 0.80f);

        const float side =
                RandomRangeFloat(
                        -weaponConfig.bloodDecalDistanceMax * 0.18f,
                        weaponConfig.bloodDecalDistanceMax * 0.18f);

        Vector2 center = TopdownAdd(
                hitPoint,
                TopdownAdd(
                        TopdownMul(baseDir, forward),
                        TopdownMul(baseRight, side)));

        clusterCenters.push_back(center);
    }

    for (int i = 0; i < decalCount; ++i) {
        Vector2 dir = BuildBloodSprayDirection(baseDir, halfSpreadRadians);
        if (TopdownLengthSqr(dir) <= 0.000001f) {
            continue;
        }

        const Vector2 right{ -dir.y, dir.x };

        // ------------------------------------------------------------
        // Two placement modes:
        // 1) clustered around one of the local burst centers
        // 2) free spray placement
        // ------------------------------------------------------------
        Vector2 targetPos{};

        const bool useCluster =
                !clusterCenters.empty() &&
                (RandomRangeFloat(0.0f, 1.0f) < 0.68f);

        if (useCluster) {
            const int clusterIndex = GetRandomValue(0, static_cast<int>(clusterCenters.size()) - 1);
            const Vector2 clusterCenter = clusterCenters[clusterIndex];

            const float forwardJitter = RandomRangeFloat(-16.0f, 16.0f);
            const float sideJitter = RandomRangeFloat(-14.0f, 14.0f);

            targetPos = TopdownAdd(
                    clusterCenter,
                    TopdownAdd(
                            TopdownMul(dir, forwardJitter),
                            TopdownMul(right, sideJitter)));
        } else {
            const float desiredDistance =
                    BuildBiasedBloodDecalDistance(weaponConfig);

            float finalDistance = desiredDistance;

            if (!segments.empty()) {
                Vector2 wallHitPoint{};
                float wallHitDistance = desiredDistance;

                if (TopdownRaycastSegments(
                        hitPoint,
                        dir,
                        segments,
                        desiredDistance,
                        wallHitPoint,
                        &wallHitDistance)) {
                    finalDistance = wallHitDistance - weaponConfig.bloodDecalWallPadding;
                }
            }

            if (finalDistance < 2.0f) {
                finalDistance = 2.0f;
            }

            targetPos = TopdownAdd(hitPoint, TopdownMul(dir, finalDistance));

            // Small local jitter so even the "free" decals do not look too uniform.
            targetPos = TopdownAdd(
                    targetPos,
                    TopdownAdd(
                            TopdownMul(dir, RandomRangeFloat(-6.0f, 8.0f)),
                            TopdownMul(right, RandomRangeFloat(-8.0f, 8.0f))));
        }

        // Final wall/boundary safety pass for the chosen target position.
        if (!segments.empty()) {
            Vector2 toTarget = TopdownSub(targetPos, hitPoint);
            const float distToTarget = TopdownLength(toTarget);

            if (distToTarget > 0.000001f) {
                const Vector2 rayDir = TopdownMul(toTarget, 1.0f / distToTarget);

                Vector2 wallHitPoint{};
                float wallHitDistance = distToTarget;

                if (TopdownRaycastSegments(
                        hitPoint,
                        rayDir,
                        segments,
                        distToTarget,
                        wallHitPoint,
                        &wallHitDistance)) {
                    const float safeDistance =
                            std::max(2.0f, wallHitDistance - weaponConfig.bloodDecalWallPadding);

                    targetPos = TopdownAdd(hitPoint, TopdownMul(rayDir, safeDistance));
                }
            }
        }

        const float travelDist = TopdownLength(TopdownSub(targetPos, hitPoint));
        const float dist01 = Clamp(
                (travelDist - weaponConfig.bloodDecalDistanceMin) /
                std::max(weaponConfig.bloodDecalDistanceMax - weaponConfig.bloodDecalDistanceMin, 0.001f),
                0.0f,
                1.0f);



        TopdownBloodDecal decal;
        decal.active = true;
        decal.kind = TopdownBloodDecalKind::Spatter;
        decal.position = targetPos;
        decal.radius = RandomRangeFloat(
                weaponConfig.bloodDecalRadiusMin,
                weaponConfig.bloodDecalRadiusMax);
        decal.targetRadius = decal.radius;
        decal.growthRate = 0.0f;
        decal.opacity = RandomRangeFloat(
                weaponConfig.bloodDecalOpacityMin,
                weaponConfig.bloodDecalOpacityMax);
        decal.ageMs = 0.0f;
        decal.variantSeed = static_cast<unsigned int>(GetRandomValue(0, 0x7fffffff));

        decal.useGeneratedStamp = library.generated;
        decal.preferStreakStamp = ShouldPreferStreakStamp(weaponConfig, dist01);
        decal.stretch = ChooseBloodDecalStretch(decal.preferStreakStamp);

        if (decal.useGeneratedStamp) {
            if (decal.preferStreakStamp && !library.streaks.empty()) {
                decal.stampIndex = ChooseBloodStampIndex(static_cast<int>(library.streaks.size()));
                decal.rotationRadians = std::atan2(dir.y, dir.x);
            } else if (!library.splats.empty()) {
                decal.preferStreakStamp = false;
                decal.stampIndex = ChooseBloodStampIndex(static_cast<int>(library.splats.size()));
                decal.rotationRadians = RandomRangeFloat(0.0f, 2.0f * PI);
            } else if (!library.streaks.empty()) {
                decal.preferStreakStamp = true;
                decal.stampIndex = ChooseBloodStampIndex(static_cast<int>(library.streaks.size()));
                decal.rotationRadians = std::atan2(dir.y, dir.x);
            } else {
                decal.useGeneratedStamp = false;
                decal.stampIndex = -1;
                decal.rotationRadians = RandomRangeFloat(0.0f, 2.0f * PI);
            }
        } else {
            decal.rotationRadians = RandomRangeFloat(0.0f, 2.0f * PI);
        }

        state.topdown.runtime.render.bloodDecals.push_back(decal);
    }

    EnforceBloodDecalCap(state.topdown.runtime.render);

    MarkTopdownBloodRenderTargetDirty(state);
}

void QueueBloodSpatterDecals(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    TopdownPendingBloodDecalSpawn pending;
    pending.active = true;
    pending.hitPoint = hitPoint;
    pending.incomingShotDir = incomingShotDir;
    pending.weaponConfig = weaponConfig;
    pending.elapsedMs = 0.0f;

    if (weaponConfig.equipmentSetId == "handgun") {
        pending.delayMs = RandomRangeFloat(90.0f, 140.0f);
    } else if (weaponConfig.equipmentSetId == "shotgun") {
        pending.delayMs = RandomRangeFloat(120.0f, 190.0f);
    } else {
        pending.delayMs = 0.0f;
    }

    state.topdown.runtime.render.pendingBloodDecalSpawns.push_back(pending);
}

void SpawnBloodPoolEmitter(
        GameState& state,
        Vector2 position,
        float maxRadius,
        float durationMs)
{
    TopdownBloodPoolEmitter emitter;
    emitter.active = true;
    emitter.position = position;
    emitter.elapsedMs = 0.0f;
    emitter.durationMs = std::max(1.0f, durationMs);
    emitter.spawnIntervalMs = RandomRangeFloat(30.0f, 70.0f);
    emitter.spawnTimerMs = 0.0f;
    emitter.maxRadius = std::max(4.0f, maxRadius);

    state.topdown.runtime.render.bloodPoolEmitters.push_back(emitter);
}

static void EnforceBloodImpactParticleCap(TopdownRenderWorld& renderWorld)
{
    std::vector<TopdownBloodImpactParticle>& particles =
            renderWorld.bloodImpactParticles;

    while (static_cast<int>(particles.size()) > kMaxBloodImpactParticles) {
        particles.erase(particles.begin());
    }
}

void SpawnBloodImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    const int particleCount = weaponConfig.bloodImpactParticleCount;
    if (particleCount <= 0) {
        return;
    }

    incomingShotDir = TopdownNormalizeOrZero(incomingShotDir);
    if (TopdownLengthSqr(incomingShotDir) <= 0.000001f) {
        incomingShotDir = Vector2{1.0f, 0.0f};
    }

    const Vector2 baseDir = TopdownMul(incomingShotDir, -1.0f);
    const Vector2 baseRight{ -baseDir.y, baseDir.x };

    const float halfSpreadRadians =
            weaponConfig.bloodImpactSpreadDegrees * 0.5f * DEG2RAD;

    const TopdownBloodStampLibrary& library = state.topdown.bloodStampLibrary;

    for (int i = 0; i < particleCount; ++i) {
        TopdownBloodImpactParticle particle;
        particle.active = true;

        Vector2 dir = BuildBloodSprayDirection(baseDir, halfSpreadRadians);
        if (TopdownLengthSqr(dir) <= 0.000001f) {
            continue;
        }

        const Vector2 right{ -dir.y, dir.x };

        const float forwardJitter = RandomRangeFloat(-4.0f, 10.0f);
        const float sideJitter = RandomRangeFloat(-8.0f, 8.0f);

        particle.position = TopdownAdd(
                hitPoint,
                TopdownAdd(
                        TopdownMul(dir, forwardJitter),
                        TopdownMul(right, sideJitter)));

        const float speed = RandomRangeFloat(
                weaponConfig.bloodImpactParticleSpeedMin,
                weaponConfig.bloodImpactParticleSpeedMax);

        particle.velocity = TopdownMul(dir, speed);

        particle.velocity = TopdownAdd(
                particle.velocity,
                TopdownMul(baseDir, RandomRangeFloat(8.0f, 26.0f)));

        particle.velocity = TopdownAdd(
                particle.velocity,
                TopdownMul(baseRight, RandomRangeFloat(-18.0f, 18.0f)));

        particle.ageMs = 0.0f;
        particle.lifetimeMs = RandomRangeFloat(
                weaponConfig.bloodImpactParticleLifetimeMsMin,
                weaponConfig.bloodImpactParticleLifetimeMsMax);

        particle.size = RandomRangeFloat(
                weaponConfig.bloodImpactParticleSizeMin,
                weaponConfig.bloodImpactParticleSizeMax);

        particle.alpha = 1.0f;

        switch (GetRandomValue(0, 2)) {
            case 0: particle.color = Color{220, 32, 32, 255}; break;
            case 1: particle.color = Color{185, 20, 20, 255}; break;
            default: particle.color = Color{245, 52, 52, 255}; break;
        }

        particle.useGeneratedStamp = library.generated && !library.particles.empty();
        particle.stampIndex = particle.useGeneratedStamp
                              ? ChooseBloodParticleStampIndex(library)
                              : -1;

        particle.rotationRadians =
                std::atan2(dir.y, dir.x) +
                RandomRangeFloat(-30.0f * DEG2RAD, 30.0f * DEG2RAD);

        particle.stretch = RandomRangeFloat(0.85f, 1.35f);

        state.topdown.runtime.render.bloodImpactParticles.push_back(particle);
    }

    EnforceBloodImpactParticleCap(state.topdown.runtime.render);
}

bool TopdownShakeScreen(GameState& state,
                        float durationMs,
                        float strengthPx,
                        float frequencyHz,
                        bool smooth)
{
    if (durationMs <= 0.0f) {
        return false;
    }

    if (strengthPx <= 0.0f) {
        return false;
    }

    if (frequencyHz <= 0.0f) {
        frequencyHz = 30.0f;
    }

    TopdownScreenShakeState& shake = state.topdown.runtime.screenShake;

    if (!shake.active) {
        shake.active = true;
        shake.durationMs = durationMs;
        shake.elapsedMs = 0.0f;
        shake.strengthX = strengthPx;
        shake.strengthY = strengthPx;
        shake.frequencyHz = frequencyHz;
        shake.sampleTimerMs = 0.0f;
        shake.smooth = smooth;
        shake.previousOffset = Vector2{0.0f, 0.0f};
        shake.sampledOffset = Vector2{0.0f, 0.0f};
        shake.currentOffset = Vector2{0.0f, 0.0f};
        return true;
    }

    shake.active = true;
    shake.durationMs = durationMs;
    shake.elapsedMs = 0.0f;
    shake.strengthX = std::max(shake.strengthX, strengthPx);
    shake.strengthY = std::max(shake.strengthY, strengthPx);
    shake.frequencyHz = std::max(shake.frequencyHz, frequencyHz);
    shake.sampleTimerMs = 0.0f;
    shake.smooth = smooth;

    return true;
}

static void UpdateScreenShake(GameState& state, float dt)
{
    TopdownScreenShakeState& shake = state.topdown.runtime.screenShake;
    if (!shake.active) {
        shake.previousOffset = Vector2{0.0f, 0.0f};
        shake.sampledOffset = Vector2{0.0f, 0.0f};
        shake.currentOffset = Vector2{0.0f, 0.0f};
        return;
    }

    shake.elapsedMs += dt * 1000.0f;
    if (shake.elapsedMs >= shake.durationMs) {
        shake.active = false;
        shake.elapsedMs = 0.0f;
        shake.sampleTimerMs = 0.0f;
        shake.previousOffset = Vector2{0.0f, 0.0f};
        shake.sampledOffset = Vector2{0.0f, 0.0f};
        shake.currentOffset = Vector2{0.0f, 0.0f};
        return;
    }

    const float remaining01 = 1.0f - (shake.elapsedMs / shake.durationMs);
    const float intervalMs = 1000.0f / std::max(shake.frequencyHz, 0.001f);

    shake.sampleTimerMs += dt * 1000.0f;
    if (shake.sampleTimerMs >= intervalMs) {
        shake.sampleTimerMs = 0.0f;

        shake.previousOffset = shake.sampledOffset;

        shake.sampledOffset.x = static_cast<float>(GetRandomValue(
                static_cast<int>(std::round(-shake.strengthX)),
                static_cast<int>(std::round( shake.strengthX))));

        shake.sampledOffset.y = static_cast<float>(GetRandomValue(
                static_cast<int>(std::round(-shake.strengthY)),
                static_cast<int>(std::round( shake.strengthY))));
    }

    Vector2 baseOffset = shake.sampledOffset;

    if (shake.smooth) {
        float sampleAlpha = shake.sampleTimerMs / intervalMs;
        sampleAlpha = Clamp(sampleAlpha, 0.0f, 1.0f);
        baseOffset = Vector2Lerp(shake.previousOffset, shake.sampledOffset, sampleAlpha);
    }

    shake.currentOffset.x = baseOffset.x * remaining01;
    shake.currentOffset.y = baseOffset.y * remaining01;
}

static void UpdatePendingBloodDecalSpawns(GameState& state, float dt)
{
    std::vector<TopdownPendingBloodDecalSpawn>& pending =
            state.topdown.runtime.render.pendingBloodDecalSpawns;

    for (TopdownPendingBloodDecalSpawn& spawn : pending) {
        if (!spawn.active) {
            continue;
        }

        spawn.elapsedMs += dt * 1000.0f;
        if (spawn.elapsedMs >= spawn.delayMs) {
            SpawnBloodSpatterDecals(
                    state,
                    spawn.hitPoint,
                    spawn.incomingShotDir,
                    spawn.weaponConfig);
            spawn.active = false;
        }
    }

    pending.erase(
            std::remove_if(
                    pending.begin(),
                    pending.end(),
                    [](const TopdownPendingBloodDecalSpawn& spawn) {
                        return !spawn.active;
                    }),
            pending.end());
}

static void UpdateBloodPoolEmitters(GameState& state, float dt)
{
    bool anyDecalStillFading = false;

    for (auto& decal : state.topdown.runtime.render.bloodDecals) {
        decal.ageMs += dt * 1000.0f;

        if (decal.fadeInMs > 0.0f && decal.ageMs < decal.fadeInMs) {
            anyDecalStillFading = true;
        }
    }

    std::vector<TopdownBloodPoolEmitter>& emitters =
            state.topdown.runtime.render.bloodPoolEmitters;

    const TopdownBloodStampLibrary& library = state.topdown.bloodStampLibrary;

    bool spawnedAnyDecals = false;

    for (TopdownBloodPoolEmitter& emitter : emitters) {
        if (!emitter.active) {
            continue;
        }

        emitter.elapsedMs += dt * 1000.0f;
        if (emitter.elapsedMs >= emitter.durationMs) {
            emitter.active = false;
            continue;
        }

        emitter.spawnTimerMs += dt * 1000.0f;

        while (emitter.spawnTimerMs >= emitter.spawnIntervalMs) {
            emitter.spawnTimerMs -= emitter.spawnIntervalMs;

            const float t = Clamp(emitter.elapsedMs / emitter.durationMs, 0.0f, 1.0f);
            const float currentRadius = emitter.maxRadius * t;

            float dist01 = RandomRangeFloat(0.0f, 1.0f);
            dist01 = dist01 * dist01;

            const float angle = RandomRangeFloat(0.0f, 2.0f * PI);
            const float dist = currentRadius * dist01;

            Vector2 offset{
                    std::cos(angle) * dist,
                    std::sin(angle) * dist
            };

            offset.y += currentRadius * 0.08f * t;

            TopdownBloodDecal decal;
            decal.active = true;
            decal.kind = TopdownBloodDecalKind::Pool;
            decal.position = TopdownAdd(emitter.position, offset);

            decal.radius = RandomRangeFloat(30.0f, 50.0f);
            decal.targetRadius = decal.radius;
            decal.growthRate = 0.0f;
            decal.opacity = RandomRangeFloat(0.72f, 0.95f);

            const float fadeBias = dist01;
            const float fadeMinMs = 900.0f;
            const float fadeMaxMs = 1500.0f;

            decal.fadeInMs =
                    Lerp(fadeMinMs, fadeMaxMs, fadeBias) +
                    RandomRangeFloat(-80.0f, 80.0f);

            decal.fadeInMs = std::max(80.0f, decal.fadeInMs);
            decal.ageMs = 0.0f;
            decal.variantSeed = static_cast<unsigned int>(GetRandomValue(0, 0x7fffffff));

            decal.useGeneratedStamp = library.generated && !library.splats.empty();
            decal.preferStreakStamp = false;
            decal.stretch = RandomRangeFloat(0.92f, 1.28f);
            decal.rotationRadians = RandomRangeFloat(0.0f, 2.0f * PI);

            if (decal.useGeneratedStamp) {
                decal.stampIndex = ChooseBloodStampIndex(
                        static_cast<int>(library.splats.size()));
            } else {
                decal.stampIndex = -1;
            }

            state.topdown.runtime.render.bloodDecals.push_back(decal);
            spawnedAnyDecals = true;
            anyDecalStillFading = true;
        }
    }

    emitters.erase(
            std::remove_if(
                    emitters.begin(),
                    emitters.end(),
                    [](const TopdownBloodPoolEmitter& emitter) {
                        return !emitter.active;
                    }),
            emitters.end());

    if (spawnedAnyDecals) {
        EnforceBloodDecalCap(state.topdown.runtime.render);
    }

    if (spawnedAnyDecals || anyDecalStillFading) {
        MarkTopdownBloodRenderTargetDirty(state);
    }
}

void TopdownUpdateLevelEffects(GameState& state, float dt) {
    UpdateTracerEffects(state, dt);
    UpdatePendingBloodDecalSpawns(state, dt);
    UpdateBloodPoolEmitters(state, dt);
    UpdateWallImpactParticles(state, dt);
    UpdateBloodImpactParticles(state, dt);
    UpdateMuzzleFlashEffects(state, dt);
    UpdateMuzzleSmokeParticles(state, dt);
    UpdateScreenShake(state, dt);
}
