#include "ShaderPresets.h"
#include "ofLog.h"

void ShaderPresets::setup()
{
    presets.clear();

    presets.push_back({"passthrough", {{"passthrough", "shaders/passthru.vert", "shaders/passthru.frag", {}}}});

    presets.push_back({"colourise", {{"colourise", "shaders/passthru.vert", "shaders/colouriser.frag",
        {
            {"uHue",       0.0f, -1.0f,  1.0f, 0.0f},
            {"uSaturation",1.0f,  0.0f,  2.0f, 1.0f},
            {"uContrast",  1.0f,  0.0f,  3.0f, 1.0f},
            {"uBrightness",0.0f, -1.0f,  1.0f, 0.0f},
        }
    }}});

    presets.push_back({"sharpen", {{"sharpen", "shaders/passthru.vert", "shaders/sharpen.frag",
        {
            {"uAmount", 1.0f, 0.0f, 5.0f, 1.0f},
        }
    }}});

    presets.push_back({"mix", {{"mix", "shaders/passthru.vert", "shaders/mixer.frag",
        {
            {"uMix", 0.5f, 0.0f, 1.0f, 0.5f},
        }
    }}});

    presets.push_back({"sharpen+colour", {
        {"sharpen-pass", "shaders/passthru.vert", "shaders/sharpen.frag",
            {{"uAmount", 1.0f, 0.0f, 5.0f, 1.0f}}
        },
        {"colour-pass", "shaders/passthru.vert", "shaders/colouriser.frag",
            {
                {"uHue",       0.0f, -1.0f,  1.0f, 0.0f},
                {"uSaturation",1.0f,  0.0f,  2.0f, 1.0f},
                {"uContrast",  1.0f,  0.0f,  3.0f, 1.0f},
                {"uBrightness",0.0f, -1.0f,  1.0f, 0.0f},
            }
        },
    }});

    presets.push_back({"feedback trail", {{"feedback-pass", "shaders/passthru.vert", "shaders/feedback.frag",
        {
            {"uMix",   1.0f, 0.0f, 1.0f, 1.0f},
            {"uDecay", 0.9f, 0.0f, 1.0f, 0.9f},
            {"uSaturation", 0.6f, -1.0f, 1.0f, 0.6f},
        },
        {
            {"src",  PassInputSource::SOURCE_INPUT},
            {"fb",   PassInputSource::SELF_FEEDBACK},
        }
    }}});

    presets.push_back({"source blend", {
        {"sharpen-pass", "shaders/passthru.vert", "shaders/sharpen.frag",
            {{"uAmount", 1.0f, 0.0f, 5.0f, 1.0f}}
        },
        {"blend-pass", "shaders/passthru.vert", "shaders/source_mix.frag",
            {
                {"uMix", 0.5f, 0.0f, 1.0f, 0.5f},
            },
            {
                {"prev", PassInputSource::PREV_PASS},
                {"src",  PassInputSource::SOURCE_INPUT},
            }
        },
    }});
}

const ShaderPresets::Preset &ShaderPresets::get(int index) const
{
    return presets[index];
}

const std::string &ShaderPresets::getName(int index) const
{
    return presets[index].name;
}

void ShaderPresets::loadInto(ShaderManager &shaders, int index,
                              const std::vector<int> &sliderCCs,
                              const std::vector<int> &knobCCs)
{
    if (index < 0 || index >= (int)presets.size()) return;

    const auto &preset = presets[index];
    shaders.clearPasses();

    for (const auto &passSpec : preset.passes)
    {
        int passIdx = shaders.addPass(passSpec.name, passSpec.vertFile, passSpec.fragFile,
                                      passSpec.params, passSpec.inputs);
        if (passIdx < 0)
        {
            ofLogError("ShaderPresets") << "Failed to load pass " << passSpec.name << " in preset " << preset.name;
            return;
        }
    }

    ofLogNotice("ShaderPresets") << "Loaded preset: " << preset.name << " (" << preset.passes.size() << " pass(es))";

    int paramSlot = 0;
    for (size_t p = 0; p < preset.passes.size(); p++)
    {
        for (size_t i = 0; i < shaders.getParams(p).size() && paramSlot < 16; i++, paramSlot++)
        {
            int cc = (paramSlot < 8) ? sliderCCs[paramSlot] : knobCCs[paramSlot - 8];
            shaders.bindMidiParam(shaders.getParams(p)[i].name, cc);
        }
    }
}
