function Cutscene_onEnter()
    fadeFromBlack(900)

    showImage("car_drive", 1000)
    delay(300)
    showText("I left the house the way I came in.", 500)
    delay(2200)
    clearText(400)

    showText("Only this time, I knew it wasn't over.", 500)
    delay(2400)
    clearText(400)

    showImage("chapel_road", 900)
    delay(300)
    showText("And then there was the other thought.", 500)
    delay(2200)
    clearText(400)

    fadeToBlack(900)
    endCutscene()
end
