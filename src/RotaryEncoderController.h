#pragma once

#include "PlatformConfig.h"

#ifdef VSE_HAS_GPIO
#include <atomic>
#include <map>

class RotaryEncoderController
{
public:
    enum Rotation
    {
        NONE,
        LEFT,
        RIGHT
    };

    RotaryEncoderController();

    bool setup(int clkPin, int dtPin);
    bool isReady = false;
    Rotation getRotation();

private:
    static void staticISR();

    int clkPin;
    int dtPin;

    int lastState;
    std::atomic<Rotation> currentRotation;

    static std::map<int, RotaryEncoderController *> instanceMap;

    void handleInterrupt();
};
#else
class RotaryEncoderController
{
public:
    enum Rotation { NONE, LEFT, RIGHT };
    RotaryEncoderController() : isReady(false) {}
    bool setup(int, int) { return false; }
    bool isReady = false;
    Rotation getRotation() { return NONE; }
};
#endif
