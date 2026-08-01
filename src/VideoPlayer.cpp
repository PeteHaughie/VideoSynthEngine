#include "VideoPlayer.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

VideoPlayer::VideoPlayer()
    : loaded(false)
    , initialized(false)
{
}

VideoPlayer::~VideoPlayer()
{
    close();
}

void VideoPlayer::setup()
{
    initialized = true;
}

void VideoPlayer::update()
{
    player.update();
}

void VideoPlayer::draw(float x, float y, float w, float h)
{
    if (player.isInitialized())
        player.draw(x, y, w, h);
}

void VideoPlayer::close()
{
    if (player.isLoaded()) player.close();
    loaded = false;
}

bool VideoPlayer::load(const std::string &filePath)
{
    namespace fs = std::filesystem;

    int retries = 4;
    int delayMs = 250;

    for (int i = 0; i < retries; ++i)
    {
        std::ifstream testFile(filePath);
        if (!testFile.is_open())
        {
            ofLogError("VideoPlayer") << "Failed to open: " << filePath;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            continue;
        }
        if (fs::exists(filePath) && testFile.good())
        {
            testFile.close();
            if (player.isLoaded()) player.close();
            loaded = player.load(filePath);
            player.setSpeed(1.0f);
            player.play();
            if (loaded) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    ofLogError("VideoPlayer") << "Failed to load after retries: " << filePath;
    return false;
}

bool VideoPlayer::isLoaded() const
{
    return player.isLoaded();
}

bool VideoPlayer::isInitialized() const
{
    return player.isInitialized();
}

bool VideoPlayer::isPlaying() const
{
    return player.isPlaying();
}

void VideoPlayer::play()
{
    player.play();
}

void VideoPlayer::stop()
{
    player.stop();
}

void VideoPlayer::setPaused(bool paused)
{
    player.setPaused(paused);
}

bool VideoPlayer::isPaused() const
{
    return player.isPaused();
}

void VideoPlayer::setSpeed(float speed)
{
    player.setSpeed(speed);
}

float VideoPlayer::getSpeed() const
{
    return player.getSpeed();
}

void VideoPlayer::setLoopState(ofLoopType state)
{
    player.setLoopState(state);
}

ofLoopType VideoPlayer::getLoopState() const
{
    return player.getLoopState();
}

ofTexture &VideoPlayer::getTexture()
{
    return player.getTexture();
}
