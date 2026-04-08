function Scene_onEnter()
    if not flag("basement_initialized") then
        setFlag("basement_initialized", true)

        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end

    stopScript("FlickerLightBulbLoop")
    startScript("FlickerLightBulbLoop")

    stopScript("FurnaceGlowLoopFancy")
    startScript("FurnaceGlowLoopFancy")

    stopScript("FurnaceGlowCoreLoop")
    startScript("FurnaceGlowCoreLoop")

    playMusic("basement_music", 3000)
end

function Scene_onExit()
    --stopMusic(1000)
    print("On exit fired from LUA!!")
end

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

function FurnaceGlowLoopFancyOLD()

    -- base levels drift slowly over time
    local baseA = 0.85
    local baseB = 0.35

    while true do

        -- occasionally change the base intensity (slow fire breathing)
        if math.random(1,100) <= 25 then
            baseA = math.random(75,95) / 100
            baseB = math.random(20,55) / 100
        end

        -- fast flame flicker layered on top
        local flickerA = (math.random(-8,8)) / 100
        local flickerB = (math.random(-12,12)) / 100

        local a = math.max(0, math.min(1, baseA + flickerA))
        local b = math.max(0, math.min(1, baseB + flickerB))

        setEffectOpacity("furnace_glow_a", a)
        setEffectOpacity("furnace_glow_b", b)

        delay(math.random(40,120))
    end
end

function SetBulbLighting(on)
    setEffectVisible("light_bulb", on)
    setEffectVisible("light_bulb_halo", on)
    setEffectVisible("no_light_shadow", not on)

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

function Looper()
    while true do
        log("tick")
        --sayActor("npc_actor", "Looping...", CYAN, 700)
        delay(1200)
    end
end

function Scene_use_furnace()
    if not walkToHotspot("furnace") then
        return false
    end

    log("about to face right")
    face("right")

    log("about to play reach_right")
    if not playAnimation("reach_right") then
        log("pickup anim missing, skipping")
    end

    delay(1000)

    setPropAnimation("cat", "walk_right")
    if flag("cat_moved") then
        log("cat moved")
        setPropFlipX("cat", true)
        playSound("cat_meow")
        movePropBy("cat", -250, -20, 1500, "accelerateDecelerate")
        setFlag("cat_moved", false)
    else
        log("cat not moved")
        setPropFlipX("cat", false)
        playSound("cat_meow")
        movePropBy("cat", 250, 20, 1500, "accelerateDecelerate")
        setFlag("cat_moved", true)
    end

    delay(1500)
    setPropAnimation("cat", "idle_right")

    playSound("cat_meow")
    sayProp("cat", "Meooow!", PINK)

    say("Easy there buddy.")

    sayProp("cat", "Meoooooow FOR HELVEDE!!!", RED)

    return true
end

function Scene_look_furnace()
    walkToHotspot("furnace")
    face("back")
    --delay(500)
    --say("A coal furnace. Mean-looking bastard.")
    giveItem("rusty_key")
    giveItem("sausage")
    say("I found a rusty key inside!.")
    return true
end

function Scene_look_to_first_floor()
    if not walkToExit("to_first_floor") then
        return false
    end

    face("back")
    say("Up we go.")
    return changeScene("first_floor", "stairs")
end

controlledActor = "main_actor"

function Scene_look_light_bulb()
    if controlledActor == "main_actor" then
        controlledActor = "npc_actor"
    else
        controlledActor = "main_actor"
    end
    controlActor(controlledActor)
    say("I am now in control.")
    -- walkActorTo("npc_actor", 1527, 663)
end

function Scene_use_window()
    disableControls()
    walkToHotspot("window")
    face("left")
    if not hasItem("rusty_key") then
        say("The window is locked. I should probably find the key first.")
    else
        say("The window is locked, maybe I should try to use the rusty key I found.")
    end
    enableControls()
    return true
end

function Scene_use_item_rusty_key_on_hotspot_window()
    disableControls()
    clearHeldItem()
    walkToHotspot("window")
    face("left")
    say("The key turns. The window unlocks.")

    setFlag("windowUnlocked", true)
    removeItem("rusty_key")
    enableControls()
    return true
end

function Scene_use_item_rusty_key_on_hotspot_furnace()
    say("I am not gonna burn it!!.")
    return false
end

function Scene_look_actor_npc_actor()
    say("He looks exhausted.")
    return true
end

function Scene_use_item_sausage_on_actor_main_actor()
    say("Om nom nom!")
    removeItem("sausage")
    return true
end

--[[
function Scene_use_actor_npc_actor()
    sayActor("npc_actor", "What do you want?")

    local choice = dialogue("npc_actor_intro")

    if choice == "who_are_you" then
        say("Who are you?")
        sayActor("npc_actor", "None of your business.")
    elseif choice == "what_happened" then
        say("What happened here?")
        sayActor("npc_actor", "Nothing you'd understand.")
    elseif choice == "goodbye" then
        say("Never mind.")
    else
        say("...")
    end

    return true
end
--]]

