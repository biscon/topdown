function Cutscene_onEnter()
    playMusic("dark_atmosphere")
    delay(1000)
    showImage("slide1", 0)
    fadeFromBlack(900)
    delay(300)

    showText("Driving to the chapel, I knew one thing for certain.", 500)
    delay(2800)
    clearText(400)
    delay(200)

    showText("My past had found me.", 500)
    delay(2200)
    clearText(400)
    delay(200)

    showText("You don't send that many men unless you're trying to bury something.", 500)
    delay(3200)
    clearText(400)
    delay(1500)

    showImage("slide2", 900)
    delay(300)

    showText("The woman on the phone knew where to find me.", 500)
    delay(2400)
    clearText(400)
    delay(200)

    showText("That wasn't luck.", 500)
    delay(1800)
    clearText(400)
    delay(200)

    showText("Someone had opened a door I thought I'd sealed.", 500)
    delay(3000)
    clearText(400)
    delay(200)

    showText("I just didn't know which side she was standing on.", 500)
    delay(3000)
    clearText(400)
    delay(300)

    showImage("slide3", 900)
    delay(300)

    showText("And then there was the other question.", 500)
    delay(2400)
    clearText(400)
    delay(200)

    showText("The one I'd spent years trying not to ask...", 500)
    delay(2800)
    clearText(400)
    delay(200)

    showText("could she still be alive after all this time?", 500)
    delay(2800)
    clearText(400)
    delay(1500)

    fadeToBlack(900)
    delay(1200)
    endCutscene()
end
