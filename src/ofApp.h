#pragma once

#include "ofMain.h"
#include "ofxModulation.h"
#include "MidiController.h"
#include "ShaderManager.h"
#include "ShaderPresets.h"
#include "VideoInputManager.h"
#include "VideoPlayer.h"
#include "DriveWatcher.h"
#include "PlatformConfig.h"
#ifdef VSE_HAS_GPIO
#include "ButtonController.h"
#include "RotaryEncoderController.h"
#endif

class ofApp : public ofBaseApp
{
public:
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;
    void windowResized(int w, int h) override;

private:
    enum SourceMode { SOURCE_CAMERA, SOURCE_PLAYBACK, SOURCE_TEST_PATTERN };

    ofTexture &getSourceTexture();
    void setSourceMode(SourceMode mode);
    void nextPreset();
    void prevPreset();
    void loadPreset(int index);
    void refreshFiles();
    void increaseIndex();
    void decreaseIndex();

    ofTrueTypeFont font;

    MidiController midi;
    ShaderManager shaders;
    ShaderPresets presets;

    ModulationEngine modulation;
    void applyLfoModulation();
    int resolveLfoPass(const Lfo::Target &target) const;

    VideoInputManager videoInput;
    VideoPlayer videoPlayer;
    DriveWatcher driveWatcher;

    ofShader testPatternShader;
    ofFbo testPatternFbo;

    SourceMode sourceMode;
    int currentPreset;
    bool debug;
    bool drawing;

    std::vector<std::string> files;
    std::string currentFilePath;
    size_t videoIndex;
    size_t lastLoadedVideoIndex;
    bool fileProblem;

    std::vector<int> sliderCCs;
    std::vector<int> knobCCs;
    int loopState;

    ofLoopType getLoopType(int state);

#ifdef VSE_HAS_GPIO
    std::vector<std::unique_ptr<ButtonController>> buttons;
    std::vector<std::unique_ptr<RotaryEncoderController>> encoders;
#endif
};
