#include "RotaryEncoderController.h"

#ifdef VSE_HAS_GPIO

std::map<int, RotaryEncoderController *> RotaryEncoderController::instanceMap;

RotaryEncoderController::RotaryEncoderController()
    : clkPin(-1), dtPin(-1), lastState(0), currentRotation(NONE)
{
    isReady = false;
}

bool RotaryEncoderController::setup(int clkPin, int dtPin)
{
    this->clkPin = clkPin;
    this->dtPin = dtPin;

    int status = wiringPiSetupGpio();
    if (status != -1)
    {
        ofLogVerbose() << "Rotary Encoder Ready: " << clkPin << " " << dtPin;
        isReady = true;
    }
    else
    {
        ofLogError() << "Rotary Encoder Failed: " << status;
    }

    pinMode(clkPin, INPUT);
    pinMode(dtPin, INPUT);

    pullUpDnControl(clkPin, PUD_UP);
    pullUpDnControl(dtPin, PUD_UP);

    lastState = (digitalRead(clkPin) << 1) | digitalRead(dtPin);

    instanceMap[clkPin] = this;
    instanceMap[dtPin] = this;

    // Register the same static ISR for both pins.
    // The ISR calls handleInterrupt() on every registered instance;
    // the lookup table correctly produces no-op (0) for instances
    // whose pins haven't actually changed, so this is safe.
    if (wiringPiISR(clkPin, INT_EDGE_BOTH, &RotaryEncoderController::staticISR) < 0)
        return false;
    if (wiringPiISR(dtPin, INT_EDGE_BOTH, &RotaryEncoderController::staticISR) < 0)
        return false;

    return true;
}

void RotaryEncoderController::staticISR()
{
    for (const auto &pair : instanceMap)
        pair.second->handleInterrupt();
}

void RotaryEncoderController::handleInterrupt()
{
    int state = (digitalRead(clkPin) << 1) | digitalRead(dtPin);

    static const int8_t dirTable[4][4] = {
        { 0,  1, -1,  0},
        {-1,  0,  0,  1},
        { 1,  0,  0, -1},
        { 0, -1,  1,  0}
    };

    int direction = dirTable[lastState & 0x3][state];

    if (state == 0b11)
    {
        if (direction == 1)
            currentRotation = RIGHT;
        else if (direction == -1)
            currentRotation = LEFT;
    }
    lastState = state;
}

RotaryEncoderController::Rotation RotaryEncoderController::getRotation()
{
    Rotation rotation = currentRotation;
    currentRotation = NONE;
    return rotation;
}

#endif
