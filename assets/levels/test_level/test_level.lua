local Glow = require("effects.glow")

function Level_onEnter()
    log("running onEnter")
    --startScript("WalkAround")
    --startScript("ZombiePatrol")
    --spawnNpc("knifethug_1", "knifethug", "patrol_1")
    startScript("TableLampGlowLoop")
    startScript("CeilingLampGlowLoop")
    --SpawnThugPatrol()

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
    delay(3000)
    showNarration("Coming Home", "Upon returning to my Florida beach house, I noticed something was off.", 5)
    delay(6000)
    showNarration("Coming Home", "At first, it was the cars - too many of them, all lined up along the pavement like they belonged to the same man.", 5)
    delay(6000)
    showNarration("Coming Home", "Then there was the silence. Not the peaceful kind you pay good money for out here, but the kind that settles in when something's already gone wrong. I proceeded with caution.", 10)
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

function ZombiePatrol()
    spawnNpc("zombie_a", "zombie", "patrol_1")
    spawnNpc("zombie_b", "zombie", "patrol_2")
    spawnNpc("zombie_c", "zombie", "patrol_3")
    spawnNpc("zombie_d", "zombie", "patrol_4")
    while true do
        startWalkNpcToSpawn("zombie_a", "patrol_1")
        startWalkNpcToSpawn("zombie_b", "patrol_2")
        startWalkNpcToSpawn("zombie_c", "patrol_3")
        startWalkNpcToSpawn("zombie_d", "patrol_4")
        delay(10000)
        startWalkNpcToSpawn("zombie_a", "patrol_2")
        startWalkNpcToSpawn("zombie_b", "patrol_3")
        startWalkNpcToSpawn("zombie_c", "patrol_4")
        startWalkNpcToSpawn("zombie_d", "patrol_1")
        delay(10000)
        startWalkNpcToSpawn("zombie_a", "patrol_3")
        startWalkNpcToSpawn("zombie_b", "patrol_4")
        startWalkNpcToSpawn("zombie_c", "patrol_1")
        startWalkNpcToSpawn("zombie_d", "patrol_2")
        delay(10000)
        startWalkNpcToSpawn("zombie_a", "patrol_4")
        startWalkNpcToSpawn("zombie_b", "patrol_1")
        startWalkNpcToSpawn("zombie_c", "patrol_2")
        startWalkNpcToSpawn("zombie_d", "patrol_3")
        delay(10000)
    end
end

function SpawnThugPatrol()
    spawnNpcSmart("guard_a", "zombie", "patrol_start", false)
    spawnNpcSmart("guard_b", "zombie", "patrol_start", false)
    spawnNpcSmart("guard_c", "zombie", "patrol_start", false)
    spawnNpcSmart("guard_d", "zombie", "patrol_start", false)
    assignNpcPatrolRoute("guard_a", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 0
    })
    assignNpcPatrolRoute("guard_b", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 0
    })
    assignNpcPatrolRoute("guard_c", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 0
    })
    assignNpcPatrolRoute("guard_d", {"patrol_1", "patrol_2", "patrol_3", "patrol_4"}, {
        loop = true,
        running = false,
        waitMs = 0
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