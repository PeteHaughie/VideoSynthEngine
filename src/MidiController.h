#pragma once

#include "ofxMidi.h"
#include <mutex>
#include <string>
#include <vector>
#include <map>

class MidiController : public ofxMidiListener
{
public:
    MidiController();
    ~MidiController();

    void setup();
    void update();
    void exit();

    void bindContinuous(int cc, const std::string &name, float threshold = 0.04f);
    void bindTrigger(int cc, const std::string &name);

    float get(const std::string &name) const;
    bool active(const std::string &name) const;
    float getByCC(int cc) const;
    bool activeByCC(int cc) const;
    bool fired(const std::string &name);

    void cyclePort();
    int portCount();

    void newMidiMessage(ofxMidiMessage &msg) override;

private:
    struct ContinuousParam
    {
        int cc;
        float value;
        float previousValue;
        float threshold;
        bool isActive;
    };

    struct TriggerParam
    {
        int cc;
        bool wasFired = false;
        bool wasPressed = false;
    };

    ofxMidiIn midiIn;
    std::vector<ofxMidiMessage> messageQueue;
    std::mutex messageQueueMutex;
    std::map<std::string, ContinuousParam> continuousParams;
    std::map<std::string, TriggerParam> triggerParams;
    int currentPort;
    int prevPort;
    bool portDirty;
};
