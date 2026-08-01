#pragma once

#include "ofMain.h"

class FrameBuffer
{
public:
    FrameBuffer(long unsigned int maxFrames) : maxFrames(maxFrames) {}

    void addFrame(const ofPixels &pixels)
    {
        if (frames.size() >= maxFrames)
        {
            frames.erase(frames.begin());
        }
        frames.push_back(pixels);
    }

    void removeFirstFrame()
    {
        if (!frames.empty())
        {
            frames.erase(frames.begin());
        }
    }

    void clear()
    {
        frames.clear();
    }

    const ofPixels &getFrame(int index) const
    {
        return frames[index];
    }

    const std::vector<ofPixels> &getAllFrames() const { return frames; }

    void setFrames(const std::vector<ofPixels> &newFrames)
    {
        frames.clear();
        for (const auto &px : newFrames)
        {
            ofPixels copy;
            copy = px;
            frames.push_back(copy);
        }
        while (frames.size() > maxFrames)
        {
            frames.erase(frames.begin());
        }
    }

    void resize(int newSize)
    {
        maxFrames = newSize;
        while (frames.size() > maxFrames)
        {
            frames.erase(frames.begin());
        }
    }

    int size() const
    {
        return frames.size();
    }

private:
    int maxFrames;
    std::vector<ofPixels> frames;
};
