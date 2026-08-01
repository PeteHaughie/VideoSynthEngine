#pragma once

#include "ofMain.h"
#include <string>

class VideoPlayer
{
public:
    VideoPlayer();
    ~VideoPlayer();

    void setup();
    void update();
    void draw(float x, float y, float w, float h);
    void close();

    bool load(const std::string &filePath);
    bool isLoaded() const;
    bool isInitialized() const;
    bool isPlaying() const;

    void play();
    void stop();
    void setPaused(bool paused);
    bool isPaused() const;

    void setSpeed(float speed);
    float getSpeed() const;

    void setLoopState(ofLoopType state);
    ofLoopType getLoopState() const;

    ofTexture &getTexture();

private:
    ofVideoPlayer player;
    bool loaded;
    bool initialized;
};
