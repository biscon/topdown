local bell = require("audio.desk_bell")
local Glow = require("effects.glow")

function Scene_onEnter()
    if not flag("store_init") then
        setFlag("store_init", true)
        -- first time only stuff
    end

    if flag("distracted_by_dog") then
        RestoreDistractedByDog()
    else
        setSoundEmitterEnabled("dog_snoring", true)
    end

    startScript("LampGlowLoop")
    startScript("StoreAmbienceLoop")
end

function Scene_onExit()
    stopScript("HotelAmbienceLoop")
    --setSoundEmitterEnabled("store_room_tone", false)
end

function Scene_look_ledger()
    if flag("saw_store_ledger") then
        say("I've no need to examine it further.")
        return true
    end
    if not flag("hotel_room_denied") then
        say("A business ledger.")
    else
        say("A business ledger.")
        say("Names, dates, and deliveries carefully entered.")
        say("The sort of thing one might consult to learn who deals with whom in a place like this.")
    end
    return true
end

function Scene_use_ledger()
    if not flag("hotel_room_denied") then
        return Scene_look_ledger()
    end

    if flag("saw_store_ledger") then
        say("I've already gotten what I needed from that.")
        return true
    end

    disableControls()

    playAnimation("reach_left")
    delay(600)

    if not flag("distracted_by_dog") then
        sayActor("store_clerk", "That is not for sale, sir.")
    else
        say("The ledger is full of deliveries to the hotel.")
        say("Food, lamp oil, soap... enough to serve more guests than the clerk admits to having.")
        setFlag("saw_store_ledger", true)
        StopComfortDog()
        sayActor("store_clerk", "Can I help you, sir?")
    end

    enableControls()
    return true
end

function StopComfortDog()
    stopScript("ComfortDog")
    playPropAnimation("german_shepard", "goto_sleep")
    delay(600)
    startSayActor("store_clerk", "There now, be still.")
    delay(1500)
    setSoundEmitterEnabled("dog_snoring", true)
    walkActorTo("store_clerk", 3*215, 3*291)
    faceActor("store_clerk", "right")
    setFlag("distracted_by_dog", false)
end

-- Saved by the bell ------------------------------------------------------
function Scene_look_bell()
    say("A small counter bell.")
    say("It looks as though it gets more use than the clerk would like.")
    return true
end

function Scene_use_bell()
    if flag("distracted_by_dog") then
        bell.RingDeskBell(1)
        sayActor("store_clerk", "One moment, sir. I'll be with you directly.")
        StopComfortDog()
        faceActor("store_clerk", "front")
        sayActor("store_clerk", "How can I help you?.")
        return true
    end
    local times = getInt("store_bell_rang_count")
    if times < 0 then
        times = 0
    end

    if times == 0 then
        bell.RingDeskBell(1)
        setInt("store_bell_rang_count", 1)
        faceActor("store_clerk", "front")
        sayActor("store_clerk", "No need for that, sir. I'm right here.")
    elseif times == 1 then
        bell.RingDeskBell(2)
        setInt("store_bell_rang_count", 2)
        faceActor("store_clerk", "front")
        sayActor("store_clerk", "Please, sir. There's no call for that.")
    else
        say("I had better not be overly rude.")
    end
    return true
end

-- Flavor hotspots --------------------------------------------------------
function Scene_look_notice_board()
    say("Notices, schedules, and scraps of paper.")
    say("Some are so old the ink has nearly vanished.")
    return true
end

function Scene_use_notice_board()
    return Scene_look_notice_board()
end

function Scene_look_shelves()
    say("Tinned goods, jars, and things I can't quite identify.")
    say("All of it looks like it's been here a long time.")
    return true
end

function Scene_use_shelves()
    return Scene_look_shelves()
end

function Scene_look_file_cabinet()
    say("Drawers for records.")
    say("Organized, but not inviting.")
    return true
end

function Scene_use_file_cabinet()
    if not flag("distracted_by_dog") then
        sayActor("store_clerk", "Kindly keep your hands off my files, sir.")
        face("left")
        say("My apologies.")
    else
        say("I have no wish to rummage through a cabinet full of dull papers.")
    end

    return true
end

-- Store clerk ----------------------------------------------------

function Scene_look_actor_store_clerk()
    walkToHotspot("ledger")
    face("left")
    say("A young man trying very hard not to draw attention to himself.")
    return true
end

