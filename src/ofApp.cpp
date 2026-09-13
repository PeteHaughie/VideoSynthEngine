#include "ofApp.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

ofLoopType ofApp::getLoopType(int state)
{
    switch (state)
    {
    case 0: return OF_LOOP_NORMAL;
    case 1: return OF_LOOP_PALINDROME;
    default: return OF_LOOP_NORMAL;
    }
}

void ofApp::setup()
{
    ofDisableArbTex();
    ofSetFrameRate(30);
    ofSetVerticalSync(true);
    ofBackground(0);

    font.load("fonts/VCR_OSD_MONO_1.001.ttf", 12);

    // --- Shaders ---
    shaders.setup(ofGetWidth(), ofGetHeight());
    shaders.setDebug(debug);
    presets.setup();

    ofFbo::Settings s;
    s.width = ofGetWidth();
    s.height = ofGetHeight();
    s.internalformat = GL_RGBA;
    s.textureTarget = GL_TEXTURE_2D;
    testPatternFbo.allocate(s);

    std::string vertSrc = ShaderManager::getVersionHeader() + ofBufferFromFile("shaders/passthru.vert").getText();
    std::string fragSrc = ShaderManager::getVersionHeader() + ofBufferFromFile("shaders/test_pattern.frag").getText();
    testPatternShader.setupShaderFromSource(GL_VERTEX_SHADER, vertSrc);
    testPatternShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragSrc);
    testPatternShader.bindDefaults();
    testPatternShader.linkProgram();

    // --- MIDI ---
    sliderCCs = {0, 1, 2, 3, 4, 5, 6, 7};
    knobCCs  = {16, 17, 18, 19, 20, 21, 22, 23};
    for (int i = 0; i < 8; i++)
    {
        midi.bindContinuous(sliderCCs[i], "slider" + ofToString(i));
        midi.bindContinuous(knobCCs[i], "knob" + ofToString(i));
    }
    midi.bindTrigger(41, "prevPreset");
    midi.bindTrigger(42, "nextPreset");
    midi.bindTrigger(43, "debug");
    midi.bindTrigger(44, "prev");
    midi.bindTrigger(45, "next");
    midi.bindTrigger(62, "reset");
    midi.setup();

    // --- Modulation (ofxModulation) ---
    // Example: a sine LFO that modulates uHue on whichever preset exposes it.
    // CC 24 grabs depth, CC 25 grabs rate. Both start at 0 (LFO off).
    modulation.setup();
    Lfo *hueLfo = modulation.createLfo("hue");
    hueLfo->addTarget(-1, "uHue");              // -1 = resolve by name across passes
    hueLfo->oscillator().setWave(Oscillator::SINE);
    hueLfo->oscillator().setFrequency(0.2f);
    hueLfo->setScale(0.15f);
    hueLfo->setDepth(0.0f);
    hueLfo->setDepthBindable(true);
    hueLfo->setRateBindable(true);
    midi.bindContinuous(24, "hueLfoDepth", 0.02f);
    midi.bindContinuous(25, "hueLfoRate", 0.02f);

    // --- Camera ---
    videoInput.setup(ofGetWidth(), ofGetHeight());

    // --- Drive watcher ---
    driveWatcher.setCallback([this](const std::string &path, bool appeared)
    {
        if (appeared)
            refreshFiles();
    });
    driveWatcher.start();
    refreshFiles();

    // --- Video playback ---
    videoPlayer.setup();

    // --- Source ---
    sourceMode = videoInput.isInputDeviceConnected() ? SOURCE_CAMERA : SOURCE_TEST_PATTERN;
    currentPreset = 0;
    debug = true;
    drawing = false;
    videoIndex = 0;
    lastLoadedVideoIndex = std::numeric_limits<size_t>::max();
    fileProblem = false;
    loopState = 0;

    loadPreset(0);

#ifdef VSE_HAS_GPIO
    std::vector<int> buttonPins = {26, 19, 13, 6, 1};
    for (int pin : buttonPins)
    {
        auto btn = std::make_unique<ButtonController>();
        btn->setup(pin);
        buttons.push_back(std::move(btn));
    }
    std::vector<std::pair<int, int>> encoderPins = {
        {5, 0}, {16, 12}, {21, 20}
    };
    for (const auto &pins : encoderPins)
    {
        auto enc = std::make_unique<RotaryEncoderController>();
        enc->setup(pins.first, pins.second);
        encoders.push_back(std::move(enc));
    }
#endif

    ofLogNotice("ofApp") << "VideoSynthEngine ready. Keys: [ ] presets, D debug, M midi, F fullscreen";
}

