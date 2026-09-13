#pragma once

#include "ShaderParam.h"
#include "ShaderManager.h"
#include <string>
#include <vector>

class ShaderPresets
{
public:
    struct ShaderPassSpec
    {
        std::string name;
        std::string vertFile;
        std::string fragFile;
        std::vector<ShaderParam> params;
        std::vector<PassInput> inputs;
    };

    struct Preset
    {
        std::string name;
        std::vector<ShaderPassSpec> passes;
    };

    void setup();
    int size() const { return (int)presets.size(); }
    const Preset &get(int index) const;
    const std::string &getName(int index) const;

    void loadInto(ShaderManager &shaders, int index,
                  const std::vector<int> &sliderCCs,
                  const std::vector<int> &knobCCs);

private:
    std::vector<Preset> presets;
};
