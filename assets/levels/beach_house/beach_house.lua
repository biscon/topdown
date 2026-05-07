local Glow = require("effects.glow")

function Level_onEnter()
    log("running onEnter")
    startScript("TableLampGlowLoop")
    startScript("CeilingLampGlowLoop")
    startScript("StudyCeilingLampGlowLoop")
    startScript("StudyLampGlowLoop")
    startScript("BathroomLampGlowLoop")
    startScript("BeachHouseAudioLoop")
    --[[
    spawnPatrol(
        "guard_",
        "zombie",
        "patrol_start",
        false,
        4,
        {"patrol_1", "patrol_2", "patrol_3", "patrol_4"},
        {
            loop = true,
            running = false,
            waitMs = 5000
        }
    )
    --]]
    RestoreLevel()
    if not flag("beach_house_init") then
        setFlag("beach_house_init", true)
        startScript("IntroNarration")
        SpawnAndAssignInitialPatrols()
    end
    --movePropPositionRelative("test_prop", 500, 0, 8000, "accelerateDecelerate")
end

function RestoreLevel()
    SetPhoneRinging(flag("beach_house_phone_ringing"))
    SetExitUnlocked(flag("beach_house_exit_unlocked"))
end

function SpawnAndAssignInitialPatrols()
    assignNpcPatrolRoute("enemy_7", {"living_room_window1", "living_room_window2"}, {
        loop = true,
        running = false,
        waitMs = 4000
    })
    SpawnGuardPatrol()
end

function PhoneCallCutscene()
    setSoundEmitterEnabled("room_emitter_3", false)
    playSound("phone_pickup")
    delay(500)
    playSound("phone_noise")
    delay(500)
    sayNpc("phone", "...Good. You're still breathing.", MAGENTA, 1800)

    say("Who is this?", CREAM, 1200)

    sayNpc("phone", "Someone who doesn't want you dead in your own house.", MAGENTA, 2600)

    say("Then start talking.", CREAM, 1400)

    sayNpc("phone", "The men you've been killing—", MAGENTA, 1800)
    sayNpc("phone", "they were supposed to report in.", MAGENTA, 2200)
    sayNpc("phone", "They didn't.", MAGENTA, 1400)

    say("So they send more.", CREAM, 1500)

    sayNpc("phone", "Not just more.", MAGENTA, 1500)
    sayNpc("phone", "Better.", MAGENTA, 1200)

    -- 🔥 tension beat + spawn
    delay(800)

    say("How many?", CREAM, 1200)

    sayNpc("phone", "Enough that staying isn't an option.", MAGENTA, 2400)
    sayNpc("phone", "You got in once.", MAGENTA, 1400)
    sayNpc("phone", "Get out the same way.", MAGENTA, 1800)

    say("Why help me?", CREAM, 1400)

    sayNpc("phone", "...Because if they find you, they start asking questions.", MAGENTA, 3000)
    sayNpc("phone", "And I'm not ready for those yet.", MAGENTA, 2200)

    sayNpc("phone", "Move.", MAGENTA, 1000)
    sayNpc("phone", "They're already on their way.", MAGENTA, 2200)
    stopSound("phone_noise")
    playSound("dialtone")
    delay(1600)
    stopSound("dialtone")
    playSound("phone_put_down")
    delay(2000)
    setSoundEmitterEnabled("room_emitter_3", true)
end

function SpawnBackup1()
    log("Spawn backup 1")
    spawnNpcSmart("backup_a", "pistolthug", "backup_arrival", false, false)
    spawnNpcSmart("backup_d", "pistolthug", "backup_arrival", false, false)
    spawnNpcSmart("backup_e", "pistolthug", "backup_arrival", false, false)
    assignNpcPatrolRoute("backup_a", {"hallway_far_end", "hallway_middle", "hallway_start"}, {loop = true,running = false, waitMs = 2000})
    assignNpcPatrolRoute("backup_d", {"hallway_start", "hallway_middle", "hallway_far_end"}, {loop = true,running = false, waitMs = 3000})
    assignNpcPatrolRoute("backup_e", {"study", "bathroom"}, {loop = true, running = false, waitMs = 4000})