function BuildNpcActorIntroHiddenOptions()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_npc_name"),
        what_happened = flag("asked_npc_what_happened"),
        apologize = not flag("insulted_npc_actor"),
        insult = flag("insulted_npc_actor")
    })
end

function Scene_use_actor_npc_actor()
    sayActor("npc_actor", "What do you want chico?")

    while true do
        local hidden = BuildNpcActorIntroHiddenOptions()
        local choice = dialogue("npc_actor_intro", hidden)

        if choice == nil then
            say("...")
            return true
        end

        if choice == "who_are_you" then
            setFlag("asked_npc_name", true)
            say("Who are you?")
            sayActor("npc_actor", "None of your business.")

        elseif choice == "what_happened" then
            setFlag("asked_npc_what_happened", true)
            say("What happened here?")
            sayActor("npc_actor", "Long day. Bad furnace. Worse company.")

        elseif choice == "rusty_key" then
            if hasItem("rusty_key") then
                say("I found a rusty key.")
                sayActor("npc_actor", "Good for you. Try not to lose it in your own skull.")
            else
                say("Seen any keys around?")
                sayActor("npc_actor", "No. And if I had, I wouldn't hand them to you.")
            end

        elseif choice == "sausage" then
            if hasItem("sausage") then
                say("I found a sausage.")
                sayActor("npc_actor", "Congratulations. A feast for kings.")
                say("You want it?")
                sayActor("npc_actor", "Not if you've been carrying it around in your pocket.")
            else
                say("You hungry?")
                sayActor("npc_actor", "Always. Doesn't mean I trust your cooking.")
            end

        elseif choice == "insult" then
            setFlag("insulted_npc_actor", true)
            say("You seem like a real joy to be around.")
            sayActor("npc_actor", "And you seem like a corpse that forgot to lie down.")

        elseif choice == "apologize" then
            say("Alright. Sorry.")
            sayActor("npc_actor", "Hmph. Better.")

        elseif choice == "give_sausage" then
            if hasItem("sausage") then
                say("Here. Take the sausage.")
                removeItem("sausage")
                sayActor("npc_actor", "Well... that's the least terrible thing you've done.")
                setFlag("npc_actor_fed", true)
            else
                say("I don't actually have it anymore.")
            end

        elseif choice == "goodbye" then
            say("Never mind.")
            return true

        else
            say("...")
            return true
        end
    end
end

function BuildMainActorConversationHiddenOptions()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_main_actor_name"),
        what_happened = flag("asked_main_actor_what_happened"),
        rusty_key = not hasItem("rusty_key"),
        sausage = not hasItem("sausage"),
        give_sausage = not hasItem("sausage") or flag("main_actor_fed"),

        insult = flag("insulted_main_actor") or flag("apologized_to_main_actor"),
        apologize = (not flag("insulted_main_actor")) or flag("apologized_to_main_actor")
    })
end

function Scene_use_actor_main_actor()
    sayActor("main_actor", "Yes?")

    Adv.runConversationDynamic("npc_actor_intro", {
        who_are_you = function()
            setFlag("asked_main_actor_name", true)
            sayActor("npc_actor", "Who are you, exactly?")
            sayActor("main_actor", "The poor bastard doing all the work around here.")
        end,

        what_happened = function()
            setFlag("asked_main_actor_what_happened", true)
            sayActor("npc_actor", "What happened here?")
            sayActor("main_actor", "Bad luck, bad timing, and apparently bad company.")
        end,

        rusty_key = function()
            sayActor("npc_actor", "About that rusty key...")
            sayActor("main_actor", "Then keep hold of it. It looks important.")
        end,

        sausage = function()
            sayActor("npc_actor", "About this sausage...")
            sayActor("main_actor", "I was happier before I knew you had one.")
        end,

        give_sausage = function()
            sayActor("npc_actor", "Here. You can have the sausage.")
            removeItem("sausage")
            setFlag("main_actor_fed", true)
            sayActor("main_actor", "Finally, a productive conversation.")
        end,

        insult = function()
            setFlag("insulted_main_actor", true)
            sayActor("npc_actor", "You seem like a real pain in the ass.")
            playSound("cat_meow")
            sayProp("cat", "Meooow!", PINK)
            sayActor("npc_actor", "See even the cat thinks you're an asshole.")
            sayActor("main_actor", "And you seem very brave now that you're standing over there.")
        end,

        apologize = function()
            setFlag("insulted_main_actor", false)
            setFlag("apologized_to_main_actor", true)
            sayActor("npc_actor", "Alright. Sorry.")
            sayActor("main_actor", "Accepted. Don't make it a habit.")
        end,

        goodbye = function()
            sayActor("npc_actor", "Never mind.")
            return "exit"
        end
    }, BuildMainActorConversationHiddenOptions)

    return true
end
