#include "VideoInputManager.h"

VideoInputManager::VideoInputManager() {}

#ifdef __linux__
#include <filesystem>
namespace fs = std::filesystem;

size_t getVideoDeviceCount()
{
    size_t count = 0;
    for (const auto &entry : fs::directory_iterator("/dev"))
    {
        std::string path = entry.path().string();
        if (path.find("/dev/video") == 0 && isdigit(path.back()))
        {
            count++;
        }
    }
    return count;
}
#endif

void VideoInputManager::setup(int width, int height) {
    input = std::make_shared<ofVideoGrabber>();
    auto devices = input->listDevices();
    suitableDeviceIDs = getSuitableDeviceIDs(devices);
    currentDeviceIndex = 0;
    if (!suitableDeviceIDs.empty()) {
        currentDeviceID = suitableDeviceIDs[currentDeviceIndex];
        input->setDeviceID(currentDeviceID);
        input->setDesiredFrameRate(30);
        input->setup(width, height);
        inputDeviceConnected = input->isInitialized();
        if (!inputDeviceConnected)
            ofLogError() << "Capture device present but failed to initialise";
    } else {
        inputDeviceConnected = false;
        ofLogError() << "No suitable capture device found!";
    }
#ifdef __linux__
    // Baseline the cheap /dev/video* count so the first poll does not falsely
    // report a change.
    lastVideoDeviceCount = getVideoDeviceCount();
#endif
}

void VideoInputManager::refreshInputDevices() {
    ofLogNotice() << "Refreshing video input devices...";
    input.reset();
    input = std::make_shared<ofVideoGrabber>();
    auto devices = input->listDevices();
    suitableDeviceIDs = getSuitableDeviceIDs(devices);
    if (!suitableDeviceIDs.empty()) {
        if (currentDeviceIndex >= suitableDeviceIDs.size()) {
            currentDeviceIndex = 0;
        }
        currentDeviceID = suitableDeviceIDs[currentDeviceIndex];
        input->setDeviceID(currentDeviceID);
        input->setDesiredFrameRate(30);
        input->setup(ofGetWidth(), ofGetHeight());
        // The node can appear before the device is ready to stream; only report
        // connected once the grabber actually initialised, so the poll retries.
        inputDeviceConnected = input->isInitialized();
        if (inputDeviceConnected)
            ofLogNotice() << "Auto-selected video device: " << currentDeviceID;
        else
            ofLogError() << "Capture device " << currentDeviceID << " failed to initialise";
    } else {
        inputDeviceConnected = false;
        ofLogError() << "No suitable capture device found after refresh!";
    }
}

void VideoInputManager::incrementDeviceID() {
    refreshInputDevices();
    auto devices = input->listDevices();
    suitableDeviceIDs = getSuitableDeviceIDs(devices);
    if (!suitableDeviceIDs.empty()) {
        currentDeviceIndex = (currentDeviceIndex + 1) % suitableDeviceIDs.size();
        currentDeviceID = suitableDeviceIDs[currentDeviceIndex];
        ofLog() << "Device ID set to: " << currentDeviceID;
        input->close();
        input->setDeviceID(currentDeviceID);
        input->setup(ofGetWidth(), ofGetHeight());
    } else {
        ofLogError() << "No suitable capture device found!";
    }
}

std::vector<int> VideoInputManager::getSuitableDeviceIDs(const std::vector<ofVideoDevice>& devices) {
    std::vector<int> ids;
    for (const auto& d : devices) {
        if (d.deviceName.find("bcm2835") == std::string::npos) {
            ids.push_back(d.id);
        }
    }
    return ids;
}

std::shared_ptr<ofVideoGrabber> VideoInputManager::getInput() {
    return input;
}

int VideoInputManager::getCurrentDeviceID() const {
    return currentDeviceID;
}

int VideoInputManager::getCurrentDeviceIndex() const {
    return currentDeviceIndex;
}

bool VideoInputManager::isInputDeviceConnected() const {
    return inputDeviceConnected;
}

void VideoInputManager::update() {
    if (input && input->isInitialized()) {
        input->update();
    }

    uint64_t now = ofGetElapsedTimeMillis();
    if (now - lastInputPollTime <= inputPollInterval) {
        return;
    }
    lastInputPollTime = now;

    // Re-scan for devices. On the Pi the fixed bcm2835 codec/ISP nodes keep the
    // /dev/video* count steady, so a change means a USB capture card was added
    // or removed. Use that cheap check as the trigger and only fall back to the
    // (udev-probing, log-heavy) listDevices() when something changed, or while a
    // device we tried to open has not initialised yet. Polling unconditionally
    // here -- not only when a device is already open -- is what makes a card
    // hotplugged after a camera-less boot get picked up.
    bool rescan = true;
#ifdef __linux__
    size_t currentCount = getVideoDeviceCount();
    rescan = (currentCount != lastVideoDeviceCount) || pendingOpen;
    lastVideoDeviceCount = currentCount;
#endif
    if (!rescan) {
        return;
    }

    // Probe with a throwaway grabber. ofGstVideoGrabber caches its device list
    // (camData.bInited), so calling listDevices() on the long-lived grabber
    // would keep returning the boot-time list and never see a hotplugged card.
    ofVideoGrabber probe;
    auto currentIDs = getSuitableDeviceIDs(probe.listDevices());

    if (inputDeviceConnected) {
        // Already latched: only react if our device disappeared.
        if (std::find(currentIDs.begin(), currentIDs.end(), currentDeviceID) == currentIDs.end()) {
            ofLogError() << "Current video input device disconnected!";
            input->close();
            inputDeviceConnected = false;
            suitableDeviceIDs.clear();
        }
        pendingOpen = false;
    } else if (!currentIDs.empty()) {
        // A card appeared (or is present but was never opened): latch it.
        ofLogNotice() << "Capture device available; opening";
        refreshInputDevices();
        // If it did not initialise, retry on the next tick.
        pendingOpen = !inputDeviceConnected;
    } else {
        pendingOpen = false;
    }

    lastSuitableDeviceIDs = currentIDs;
}

void VideoInputManager::close() {
    if (input && input->isInitialized()) {
        input->close();
        inputDeviceConnected = false;
        ofLogNotice() << "Video input device closed.";
    }
}
