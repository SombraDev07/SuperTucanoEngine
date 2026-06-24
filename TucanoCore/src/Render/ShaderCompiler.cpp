#include "Tucano/Render/ShaderCompiler.h"
#include "Tucano/Core/Logger.h"

#include <shaderc/shaderc.hpp>
#include <fstream>
#include <sstream>

namespace Tucano {

std::string ShaderCompiler::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        TUCANO_CORE_ERROR("Failed to open shader file: {0}", filepath);
        return "";
    }

    size_t fileSize = (size_t)file.tellg();
    std::string buffer(fileSize, ' ');
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule ShaderCompiler::CompileShader(VkDevice device, const std::string& source, VkShaderStageFlagBits stage, const std::string& filename) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    
    shaderc_shader_kind kind;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: kind = shaderc_glsl_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_glsl_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: kind = shaderc_glsl_compute_shader; break;
        default: kind = shaderc_glsl_infer_from_source; break;
    }

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, filename.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        TUCANO_CORE_ERROR("Shader Compilation Error in {0}:\n{1}", filename, module.GetErrorMessage());
        return VK_NULL_HANDLE;
    }

    std::vector<uint32_t> spirv(module.cbegin(), module.cend());

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        TUCANO_CORE_ERROR("Failed to create shader module for {0}", filename);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

} // namespace Tucano