end

function SpawnBackup2()
    log("Spawn backup 2")
    playMusic("motivation", 20000)
    spawnNpcSmart("backup_b", "pistolthug", "living_room_window1", false, false)
    spawnNpcSmart("backup_c", "pistolthug", "living_room_window2", false, false)
    assignNpcPatrolRoute("backup_b", {"living_room_window1", "living_room_kitchen", "living_room_window2", "study"}, {loop = true,running = false, waitMs = 2500})
    assignNpcPatrolRoute("backup_c", {"living_room_window2", "living_room_window1", "living_room_kitchen"}, {loop = true,running = false, waitMs = 4000})

end

function SpawnBackup3()
    log("Spawn backup 3")
    spawnNpcSmart("backup_f", "pistolthug", "patrol_2_1", false, false)
    spawnNpcSmart("backup_g", "pistolthug", "patrol_2_3", false, false)
    assignNpcPatrolRoute("backup_f", {"patrol_2_1", "patrol_2_3"}, {loop = true, running = false, waitMs = 2000})
    assignNpcPatrolRoute("backup_g", {"patrol_2_3", "patrol_2_1"}, {loop = true, running = false, waitMs = 2000})
end



function EscapeStartCutscene()
    playSound("car_breaks")
    delay(2000)
    playSound("car_door_open")
    delay(1000)
    playSound("car_door_open")
    delay(500)
    playSound("car_door_open")
    SpawnBackup1()
    setTriggerEnabled("backup2_trigger", true)
    setTriggerEnabled("backup3_trigger", true)
    panCameraTarget("backup_arrival", 1500)
    speakNpc("backup_a", "Search the house!", RED, 3000)
    playSound("drama")
    delay(2500)
    panCameraTarget("bedroom_phone", 1000)
    delay(1000)
    speak("Fuck!")
end

function Level_phoneTrigger()
    log("phone triggered")
    SetPhoneRinging(false)
    disableControls()
    enableScriptCamera()
    PhoneCallCutscene()
    EscapeStartCutscene()
    disableScriptCamera()
    enableControls()
    SetExitUnlocked(true)
end

function Level_bedroomTrigger()
    log("bedroom triggered")
    SetPhoneRinging(true)
    speakProp("phone", "RIIIIIIING!!!", YELLOW, 3000)
    speakNpc("enemy_4", "Come get some!", CREAM)
end

function IntroNarration()
    disableControls()
    enableScriptCamera()
    playSound("drama")
    delay(3000)
    panCameraTarget("intro_camera_1", 7000)
    showNarration("Coming Home", "Upon returning to my idyllic beach house, I noticed something was off.", 5)
    delay(7000)
    panCameraTarget("intro_camera_2", 7000)
    showNarration("Coming Home", "At first, it was the cars - too many of them, all lined up along the pavement like they belonged to the same man.", 5)
    delay(7000)
    panCameraTarget("intro_camera_3", 5000)
    showNarration("Coming Home", "Then there was the silence. Not the peaceful kind you pay good money for out here, but the kind that settles in when something's already gone wrong. I proceeded with caution.", 10)
    delay(5000)
    panCameraTarget("default", 5000)
    delay(5000)
    disableScriptCamera()
    enableControls()
    stopMusic(20000)
end

function SetPhoneRinging(ringing)
    setFlag("beach_house_phone_ringing", ringing)
    setSoundEmitterEnabled("phone_emitter", ringing)
    if ringing then
        setPropAnimation("phone", "Ring")
    else
        setPropAnimation("phone", "Idle")
    end
end

function SetExitUnlocked(unlocked)
    setFlag("beach_house_exit_unlocked", unlocked)
    -- setTriggerEnabled("exit_trigger", unlocked)
end

function WalkAround()
    disableControls()
    while true do
        runTo(800, 727)
        delay(2000)
        walkTo(796, -228)
        delay(3000)
        walkTo(1831, 859)
        delay(2000)
    end
    enableControls()
end

