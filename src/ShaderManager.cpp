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
    fb.begin();
    ofClear(0, 0, 0, 0);
    fb.end();
}

std::string ShaderManager::getVersionHeader()
{
#ifdef TARGET_RASPBERRY_PI
    return "#version 300 es\n#define GLES 1\nprecision highp float;\n";
#else
    return "#version 150\n";
#endif
}

static std::string loadCommonHeader()
{
    static std::string cached;
    if (!cached.empty()) return cached;
    ofBuffer buf = ofBufferFromFile("shaders/common.glsl");
    if (buf.size()) cached = buf.getText();
    return cached;
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

    return getVersionHeader() + loadCommonHeader() + body;
}

void ShaderManager::setup(int width, int height)
{
    fboWidth = width;
    fboHeight = height;
    for (auto &pass : passes)
    {
        allocateFbo(pass.feedback[0], width, height);
        allocateFbo(pass.feedback[1], width, height);
    }
}

void ShaderManager::update()
{
    int w = ofGetWidth();
    int h = ofGetHeight();
    if (w != fboWidth || h != fboHeight)
    {
        fboWidth = w;
        fboHeight = h;
        for (auto &pass : passes)
        {
            allocateFbo(pass.feedback[0], w, h);
            allocateFbo(pass.feedback[1], w, h);
        }
    }
}

void ShaderManager::clearPasses()
{
    passes.clear();
}

int ShaderManager::addPass(const std::string &shaderName,
                            const std::string &vertPath,
                            const std::string &fragPath,
                            const std::vector<ShaderParam> &params,
                            const std::vector<PassInput> &inputs)
{
    ShaderPass pass;
    pass.name = shaderName;
    pass.params = params;
    pass.inputs = inputs;
    if (pass.inputs.empty())
        pass.inputs.push_back({"src", PassInputSource::PREV_PASS});

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

    if (passes.front().feedback[0].getWidth() != (size_t)fboWidth ||
        passes.front().feedback[0].getHeight() != (size_t)fboHeight)
    {
        for (auto &pass : passes)
        {
            allocateFbo(pass.feedback[0], fboWidth, fboHeight);
            allocateFbo(pass.feedback[1], fboWidth, fboHeight);
        }
    }

    for (size_t i = 0; i < passes.size(); i++)
    {
        ShaderPass &pass = passes[i];
        ofShader &shader = pass.shader;
        ShaderPass *prevPass = (i > 0) ? &passes[i - 1] : nullptr;
        ofFbo &target = pass.feedback[pass.feedbackIndex];

        target.begin();
        ofClear(0, 0, 0, 0);

        shader.begin();

        for (const auto &p : pass.params)
            shader.setUniform1f(p.name, p.value);

        shader.setUniform1f("uFrameCount", (float)ofGetFrameNum());
        shader.setUniform1i("iFrame", ofGetFrameNum());
        shader.setUniform1f("iTime", (float)ofGetFrameNum() / 60.0f);
        shader.setUniform3f("iResolution", (float)fboWidth, (float)fboHeight, 0.0f);
        shader.setUniform2f("uOutputSize", (float)fboWidth, (float)fboHeight);

        const ofTexture *primarySrc = nullptr;
        int texUnit = 0;
        for (const auto &in : pass.inputs)
        {
            const ofTexture *src = nullptr;
            switch (in.source)
            {
            case PassInputSource::PREV_PASS:
                src = prevPass ? &prevPass->feedback[1 - prevPass->feedbackIndex].getTexture()
                               : &input;
                break;
            case PassInputSource::SOURCE_INPUT:
                src = &input;
                break;
            case PassInputSource::SELF_FEEDBACK:
                src = &pass.feedback[1 - pass.feedbackIndex].getTexture();
                break;
            }
            if (src && src->isAllocated())
            {
                shader.setUniformTexture(in.samplerName, *src, texUnit);
                if (primarySrc == nullptr) primarySrc = src;
            }
            texUnit++;
        }

        if (primarySrc)
            shader.setUniform2f("uSourceSize", (float)primarySrc->getWidth(), (float)primarySrc->getHeight());

        ofMesh quad;
        quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
        quad.addVertex(glm::vec3(0, 0, 0));
        quad.addVertex(glm::vec3(fboWidth, 0, 0));
        quad.addVertex(glm::vec3(fboWidth, fboHeight, 0));
        quad.addVertex(glm::vec3(0, fboHeight, 0));
        quad.addTexCoord(glm::vec2(0, 0));
        quad.addTexCoord(glm::vec2(1, 0));
        quad.addTexCoord(glm::vec2(1, 1));
        quad.addTexCoord(glm::vec2(0, 1));
        quad.draw();

        shader.end();
        target.end();

        pass.feedbackIndex = 1 - pass.feedbackIndex;
    }

    passes.back().feedback[1 - passes.back().feedbackIndex].draw(x, y, w, h);

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
