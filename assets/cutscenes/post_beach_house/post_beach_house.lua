function Cutscene_onEnter()
    showImage("slide1", 0)
    fadeFromBlack(900)
    delay(300)

    showText("I left the house the way I came in.", 500)
    delay(2200)
    clearText(400)
    delay(200)

    showText("Only this time, I knew it wasn’t over.", 500)
    delay(2400)
    clearText(400)
    delay(200)

    showText("You don’t send that many men unless you’re trying to bury something.", 500)
    delay(3200)
    clearText(400)
    delay(300)

    showImage("slide2", 900)
    delay(300)

    showText("The woman on the phone knew where to find me.", 500)
    delay(2400)
    clearText(400)
    delay(200)

    showText("That meant she was connected.", 500)
    delay(2200)
    clearText(400)
    delay(200)

    showText("To them...", 500)
    delay(1500)
    clearText(400)
    delay(200)

    showText("or to what I took from them.", 500)
    delay(2400)
    clearText(400)
    delay(300)

    showImage("slide3", 900)
    delay(300)

    showText("And then there was the other thought.", 500)
    delay(2400)
    clearText(400)
    delay(200)

    showText("The one I’d spent years trying not to think...", 500)
    delay(2800)
    clearText(400)
    delay(200)

    showText("could she still be alive after all this time?", 500)
    delay(2800)
    clearText(400)
    delay(300)

    fadeToBlack(900)
    delay(900)
    endCutscene()
end
