local Fade = require("effects.fade")

function Scene_onEnter()
    if not flag("town_square_init") then
        setFlag("town_square_init", true)
        -- setEffectVisible("black_screen_quad", true)
        --setEffectRegionVisible("scene_air_haze", false)
        startScript("PlayIntroCutscene")
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
    startScript("FogPulseLoop")
    startScript("TownSquareAudioLoop")
end

function StopIntroMusic()
    delay(5000)
    stopMusic(10000)
end

function Scene_onExit()
    stopScript("TownSquareAudioLoop")
    stopSound("wind_ambience")
    stopSound("seagull1")
    stopSound("seagull2")
    stopMusic(3000)
end

function Scene_look_hotel_sign()
    say("\"The Adrift Inn.\"")
    say("The letters look newer than the wood they're nailed to.")
    return true
end

-- let both actions do the same where it doesnt make sense to have to different ones
function Scene_use_hotel_sign()
    return Scene_look_hotel_sign()
end

function Scene_look_store_sign()
    say("\"O'Malley's Goods & Supplies.\"")
    say("The paint is fading, but someone keeps it from disappearing completely.")
    return true
end

function Scene_use_store_sign()
    return Scene_look_store_sign()
end

function Scene_look_alley()
    say("A narrow alley disappearing into shadow.")
    say("Something about it makes me not want to linger.")
    return true
end

function Scene_use_alley()
    return Scene_look_alley()
end

function Scene_look_anchors()
    say("Rust has eaten most of the metal away.")
    say("Like they haven't seen a proper ship in years.")
    return true
end

function Scene_use_anchors()
    return Scene_look_anchors()
end

function Scene_look_to_store()
    say("A plain little storefront.")
    say("Still, it feels more honest than most things here.")
    return true
end

function Scene_look_to_hotel_lobby()
    say("A shabby hotel entrance.")
    say("The sort of place where you sleep in your clothes and keep one eye open.")
    return true
end

-- Cut scenes ------------------------------------------
function FadeDownAsync()
    Fade.fadeEffectDown("black_screen_quad", 3000)
end

function PlayIntroCutscene()
    disableControls()

    startSayAt(3*50, 3*340, "Day 1 - A Sense of Unease", WHITE, 5000)
    delay(2000)
    walkTo(3*340,3*306)
    face("back")
    delay(1000)
    say("Strange place for him to end up... this town feels wrong somehow.")
    stopMusic(5000)
    say("Best find a room for the night before it gets any later.")
    --startScript("StopIntroMusic")
    enableControls()
end
-- Effect scripts -------------------------

function FogPulseLoop()
    local baseA = 0.34
    local baseB = 0.28

    local cycleDuration = 18000
    local cycleStart = os.clock() * 1000.0

    while true do
        local now = os.clock() * 1000.0
        local t = (now - cycleStart) / cycleDuration

        if t >= 1.0 then
            cycleStart = now
            cycleDuration = math.random(16000, 26000)
            t = 0.0
        end

        -- smooth ebb/flow wave from 0..1..0
        local wave = 0.5 - 0.5 * math.cos(t * math.pi * 2.0)

        -- broad fog breathes more than detail layer
        local a = baseA + wave * 0.035
        local b = baseB + wave * 0.015

        -- occasional slightly denser passing patch
        if math.random(1, 100) <= 6 then
            a = a + math.random(0, 10) / 1000.0
            b = b + math.random(0, 6) / 1000.0
        end

        a = math.max(0.0, math.min(1.0, a))
        b = math.max(0.0, math.min(1.0, b))

        setEffectRegionOpacity("square_ground_fog", a)
        setEffectRegionOpacity("square_ground_fog_detail", b)
        setEffectRegionOpacity("alley_fog", a)
        setEffectRegionOpacity("alley_fog_detail", b)

        delay(math.random(120, 220))
    end
end

-- Audio -----------------------------------------------------------------
function TownSquareAudioLoop()
    -- start base ambience
    playSound("wind_ambience")

    while true do
        -- random delay between events (important!)
        delay(math.random(6000, 18000)) -- 6–18 seconds

        local roll = math.random(1, 100)

        if roll <= 30 then
            -- seagulls (rare, long sounds)
            playSound((math.random(1, 2) == 1) and "seagull1" or "seagull2")

            -- extra long cooldown after gulls so they don’t overlap
            delay(math.random(15000, 30000)) -- 15–30 sec

        elseif roll <= 65 then
            -- wood creaks (mid frequency)
            playSound((math.random(1, 2) == 1) and "wood_creak1" or "wood_creak2")

        else
            -- metal pipe (rare, eerie punctuation)
            playSound("metal_pipe")

            -- slight pause after metallic sound so it "lands"
            delay(math.random(4000, 8000))
        end
    end
end