void ofApp::refreshFiles()
{
    std::string mediaPath =
#ifdef __linux__
        "/media";
#else
        "/Volumes";
#endif

    std::vector<std::string> newFiles;
    try
    {
        for (const auto &entry : fs::directory_iterator(mediaPath))
        {
            std::string entryName = entry.path().filename().string();
            if (!entry.is_directory() || entryName[0] == '.' ||
                entryName.find("com.apple.") == 0)
                continue;
            try
            {
                std::string realPath = fs::canonical(entry.path()).string();
                if (realPath.find("/System/Volumes/") == 0)
                    continue;
            }
            catch (...) {}
            for (const auto &fileEntry : fs::directory_iterator(entry.path()))
            {
                if (fileEntry.path().filename().string()[0] == '.')
                    continue;
                if (fileEntry.is_regular_file() &&
                    (fileEntry.path().extension() == ".mov" || fileEntry.path().extension() == ".mp4"))
                {
                    newFiles.push_back(fileEntry.path().string());
                }
            }
        }
    }
    catch (...) {}

    if (newFiles != files)
    {
        files = std::move(newFiles);
        lastLoadedVideoIndex = std::numeric_limits<size_t>::max();
        if (!files.empty())
            fileProblem = false;
    }
}

void ofApp::increaseIndex()
{
    if (!files.empty() && videoIndex < files.size() - 1)
        videoIndex++;
}

void ofApp::decreaseIndex()
{
    if (videoIndex > 0)
        videoIndex--;
}

void ofApp::setSourceMode(SourceMode mode)
{
    sourceMode = mode;
}

ofTexture &ofApp::getSourceTexture()
{
    if (sourceMode == SOURCE_CAMERA && videoInput.getInput() && videoInput.getInput()->isInitialized())
        return videoInput.getInput()->getTexture();

    if (sourceMode == SOURCE_PLAYBACK && videoPlayer.isInitialized())
        return videoPlayer.getTexture();

    testPatternFbo.begin();
    ofClear(0, 0, 0, 0);
    testPatternShader.begin();
    testPatternShader.setUniform1f("uFrameCount", (float)ofGetFrameNum());

    ofMesh quad;
    quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    float fw = (float)testPatternFbo.getWidth();
    float fh = (float)testPatternFbo.getHeight();
    quad.addVertex(glm::vec3(0, 0, 0));
    quad.addVertex(glm::vec3(fw, 0, 0));
    quad.addVertex(glm::vec3(fw, fh, 0));
    quad.addVertex(glm::vec3(0, fh, 0));
    quad.addTexCoord(glm::vec2(0, 0));
    quad.addTexCoord(glm::vec2(1, 0));
    quad.addTexCoord(glm::vec2(1, 1));
    quad.addTexCoord(glm::vec2(0, 1));
    quad.draw();

    testPatternShader.end();
    testPatternFbo.end();
    return testPatternFbo.getTexture();
}

void ofApp::loadPreset(int index)
{
    if (index < 0 || index >= presets.size()) return;
    currentPreset = index;
    presets.loadInto(shaders, index, sliderCCs, knobCCs);
    ofLogNotice("ofApp") << "Preset: " << presets.getName(index);
}

void ofApp::nextPreset()
{
    loadPreset((currentPreset + 1) % presets.size());
}

void ofApp::prevPreset()
{
    loadPreset((currentPreset - 1 + presets.size()) % presets.size());
}

int ofApp::resolveLfoPass(const Lfo::Target &target) const
{
    if (target.pass >= 0 && target.pass < shaders.getPassCount())
    {
        for (const auto &p : shaders.getParams(target.pass))
            if (p.name == target.paramName)
                return target.pass;
    }
    for (int i = 0; i < shaders.getPassCount(); i++)
    {
        for (const auto &p : shaders.getParams(i))
            if (p.name == target.paramName)
                return i;
    }
    return -1;
}

void ofApp::applyLfoModulation()
{
    // Accumulate per (pass, param) contributions, then apply additively on top
    // of the fresh base value. Reading the base here (after shaders.applyMidi)
    // avoids frame-to-frame compounding.
    std::map<std::pair<int, std::string>, float> contributions;
    for (const auto &lfo : modulation.getLfos())
    {
        const float v = lfo->value();
        if (std::fabs(v) < 0.0001f) continue;
        for (const auto &target : lfo->getTargets())
        {
            const int pass = resolveLfoPass(target);
            if (pass < 0) continue;
            contributions[{pass, target.paramName}] += v;
        }
    }
    for (const auto &[key, contrib] : contributions)
    {
        const float base = shaders.getParam(key.first, key.second);
        shaders.setParam(key.first, key.second, base + contrib);
    }
}

