local Glow = {}

local function clamp01(x)
    if x < 0 then return 0 end
    if x > 1 then return 1 end
    return x
end

function Glow.runFire(regions, baseA, baseB)
    local targetA = baseA
    local targetB = baseB or baseA

    while true do
        if math.random(1, 100) <= 18 then
            targetA = math.random(75, 95) / 100
            targetB = math.random(20, 55) / 100
        end

        baseA = baseA + (targetA - baseA) * 0.18
        baseB = baseB + (targetB - baseB) * 0.18

        local flickerA = (math.random(-8, 8)) / 100
        local flickerB = (math.random(-12, 12)) / 800

        local a = clamp01(baseA + flickerA)
        local b = clamp01(baseB + flickerB)

        setEffectRegionOpacity(regions[1], a)
        if regions[2] then
            setEffectRegionOpacity(regions[2], b)
        end

        delay(math.random(40, 120))
    end
end

function Glow.runElectric(regions, baseA, baseB)
    local targetA = baseA
    local targetB = baseB or baseA

    while true do
        if math.random(1, 100) <= 5 then
            targetA = math.random(62, 68) / 100
            targetB = math.random(42, 48) / 100
        end

        baseA = baseA + (targetA - baseA) * 0.03
        baseB = baseB + (targetB - baseB) * 0.03

        local shimmerA = (math.random(-2, 2)) / 200
        local shimmerB = (math.random(-2, 2)) / 300

        local a = clamp01(baseA + shimmerA)
        local b = clamp01(baseB + shimmerB)

        setEffectRegionOpacity(regions[1], a)
        if regions[2] then
            setEffectRegionOpacity(regions[2], b)
        end

        delay(math.random(80, 160))
    end
end

function Glow.runWindow(regions, baseA, baseB)
    local cycleDuration = math.random(18000, 26000)
    local cycleStart = os.clock() * 1000.0

    baseB = baseB or baseA

    while true do
        local now = os.clock() * 1000.0
        local t = (now - cycleStart) / cycleDuration

        if t >= 1.0 then
            cycleStart = now
            cycleDuration = math.random(18000, 26000)
            t = 0.0
        end

        local wave = 0.5 - 0.5 * math.cos(t * math.pi * 2.0)

        local a = clamp01(baseA + wave * 0.02)
        local b = clamp01(baseB + wave * 0.01)

        setEffectRegionOpacity(regions[1], a)
        if regions[2] then
            setEffectRegionOpacity(regions[2], b)
        end

        delay(200)
    end
end

return Glow
