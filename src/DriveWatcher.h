#pragma once
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <set>

class DriveWatcher {
public:
    using Callback = std::function<void(const std::string&, bool)>;

    DriveWatcher();
    ~DriveWatcher();

    void setCallback(Callback cb);
    void start();
    void stop();
    void simulateEvent(const std::string& path, bool appeared);
#ifdef __linux__
    std::string mediaPath = "/media";
#else
    std::string mediaPath = "/Volumes";
#endif

private:
    void watchLoop();
    std::set<std::string> getMountedDrives();

    Callback callback;
    std::thread watcherThread;
    std::atomic<bool> running;
    std::set<std::string> lastDrives;
};
