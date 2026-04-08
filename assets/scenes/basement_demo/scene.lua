function Scene_onEnter()
    if not flag("basement_initialized") then
        setFlag("basement_initialized", true)
        setFlag("door_closed", true)

        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end

    playMusic("basement_music", 3000)

    -- Restore door state
    setLayerVisible("door_closed", flag("door_closed"))

    startScript("FlickerLightBulbLoop")
    startScript("FurnaceGlowLoopFancy")
    startScript("FurnaceGlowCoreLoop")
end

function Scene_onExit()
    --stopMusic(1000)
    print("On exit fired from LUA!!")
end

function Scene_use_to_first_floor()
    log("to first floor!!!")
    walkToExit("to_first_floor")
    -- invert flag
    setFlag("door_closed", not flag("door_closed"))
    if flag("door_closed") then
        setLayerVisible("door_closed", true)
        setEffectVisible("door_shadow", true)
        playEmitter("door_close")
        delay(200)
        shakeScreen(180, 14, 45)
    else
        setLayerVisible("door_closed", false)
        setEffectVisible("door_shadow", false)
        playEmitter("door_open")
    end
    return true
end

-- Effect scripts -------------------------

function FurnaceGlowCoreLoop()
    while true do
        setEffectOpacity("furnace_glow_core", math.random(70,100)/100)
        delay(math.random(30, 60))
    end
end

function FurnaceGlowLoopFancy()
    local baseA = 0.85
    local baseB = 0.35

    local targetA = baseA
    local targetB = baseB

    while true do
        if math.random(1, 100) <= 18 then
            targetA = math.random(75, 95) / 100
            targetB = math.random(20, 55) / 100
        end

        -- drift slowly toward target values
        baseA = baseA + (targetA - baseA) * 0.18
        baseB = baseB + (targetB - baseB) * 0.18

        -- fast flame flicker layered on top
        local flickerA = (math.random(-8, 8)) / 100
        local flickerB = (math.random(-12, 12)) / 100

        local a = math.max(0, math.min(1, baseA + flickerA))
        local b = math.max(0, math.min(1, baseB + flickerB))

        setEffectOpacity("furnace_glow_a", a)
        setEffectOpacity("furnace_glow_b", b)

        delay(math.random(40, 120))
    end
end

function SetBulbLighting(on)
    setEffectVisible("light_bulb", on)
    setEffectVisible("light_bulb_halo", on)
    setEffectVisible("no_light_shadow", not on)
    setEffectRegionVisible("light_bulb_color_grade", on)
    setEffectRegionVisible("light_bulb_halo", on)
    setEffectRegionVisible("light_bulb_halo2", on)

    if on then
        setEffectOpacity("light_bulb_halo", 1.0)
    else
        setEffectOpacity("light_bulb_halo", 0.0)
    end
end

function FlickerBulbBurst(baseOn)
    local steps = math.random(2, 5)

    for i = 1, steps do
        local on = not baseOn
        SetBulbLighting(on)

        if on then
            setEffectOpacity("light_bulb_halo", math.random(55, 100) / 100.0)
        end

        delay(math.random(40, 120))

        SetBulbLighting(baseOn)

        if baseOn then
            setEffectOpacity("light_bulb_halo", math.random(75, 100) / 100.0)
        end

        delay(math.random(40, 140))
    end
end

function FlickerLightBulbLoop()
    local on = true
    SetBulbLighting(on)

    while true do
        if on then
            -- mostly stable ON period
            delay(math.random(5000, 10000))

            -- occasional small instability while on
            if math.random(1, 100) <= 35 then
                FlickerBulbBurst(true)
            end

            -- transition toward OFF
            FlickerBulbBurst(true)
            on = false
            SetBulbLighting(false)
        else
            -- mostly stable OFF period
            delay(math.random(3000, 8000))

            -- occasional weak sputter while off
            if math.random(1, 100) <= 45 then
                FlickerBulbBurst(false)
            end

            -- transition toward ON
            FlickerBulbBurst(false)
            on = true
            SetBulbLighting(true)
            setEffectOpacity("light_bulb_halo", math.random(85, 100) / 100.0)
        end
    end
end