void ofApp::update()
{
    midi.update();

    if (midi.fired("nextPreset")) nextPreset();
    if (midi.fired("prevPreset")) prevPreset();
    if (midi.fired("next"))       nextPreset();
    if (midi.fired("prev"))       prevPreset();
    if (midi.fired("debug"))      { debug = !debug; shaders.setDebug(debug); }
    if (midi.fired("reset"))      loadPreset(0);

    shaders.applyMidi(midi);

    // LFO depth/rate from MIDI (threshold-grabbed, mapped to useful ranges)
    if (Lfo *lfo = modulation.getLfo("hue"))
    {
        if (midi.active("hueLfoDepth"))
            lfo->setDepth(ofMap(midi.get("hueLfoDepth"), -1.0f, 1.0f, 0.0f, 1.0f, true));
        if (midi.active("hueLfoRate"))
            lfo->oscillator().setFrequency(ofMap(midi.get("hueLfoRate"), -1.0f, 1.0f, 0.02f, 2.0f, true));
    }

    modulation.tick(ofGetLastFrameTime());
    applyLfoModulation();

    videoInput.update();
    videoPlayer.update();
    shaders.update();

    if (!files.empty())
    {
        if (midi.active("slider1"))
        {
            float mapped = ofMap(midi.get("slider1"), -1.0f, 1.0f, 0.0f, (float)(files.size() - 1), true);
            videoIndex = (size_t)mapped;
        }
        if (midi.fired("prev")) decreaseIndex();
        if (midi.fired("next")) increaseIndex();

        if (videoIndex != lastLoadedVideoIndex && !fileProblem)
        {
            currentFilePath = files[videoIndex];
            sourceMode = SOURCE_PLAYBACK;
            videoPlayer.load(currentFilePath);
            lastLoadedVideoIndex = videoIndex;
        }
    }

    if (sourceMode == SOURCE_PLAYBACK && (!videoPlayer.isLoaded() || !videoPlayer.isInitialized()))
    {
        sourceMode = videoInput.isInputDeviceConnected() ? SOURCE_CAMERA : SOURCE_TEST_PATTERN;
    }

#ifdef VSE_HAS_GPIO
    for (auto &button : buttons)
    {
        if (button->wasPressed())
        {
            int pin = button->getPinNum();
            if (pin == 26) nextPreset();
            else if (pin == 19) prevPreset();
            else if (pin == 13) { debug = !debug; shaders.setDebug(debug); }
        }
    }
    for (auto &enc : encoders)
    {
        auto rot = enc->getRotation();
        if (rot == RotaryEncoderController::RIGHT) nextPreset();
        else if (rot == RotaryEncoderController::LEFT) prevPreset();
    }
#endif
}

void ofApp::draw()
{
    if (drawing) return;
    drawing = true;

    ofSetColor(255);
    shaders.draw(getSourceTexture(), 0, 0, ofGetWidth(), ofGetHeight());

    if (debug)
    {
        ofSetColor(0, 0, 0, 180);
        ofDrawRectangle(0, 0, ofGetWidth(), 40);

        ofSetColor(0, 255, 0);
        std::string presetName = (currentPreset < presets.size()) ? presets.getName(currentPreset) : "NONE";
        std::string srcName = (sourceMode == SOURCE_CAMERA) ? "camera" :
                              (sourceMode == SOURCE_PLAYBACK) ? "playback" : "test pattern";
        int midiPorts = midi.portCount();

        std::string info = "VSE | " + presetName
                         + " | FPS: " + ofToString((int)ofGetFrameRate())
                         + " | SRC: " + srcName
                         + " | MIDI: " + ofToString(midiPorts) + " port(s)";
        for (const auto &lfo : modulation.getLfos())
        {
            if (std::fabs(lfo->getDepth()) < 0.0001f) continue;
            info += " | LFO:" + lfo->getName() + " d=" + ofToString(lfo->getDepth(), 2)
                  + " @=" + ofToString(lfo->getRate(), 2)
                  + " " + std::string(Oscillator::waveName(lfo->oscillator().getWave()));
        }
        font.drawString(info, 10, 16);

        std::string help = "[ ] presets  D debug  M midi port  F fullscreen  L lfo wave";
        font.drawString(help, 10, 36);
    }

    drawing = false;
}

void ofApp::exit()
{
    midi.exit();
    videoInput.close();
    videoPlayer.close();
    driveWatcher.stop();
}

void ofApp::keyPressed(int key)
{
    switch (key)
    {
    case ']': case 'n': nextPreset(); break;
    case '[': case 'p': prevPreset(); break;
    case 'd': case 'D': debug = !debug; shaders.setDebug(debug); break;
    case 'm': case 'M': midi.cyclePort(); break;
    case 'f': case 'F': ofToggleFullscreen(); break;
    case 'r': case 'R': loadPreset(0); break;
    case 'l': case 'L':
        if (auto *lfo = modulation.getLfo("hue"))
        {
            Oscillator::Wave w = (Oscillator::Wave)((lfo->oscillator().getWave() + 1) % Oscillator::NUM_WAVES);
            lfo->oscillator().setWave(w);
            ofLogNotice("ofApp") << "LFO hue wave: " << Oscillator::waveName(w);
        }
        break;
    case '0': sourceMode = SOURCE_TEST_PATTERN; break;
    case '1': setSourceMode(videoInput.isInputDeviceConnected() ? SOURCE_CAMERA : SOURCE_TEST_PATTERN); break;
    case '2': if (!files.empty()) setSourceMode(SOURCE_PLAYBACK); break;
    case '9': debug = !debug; shaders.setDebug(debug); break;
    default:
        if (key >= '3' && key <= '7')
            loadPreset(key - '3');
        break;
    }
}

void ofApp::windowResized(int w, int h)
{
    shaders.setup(w, h);
    testPatternFbo.allocate(w, h, GL_RGBA);
}
