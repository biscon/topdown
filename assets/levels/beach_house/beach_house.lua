local Glow = require("effects.glow")

function Level_onEnter()
    log("running onEnter")
    --startScript("WalkAround")
    --startScript("ZombiePatrol")
    --spawnNpc("knifethug_1", "knifethug", "patrol_1")
    startScript("TableLampGlowLoop")
    startScript("CeilingLampGlowLoop")
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

    SpawnGuardPatrol()
    if not flag("beach_house_init") then
        setFlag("beach_house_init", true)

        --startScript("IntroNarration")
    end
end

function IntroNarration()
    disableControls()
    enableScriptCamera()
    playMusic("pistolero", 3000)
    --playSound("drama")
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

local count = 1
function Level_onTestTrigger1()
    log("running onTestTrigger1")
    spawnNpcSmart("enemy_" .. count, "knifethug", "test_spawn", false)
    count = count + 1
end

--local count = 1
function Level_onTestTrigger2()
    spawnNpcSmart("enemy_" .. count, "pistolthug", "patrol_1", false)
    count = count + 1
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
    spawnNpcSmart("guard_a", "pistolthug", "patrol_start", false)
    spawnNpcSmart("guard_b", "pistolthug", "patrol_start", false)
    spawnNpcSmart("guard_c", "pistolthug", "patrol_start", false)
    spawnNpcSmart("guard_d", "pistolthug", "patrol_start", false)
    assignNpcPatrolRoute("guard_a", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 3000
    })
    assignNpcPatrolRoute("guard_b", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 2000
    })
    assignNpcPatrolRoute("guard_c", {"patrol_4", "patrol_3", "patrol_2", "patrol_1"}, {
        loop = true,
        running = false,
        waitMs = 4000
    })
    assignNpcPatrolRoute("guard_d", {"patrol_4", "patrol_3", "patrol_2", "patrol_1"}, {
        loop = true,
        running = false,
        waitMs = 2000
    })

    spawnNpcSmart("patrol_2_guard_1", "pistolthug", "patrol_2_1", false)
    assignNpcPatrolRoute("patrol_2_guard_1", {"patrol_2_2", "patrol_2_3", "patrol_2_1"}, {
        loop = true,
        running = false,
        waitMs = 5000
    })
    spawnNpcSmart("patrol_2_guard_2", "pistolthug", "patrol_2_3", false)
    assignNpcPatrolRoute("patrol_2_guard_2", {"patrol_2_2", "patrol_2_1", "patrol_2_3"}, {
        loop = true,
        running = false,
        waitMs = 8000
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

function CeilingLampGlowLoop()
    Glow.runElectric(
        { "unused", "ceiling_lamp_glow" },
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