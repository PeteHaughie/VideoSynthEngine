#include "MidiController.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <cmath>

#define MIDI_MAGIC 63.50f
#define MIDI_RANGE 63.50f

static float normalizeMidiValue(int value)
{
    return (value - MIDI_MAGIC) / MIDI_RANGE;
}

MidiController::MidiController()
    :
#ifdef __linux__
      currentPort(1)
#else
      currentPort(0)
#endif
    , prevPort(-1)
    , portDirty(false)
{
}

MidiController::~MidiController()
{
    exit();
}

void MidiController::setup()
{
    midiIn.listInPorts();
    auto ports = midiIn.getInPortList();
    if (ports.empty())
    {
        ofLog() << "No MIDI input ports available";
        return;
    }
    if (currentPort >= (int)ports.size())
        currentPort = (int)ports.size() - 1;
    midiIn.openPort(currentPort);
    midiIn.ignoreTypes(false, false, false);
    midiIn.addListener(this);
    midiIn.setVerbose(true);
}

void MidiController::update()
{
    if (portDirty && currentPort != prevPort)
    {
        midiIn.closePort();
        midiIn.openPort(currentPort);
        prevPort = currentPort;
        portDirty = false;
        ofLog() << "MIDI port: " << currentPort;
    }

    std::vector<ofxMidiMessage> pendingMessages;
    {
        std::scoped_lock lock(messageQueueMutex);
        pendingMessages.swap(messageQueue);
    }

    for (const auto &msg : pendingMessages)
    {
        if (msg.status >= MIDI_SYSEX)
            continue;
        if (msg.status != MIDI_CONTROL_CHANGE)
            continue;

        for (auto &[name, param] : continuousParams)
        {
            if (msg.control == param.cc)
            {
                float normalized = normalizeMidiValue(msg.value);
                if (!param.isActive)
                {
                    float diff = std::abs(normalized - param.previousValue);
                    if (diff < param.threshold)
                    {
                        param.isActive = true;
                    }
                }
                if (param.isActive)
                {
                    param.value = normalized;
                }
                break;
            }
        }

        for (auto &[name, param] : triggerParams)
        {
            if (msg.control == param.cc)
            {
                bool isPressed = msg.value > 0;
                if (isPressed && !param.wasPressed)
                    param.wasFired = true;
                param.wasPressed = isPressed;
                break;
            }
        }
    }
}

void MidiController::exit()
{
    midiIn.closePort();
    midiIn.removeListener(this);
}

void MidiController::bindContinuous(int cc, const std::string &name, float threshold)
{
    continuousParams[name] = {cc, 0.0f, 0.0f, threshold, false};
}

void MidiController::bindTrigger(int cc, const std::string &name)
{
    triggerParams[name] = {cc};
}

float MidiController::get(const std::string &name) const
{
    auto it = continuousParams.find(name);
    if (it != continuousParams.end())
        return it->second.value;
    return 0.0f;
}

bool MidiController::active(const std::string &name) const
{
    auto it = continuousParams.find(name);
    if (it != continuousParams.end())
        return it->second.isActive;
    return false;
}

float MidiController::getByCC(int cc) const
{
    for (const auto &[name, param] : continuousParams)
    {
        if (param.cc == cc)
            return param.value;
    }
    return 0.0f;
}

bool MidiController::activeByCC(int cc) const
{
    for (const auto &[name, param] : continuousParams)
    {
        if (param.cc == cc)
            return param.isActive;
    }
    return false;
}

bool MidiController::fired(const std::string &name)
{
    auto it = triggerParams.find(name);
    if (it != triggerParams.end())
    {
        if (it->second.wasFired)
        {
            it->second.wasFired = false;
            return true;
        }
    }
    return false;
}

void MidiController::cyclePort()
{
    auto devices = midiIn.getInPortList();
    if (devices.empty())
    {
        ofLog() << "no midi devices found";
        return;
    }
    for (unsigned long i = 0; i < devices.size(); i++)
    {
        ofLog() << devices[i];
    }
    currentPort++;
    if (currentPort >= (int)devices.size())
        currentPort = 0;
    portDirty = true;
}

int MidiController::portCount()
{
    return midiIn.getInPortList().size();
}

void MidiController::newMidiMessage(ofxMidiMessage &msg)
{
    std::scoped_lock lock(messageQueueMutex);
    messageQueue.push_back(msg);
    while (messageQueue.size() > 2)
    {
        messageQueue.erase(messageQueue.begin());
    }
}
