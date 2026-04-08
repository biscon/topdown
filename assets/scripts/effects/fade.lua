local Fade = {}

local function clamp01(x)
    if x < 0.0 then return 0.0 end
    if x > 1.0 then return 1.0 end
    return x
end

local function runFade(getFn, setFn, targetValue, durationMs)
    local startValue = getFn()
    local elapsedMs = 0.0

    while elapsedMs < durationMs do
        local t = elapsedMs / durationMs
        local value = startValue + (targetValue - startValue) * t
        setFn(clamp01(value))

        delay(0)
        elapsedMs = elapsedMs + (FrameDelta * 1000.0)
    end

    setFn(clamp01(targetValue))
end

-- Image layers ------------------------------------------------------------

function Fade.fadeLayerUp(layerName, durationMs)
    setLayerVisible(layerName, true)

    runFade(
        function() return layerOpacity(layerName) end,
        function(v) setLayerOpacity(layerName, v) end,
        1.0,
        durationMs
    )
end

function Fade.fadeLayerDown(layerName, durationMs)
    runFade(
        function() return layerOpacity(layerName) end,
        function(v) setLayerOpacity(layerName, v) end,
        0.0,
        durationMs
    )

    setLayerVisible(layerName, false)
end

-- Effects ----------------------------------------------------------------

function Fade.fadeEffectUp(effectId, durationMs)
    setEffectVisible(effectId, true)

    runFade(
        function() return effectOpacity(effectId) end,
        function(v) setEffectOpacity(effectId, v) end,
        1.0,
        durationMs
    )
end

function Fade.fadeEffectDown(effectId, durationMs)
    runFade(
        function() return effectOpacity(effectId) end,
        function(v) setEffectOpacity(effectId, v) end,
        0.0,
        durationMs
    )

    setEffectVisible(effectId, false)
end

-- Effect regions ----------------------------------------------------------

function Fade.fadeEffectRegionUp(effectRegionId, durationMs)
    runFade(
        function() return 0.0 end,
        function(v) setEffectRegionOpacity(effectRegionId, v) end,
        1.0,
        durationMs
    )
end

function Fade.fadeEffectRegionDown(effectRegionId, durationMs)
    runFade(
        function() return 1.0 end,
        function(v) setEffectRegionOpacity(effectRegionId, v) end,
        0.0,
        durationMs
    )
end

return Fade