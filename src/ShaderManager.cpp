#include "ShaderManager.h"
#include "MidiController.h"
#include "ofLog.h"
#include "ofUtils.h"

ShaderManager::ShaderManager()
    : fboWidth(0)
    , fboHeight(0)
    , debug(false)
{
}

ShaderManager::~ShaderManager() {}

static void allocateFbo(ofFbo &fb, int w, int h)
{
    ofFbo::Settings s;
    s.width = w;
    s.height = h;
    s.internalformat = GL_RGBA;
    s.numColorbuffers = 1;
    s.useDepth = false;
    s.useStencil = false;
    s.textureTarget = GL_TEXTURE_2D;
    s.minFilter = GL_LINEAR;
    s.maxFilter = GL_LINEAR;
    s.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
    s.wrapModeVertical = GL_CLAMP_TO_EDGE;
    fb.allocate(s);
}

void ShaderManager::setup(int width, int height)
{
    fboWidth = width;
    fboHeight = height;
    allocateFbo(fboA, width, height);
    allocateFbo(fboB, width, height);
}

void ShaderManager::update()
{
    int w = ofGetWidth();
    int h = ofGetHeight();
    if (w != fboWidth || h != fboHeight)
    {
        fboWidth = w;
        fboHeight = h;
        allocateFbo(fboA, w, h);
        allocateFbo(fboB, w, h);
    }
}

std::string ShaderManager::getVersionHeader()
{
#ifdef TARGET_RASPBERRY_PI
    return "#version 300 es\n#define GLES 1\nprecision highp float;\n";
#else
    return "#version 150\n";
#endif
}

std::string ShaderManager::loadSource(const std::string &path) const
{
    ofBuffer buf = ofBufferFromFile(path);
    if (!buf.size())
    {
        ofLogError("ShaderManager") << "Failed to load: " << path;
        return "";
    }

    std::string src = buf.getText();

    std::string versionLine;
    for (char c : src)
    {
        versionLine += c;
        if (c == '\n') break;
    }

    std::string body;
    if (versionLine.find("#version") != std::string::npos)
        body = src.substr(versionLine.size());
    else
        body = src;

    return getVersionHeader() + body;
}

void ShaderManager::clearPasses()
{
    passes.clear();
}

int ShaderManager::addPass(const std::string &shaderName,
                            const std::string &vertPath,
                            const std::string &fragPath,
                            const std::vector<ShaderParam> &params)
{
    ShaderPass pass;
    pass.name = shaderName;
    pass.params = params;

    std::string vertSrc = loadSource(vertPath);
    std::string fragSrc = loadSource(fragPath);

    if (vertSrc.empty() || fragSrc.empty())
    {
        ofLogError("ShaderManager") << "Failed to load shader source: " << shaderName;
        return -1;
    }

    if (!pass.shader.setupShaderFromSource(GL_VERTEX_SHADER, vertSrc))
    {
        ofLogError("ShaderManager") << "Vertex shader compilation failed: " << shaderName;
        return -1;
    }

    if (!pass.shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragSrc))
    {
        ofLogError("ShaderManager") << "Fragment shader compilation failed: " << shaderName;
        return -1;
    }

    if (!pass.shader.bindDefaults())
    {
        ofLogError("ShaderManager") << "Shader bindDefaults failed: " << shaderName;
        return -1;
    }

    if (!pass.shader.linkProgram())
    {
        ofLogError("ShaderManager") << "Shader link failed: " << shaderName;
        return -1;
    }

    for (auto &p : pass.params)
        p.value = p.defaultValue;

    passes.push_back(std::move(pass));
    ofLogNotice("ShaderManager") << "Added pass: " << shaderName;

    return (int)passes.size() - 1;
}

void ShaderManager::setParam(int passIndex, const std::string &name, float value)
{
    if (passIndex < 0 || passIndex >= (int)passes.size()) return;
    for (auto &p : passes[passIndex].params)
    {
        if (p.name == name)
        {
            p.value = ofClamp(value, p.min, p.max);
            return;
        }
    }
}

