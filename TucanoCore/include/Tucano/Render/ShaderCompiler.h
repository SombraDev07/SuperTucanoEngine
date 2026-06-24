#pragma once

#include <volk.h>
#include <string>
#include <vector>

namespace Tucano {

class ShaderCompiler {
public:
    static VkShaderModule CompileShader(VkDevice device, const std::string& source, VkShaderStageFlagBits stage, const std::string& filename = "shader");
    
    // Helper to read from file
    static std::string ReadFile(const std::string& filepath);
};

} // namespace Tucano