function SpawnGuardPatrol()
    spawnNpcSmart("guard_a", "knifethug", "patrol_start", false)
    --spawnNpcSmart("guard_b", "knifethug", "patrol_start", false)
    spawnNpcSmart("guard_c", "knifethug", "patrol_start", false)
    --spawnNpcSmart("guard_d", "knifethug", "patrol_start", false)
    assignNpcPatrolRoute("guard_a", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 3000
    })
    --[[
    assignNpcPatrolRoute("guard_b", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 2000
    })
    -]]
    assignNpcPatrolRoute("guard_c", {"patrol_4", "patrol_3", "patrol_2", "patrol_1"}, {
        loop = true,
        running = false,
        waitMs = 4000
    })
    --[[
    assignNpcPatrolRoute("guard_d", {"patrol_4", "patrol_3", "patrol_2", "patrol_1"}, {
        loop = true,
        running = false,
        waitMs = 2000
    })
    -]]

    spawnNpcSmart("patrol_2_guard_1", "knifethug", "patrol_2_1", false)
    assignNpcPatrolRoute("patrol_2_guard_1", {"patrol_2_2", "patrol_2_3", "patrol_2_1"}, {
        loop = true,
        running = false,
        waitMs = 5000
    })
    spawnNpcSmart("patrol_2_guard_2", "knifethug", "patrol_2_3", false)
    assignNpcPatrolRoute("patrol_2_guard_2", {"patrol_2_2", "patrol_2_1", "patrol_2_3"}, {
        loop = true,
        running = false,
        waitMs = 2500
    })
end

function spawnPatrol(id_prefix, asset_id, spawn_wp, persistentChase, count, route_spawn_points, options)
    options = options or {}

    local loop = options.loop
    if loop == nil then
        loop = true
    end

    local running = options.running == true
    local waitMs = options.waitMs or 0

    if count == nil or count <= 0 then
        return
    end

    for i = 1, count do
        local npcId = id_prefix .. tostring(i)

        spawnNpcSmart(npcId, asset_id, spawn_wp, persistentChase)

        assignNpcPatrolRoute(npcId, route_spawn_points, {
            loop = loop,
            running = running,
            waitMs = waitMs
        })
    end
end

function Level_onExit()
    log("running onExit")
end

-- Effect scripts -------------------------
function TableLampGlowLoop()
    Glow.runElectric(
        { "table_lamp_glow1", "table_lamp_glow2" },
        0.65,
        0.45
    )
end

function StudyLampGlowLoop()
    Glow.runElectric(
        { "study_lamp_glow1", "study_lamp_glow2" },
        0.65,
        0.45
    )
end

function StudyCeilingLampGlowLoop()
    Glow.runElectric(
        { "unused", "study_ceiling_lamp_glow" },
        0.00,
        0.45
    )
end

function CeilingLampGlowLoop()
    Glow.runElectric(
        { "unused", "ceiling_lamp_glow" },
        0.00,
        0.45
    )
end

function BathroomLampGlowLoop()
    Glow.runElectric(
        { "unused", "bathroom_lamp_glow" },
        0.00,
        0.45
    )
end

-- Audio -----------------------------------------------------------------
function BeachHouseAudioLoop()
    while true do
        -- random delay between events (important!)
        delay(math.random(6000, 18000)) -- 6–18 seconds

        local roll = math.random(1, 100)

        if roll <= 50 then
            -- seagulls (rare, long sounds)
            playEmitter((math.random(1, 2) == 1) and "seagull_emitter_1" or "seagull_emitter_2")

            -- extra long cooldown after gulls so they don’t overlap
            delay(math.random(15000, 30000)) -- 15–30 sec
        else
            -- metal pipe (rare, eerie punctuation)
            playSound("metal_pipe")

            -- slight pause after metallic sound so it "lands"
            delay(math.random(4000, 8000))
        end
    end
end

-- Utility ----------------------------------------------------------------------
function sayNpc(id, text, color, duration)
    speakProp(id, text, color, duration)
    delay(duration)
end

function say(text, color, duration)
    speak(text, color, duration)
    delay(duration)
end
