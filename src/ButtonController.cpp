#include "ButtonController.h"

#ifdef VSE_HAS_GPIO

ButtonController::ButtonController()
{
    isReady = false;
    isPressed = false;
    currentValue = false;
    oldValue = false;
    buttonInput = -1;
    pinNum = -1;
}

bool ButtonController::setup(int pinNum)
{
    this->pinNum = pinNum;
    buttonInput = pinNum;
    int status = wiringPiSetupGpio();
    if (status != -1)
    {
        ofLogVerbose() << "wiringPiSetup PASS";
        isReady = true;
        pinMode(buttonInput, INPUT);
        pullUpDnControl(buttonInput, PUD_UP);
    }
    else
    {
        ofLogError() << "wiringPiSetup FAIL status: " << status;
    }

    if (isReady)
    {
        startThread(true);
    }
    return isReady;
}

void ButtonController::threadedFunction()
{
    while (isThreadRunning())
    {
        lock();
        currentValue = readButton();
        if (currentValue != oldValue && oldValue)
        {
            isPressed = true;
        }
        oldValue = currentValue;
        unlock();
        sleep(10);
    }
}

bool ButtonController::wasPressed()
{
    outputValue = isPressed;
    isPressed = false;
    return outputValue;
}

bool ButtonController::readButton()
{
    if (digitalRead(buttonInput) == 0)
        return true;
    else
        return false;
}

#endif
