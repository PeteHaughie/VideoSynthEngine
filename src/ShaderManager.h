#pragma once

#include "ofMain.h"
#include "ShaderParam.h"
#include <string>
#include <vector>
#include <map>

class MidiController;

enum class PassInputSource { PREV_PASS, SOURCE_INPUT, SELF_FEEDBACK };

struct PassInput
{
    std::string samplerName = "src";
    PassInputSource source = PassInputSource::PREV_PASS;
};

struct ShaderPass
{
    ofShader shader;
    std::string name;
    std::vector<ShaderParam> params;
    std::vector<PassInput> inputs;
    ofFbo feedback[2];
    int feedbackIndex = 0;
};

class ShaderManager
{
public:
    ShaderManager();
    ~ShaderManager();

    void setup(int width, int height);
    void update();
    void draw(ofTexture &input, float x, float y, float w, float h);

    void clearPasses();
    int addPass(const std::string &shaderName,
                const std::string &vertPath,
                const std::string &fragPath,
                const std::vector<ShaderParam> &params = {},
                const std::vector<PassInput> &inputs = {});

    void setParam(int passIndex, const std::string &name, float value);
    float getParam(int passIndex, const std::string &name) const;
    const std::vector<ShaderParam> &getParams(int passIndex) const;
    int getPassCount() const { return passes.size(); }

    void bindMidiParam(const std::string &paramName, int midiCC);
    void applyMidi(MidiController &midi);

    void setDebug(bool d) { debug = d; }
    static std::string getVersionHeader();

private:
    std::string loadSource(const std::string &path) const;

    std::vector<ShaderPass> passes;
    std::map<std::string, int> paramToMidiCC;
    std::map<int, std::pair<int, std::string>> midiToParam;

    int fboWidth;
    int fboHeight;
    bool debug;
};