float ShaderManager::getParam(int passIndex, const std::string &name) const
{
    if (passIndex < 0 || passIndex >= (int)passes.size()) return 0.0f;
    for (const auto &p : passes[passIndex].params)
    {
        if (p.name == name) return p.value;
    }
    return 0.0f;
}

const std::vector<ShaderParam> &ShaderManager::getParams(int passIndex) const
{
    static std::vector<ShaderParam> empty;
    if (passIndex < 0 || passIndex >= (int)passes.size()) return empty;
    return passes[passIndex].params;
}

void ShaderManager::bindMidiParam(const std::string &paramName, int midiCC)
{
    paramToMidiCC[paramName] = midiCC;
    for (size_t i = 0; i < passes.size(); i++)
    {
        for (const auto &p : passes[i].params)
        {
            if (p.name == paramName)
            {
                midiToParam[midiCC] = {(int)i, paramName};
                return;
            }
        }
    }
}

void ShaderManager::applyMidi(MidiController &midi)
{
    for (auto &[cc, mapping] : midiToParam)
    {
        int passIdx = mapping.first;
        const std::string &name = mapping.second;

        if (!midi.activeByCC(cc))
            continue;

        float midiVal = midi.getByCC(cc);
        if (passIdx >= 0 && passIdx < (int)passes.size())
        {
            for (auto &p : passes[passIdx].params)
            {
                if (p.name == name)
                {
                    p.value = ofMap(midiVal, -1.0f, 1.0f, p.min, p.max, true);
                    break;
                }
            }
        }
    }
}

void ShaderManager::draw(ofTexture &input, float x, float y, float w, float h)
{
    if (passes.empty()) return;
    if (!input.isAllocated()) return;

    if (fboA.getWidth() != (size_t)fboWidth || fboA.getHeight() != (size_t)fboHeight)
    {
        allocateFbo(fboA, fboWidth, fboHeight);
        allocateFbo(fboB, fboWidth, fboHeight);
    }

    ofFbo *prevFbo = nullptr;
    ofFbo *curFbo = &fboA;

    for (size_t i = 0; i < passes.size(); i++)
    {
        ofShader &shader = passes[i].shader;
        const ofTexture &src = (i == 0)
            ? input
            : prevFbo->getTexture();

        curFbo->begin();
        ofClear(0, 0, 0, 0);

        shader.begin();

        for (const auto &p : passes[i].params)
            shader.setUniform1f(p.name, p.value);

        shader.setUniform1f("uFrameCount", (float)ofGetFrameNum());
        shader.setUniform1i("iFrame", ofGetFrameNum());
        shader.setUniform1f("iTime", (float)ofGetFrameNum() / 60.0f);
        shader.setUniform3f("iResolution", (float)fboWidth, (float)fboHeight, 0.0f);
        shader.setUniform2f("uSourceSize", (float)src.getWidth(), (float)src.getHeight());
        shader.setUniform2f("uOutputSize", (float)fboWidth, (float)fboHeight);

        src.draw(0, 0, curFbo->getWidth(), curFbo->getHeight());

        shader.end();
        curFbo->end();

        prevFbo = curFbo;
        curFbo = (curFbo == &fboA) ? &fboB : &fboA;
    }

    prevFbo->draw(x, y, w, h);

    if (debug)
    {
        ofSetColor(ofColor::white);
        float debugY = (float)fboHeight - 20;

        std::string info = "Chain: ";
        for (size_t i = 0; i < passes.size(); i++)
        {
            info += passes[i].name;
            if (i < passes.size() - 1) info += " \xe2\x86\x92 ";
        }
        ofDrawBitmapString(info, 10, debugY);
        debugY -= 16;

        std::string params;
        for (size_t i = 0; i < passes.size(); i++)
        {
            if (passes.size() > 1)
                params += "[" + passes[i].name + "] ";
            const auto &p = passes[i].params;
            for (size_t j = 0; j < p.size(); j++)
            {
                params += p[j].name + "=" + ofToString(p[j].value, 2);
                if (j < p.size() - 1) params += " ";
            }
            if (i < passes.size() - 1 && !p.empty())
                params += " | ";
        }
        if (!params.empty())
        {
            ofDrawBitmapString(params, 10, debugY);
        }
    }
}
