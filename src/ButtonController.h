#pragma once

#include "ofMain.h"
#include "PlatformConfig.h"

#ifdef VSE_HAS_GPIO
class ButtonController: public ofThread
{
public:
    ButtonController();
    bool setup(int pinNum);
    void threadedFunction();
    bool wasPressed();
    bool readButton();
    int getPinNum() const { return pinNum; }
    int pinNum;
    int buttonInput;
    bool isPressed, currentValue, oldValue, outputValue;
    bool isReady;
};
#else
class ButtonController
{
public:
    ButtonController() : isReady(false) {}
    bool setup(int) { return false; }
    bool wasPressed() { return false; }
    bool readButton() { return false; }
    int getPinNum() const { return -1; }
    int pinNum = -1;
    int buttonInput = -1;
    bool isPressed = false;
    bool isReady = false;
};
#endif
