#include "DriveWatcher.h"
#include <thread>
#include "ofLog.h"
#include <filesystem>
#include <chrono>
#include <algorithm>

DriveWatcher::DriveWatcher() : running(false) {}

DriveWatcher::~DriveWatcher()
{
  stop();
}

void DriveWatcher::setCallback(Callback cb)
{
  callback = cb;
}

void DriveWatcher::start()
{
  running = true;
  lastDrives = getMountedDrives();
  watcherThread = std::thread(&DriveWatcher::watchLoop, this);
}

void DriveWatcher::stop()
{
  running = false;
  if (watcherThread.joinable())
  {
    watcherThread.join();
  }
}

void DriveWatcher::simulateEvent(const std::string &path, bool appeared)
{
  if (callback)
    callback(path, appeared);
}

std::set<std::string> DriveWatcher::getMountedDrives()
{
  namespace fs = std::filesystem;
  std::set<std::string> drives;
  try
  {
    for (const auto &entry : fs::directory_iterator(mediaPath))
    {
      std::string entryName = entry.path().filename().string();
      if (entry.is_directory() && !entryName.empty() && entryName[0] != '.')
      {
        drives.insert(entry.path().string());
      }
    }
  }
  catch (...)
  {
  }
  return drives;
}

void DriveWatcher::watchLoop()
{
  while (running)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto currentDrives = getMountedDrives();

    for (const auto &drive : currentDrives)
    {
      if (lastDrives.find(drive) == lastDrives.end())
      {
        if (callback)
          callback(drive, true);
      }
    }
    for (const auto &drive : lastDrives)
    {
      if (currentDrives.find(drive) == currentDrives.end())
      {
        if (callback)
          callback(drive, false);
      }
    }
    lastDrives = currentDrives;
  }
}