function Scene_use_actor_store_clerk()
    if flag("distracted_by_dog") then
        sayActor("store_clerk", "One moment, sir. I'll be with you directly.")
        StopComfortDog()
        faceActor("store_clerk", "front")
        sayActor("store_clerk", "How can I help you?.")
        return true
    end

    disableControls()
    walkToHotspot("ledger")
    face("left")
    faceActor("store_clerk", "right")
    sayActor("store_clerk", "Yes, sir?")
    enableControls()

    Adv.runConversationDynamic("store_clerk_intro", {

        buy_supplies = function()
            setFlag("asked_store_buy", true)
            say("Do you sell provisions?")
            sayActor("store_clerk", "A few things, sir. Not so much as we once did.")
            sayActor("store_clerk", "What there is, you see before you.")
        end,

        about_town = function()
            setFlag("asked_store_town", true)
            say("I passed a church on my way in.")
            say("St. Mary's, I think.")
            say("It looked abandoned.")
            sayActor("store_clerk", "It is.")
            say("What became of it?")
            sayActor("store_clerk", "Fewer and fewer folk went there.")
            say("Just drifted away?")
            sayActor("store_clerk", "Something like that.")
            sayActor("store_clerk", "They found other habits.")
        end,

        inn = function()
            setFlag("asked_store_inn", true)
            say("The inn seems empty.")
            sayActor("store_clerk", "It is not as empty as it looks.")
            say("The clerk gave me a different impression.")
            sayActor("store_clerk", "He would.")
            say("Why?")
            sayActor("store_clerk", "I think I'd be careful where I lodged, if I were you.")
        end,

        friend = function()
            setFlag("asked_store_friend", true)
            say("I'm looking for someone.")
            sayActor("store_clerk", "Then I hope you find him soon, sir.")
            say("Why soon?")
            sayActor("store_clerk", "Because Innsmouth is not a place for lingering.")
        end,

        hotel_deliveries = function()
            setFlag("asked_store_hotel_deliveries", true)
            say("The hotel seems to be receiving plenty of supplies.")
            sayActor("store_clerk", "It receives what is ordered.")
            say("Rather a lot for an inn with no rooms to let.")
            sayActor("store_clerk", "I would not speak too freely of other men's business, sir.")
            say("Then the inn is doing business?")
            sayActor("store_clerk", "I did not say that.")
        end,

        goodbye = function()
            say("I'll leave you to it.")
            sayActor("store_clerk", "Yes, sir.")
            return "exit"
        end

    }, function()
        return Adv.hiddenOptions({
            buy_supplies = flag("asked_store_buy"),
            about_town = flag("asked_store_town"),
            inn = flag("asked_store_inn") or (not flag("hotel_room_denied")) or flag("saw_store_ledger"),
            friend = flag("asked_store_friend"),
            hotel_deliveries = (not flag("saw_store_ledger")) or flag("asked_store_hotel_deliveries")
        })
    end)

    return true
end

-- Dog the bounty hunter ---------------------------------------------

function RestoreDistractedByDog()
    local pos = getHotspotInteractionPosition("dog")
    setActorPosition("store_clerk", pos.x, pos.y)
    faceActor("store_clerk", "left")
    setPropAnimation("german_shepard", "idle")
    startScript("ComfortDog")
end

function ComfortDog()
    local barks = {
        "Woof!",
        "Arf!",
        "Rrrf!"
    }

    local sootheLines = {
        "Easy now...",
        "Steady, lad.",
        "Hush now.",
        "There, there.",
        "Quiet now, boy."
    }

    while true do
        local cyclesThisMinute = math.random(2, 3)

        for cycle = 1, cyclesThisMinute do
            local barkCount = math.random(1, 3)

            for bark = 1, barkCount do
                playPropAnimation("german_shepard", "bark")
                delay(350)
                setPropAnimation("german_shepard", "idle")
                PlayDogBark()
                startSayAt(3*100, 3*300, barks[math.random(#barks)], YELLOW)
                delay(1900)
            end

            playActorAnimation("store_clerk", "pickup_left")
            delay(300)
            playEmitter("dog_whine")
            startSayActor("store_clerk", sootheLines[math.random(#sootheLines)])
            delay(math.random(5000, 6000))
        end

        local remainingDelay = math.random(12000, 22000)
        delay(remainingDelay)
    end
end

function Scene_click_dog()
    if not flag("distracted_by_dog") then
        return false
    end
    say("I don't want to disturb them.")
    return true
end

function Scene_use_dog()
    if flag("saw_store_ledger") then
        say("I better not disturb it again.")
        return true
    end
    if flag("distracted_by_dog") then
        say("I don't want to disturb them.")
        return true
    end
    disableControls()
    playAnimation("pickup_left")
    delay(300)
    setSoundEmitterEnabled("dog_snoring", false)
    playPropAnimation("german_shepard", "wake_up")
    delay(1000)

    for i = 1, 3 do
        if i == 2 then
            faceActor("store_clerk", "front")
            startWalkTo(3*240, 3*336)
        end

        playPropAnimation("german_shepard", "bark")
        delay(500)
        setPropAnimation("german_shepard", "idle")
        PlayDogBark()
        startSayAt(3*100, 3*300, ({ "Woof!", "Arf!", "Rrrf!" })[math.random(3)], ORANGE)
        delay(1900)
    end

    face("left")
    sayActor("store_clerk", "Easy there, little fellow.")
    walkActorToHotspot("store_clerk", "dog")
    faceActor("store_clerk", "left")
    playActorAnimation("store_clerk", "pickup_left")
    delay(300)
    sayActor("store_clerk", "There now. Be still.")

    enableControls()
    setFlag("distracted_by_dog", true)
    startScript("ComfortDog")

    return true
end

-- Effect scripts -------------------------
function LampGlowLoop()
    Glow.runFire(
        { "lamp_glow1", "lamp_glow2" },
        0.50,
        0.35
    )
end

-- Audio ----------------------------------------------

function StoreAmbienceLoop()
    -- base bed
    --setSoundEmitterEnabled("store_room_tone", true)

    while true do
        delay(math.random(8000, 20000)) -- 8–20 sec gaps

        local roll = math.random(1, 100)

        if roll <= 40 then
            -- structure creaks
            local creaks = {
                "floor_creak_left",
                "floor_creak_upstairs",
                "wall_creak_right"
            }
            playEmitter(creaks[math.random(#creaks)])

        elseif roll <= 70 then
            -- subtle object sounds
            local objects = {
                "three_knocks",
                "metal_pipe"
            }
            -- playEmitter(objects[math.random(#objects)])

        else
            -- the important one: upstairs presence

            -- give it space to breathe
            delay(math.random(5000, 10000))
        end
    end
end

local _dogBarkIndex = 0

function PlayDogBark()
    local emitters = {
        "dog_bark1",
        "dog_bark2",
        "dog_bark3"
    }

    _dogBarkIndex = _dogBarkIndex + 1
    if _dogBarkIndex > #emitters then
        _dogBarkIndex = 1
    end

    playEmitter(emitters[_dogBarkIndex])
end
