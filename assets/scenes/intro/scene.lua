local Fade = require("effects.fade")

function Scene_onEnter()
    setActorVisible("main_actor", false)
    playMusic("minimal_piano", 5000)
    startScript("PlayIntroSequence")
end

function PlayIntroSequence()
    disableControls()

    setLayerVisible("reading_letter", false)
    setLayerVisible("leaving_home", false)

    setLayerOpacity("black", 1.0)
    setLayerVisible("black", true)

    delay(500)
    -- ---------------------------------------------------------------------
    -- Still 1: reading letter
    -- ---------------------------------------------------------------------
    setLayerVisible("reading_letter", true)
    Fade.fadeLayerDown("black", 1500)

    sayAt(3*160, 3*300, "Arthur has always been a faithful correspondent.", WHITE)
    sayAt(3*160, 3*330, "His letters arrived with a regularity one could almost set a clock by.", WHITE)

    delay(700)

    sayAt(3*160, 3*360, "And yet... it has been weeks since I last heard from him.", WHITE)

    delay(700)

    sayAt(3*160, 3*390, "His last letter bore the mark of the Innsmouth post office.", WHITE)
    sayAt(3*160, 3*420, "A place I know only by name.", WHITE)

    delay(1200)

    Fade.fadeLayerUp("black", 1200)
    setLayerVisible("reading_letter", false)

    -- ---------------------------------------------------------------------
    -- Still 2: leaving home
    -- ---------------------------------------------------------------------
    setLayerVisible("leaving_home", true)
    Fade.fadeLayerDown("black", 1200)

    sayAt(3*160, 3*340, "Whatever has kept him silent, I mean to learn the cause.", WHITE)

    delay(700)

    sayAt(3*160, 3*370, "Innsmouth lies some distance from here,", WHITE)
    sayAt(3*160, 3*400, "but I can hardly remain idle and wonder.", WHITE)

    delay(700)

    sayAt(3*160, 3*430, "I shall see Arthur for myself.", WHITE)

    delay(1500)

    Fade.fadeLayerUp("black", 1200)
    changeScene("town_square")
end
