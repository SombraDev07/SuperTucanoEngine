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

static std::string ResolveIncludes(const std::string& source, const std::string& currentFilePath) {
    std::string directory = "";
    size_t lastSlash = currentFilePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        directory = currentFilePath.substr(0, lastSlash + 1);
    }

    std::stringstream ss(source);
    std::string line;
    std::string processedSource = "";

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t includePos = line.find("#include");
        if (includePos != std::string::npos) {
            size_t startQuote = line.find('"', includePos);
            size_t endQuote = line.find('"', startQuote + 1);
            if (startQuote != std::string::npos && endQuote != std::string::npos) {
                std::string includeFile = line.substr(startQuote + 1, endQuote - startQuote - 1);
                std::string includePath = directory + includeFile;
                std::string includeContent = ShaderCompiler::ReadFile(includePath);
                if (!includeContent.empty()) {
                    processedSource += ResolveIncludes(includeContent, includePath) + "\n";
                    continue;
                } else {
                    TUCANO_CORE_ERROR("Failed to resolve shader include: {0} in {1}", includePath, currentFilePath);
                }
            }
        }
        processedSource += line + "\n";
    }
    return processedSource;
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

    std::string preprocessedSource = ResolveIncludes(source, filename);
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(preprocessedSource, kind, filename.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        TUCANO_CORE_ERROR("Shader Compilation Error in {0}:\n{1}", filename, module.GetErrorMessage());
        // Print preprocessed source with line numbers to make debugging errors easier
        std::stringstream ss(preprocessedSource);
        std::string line;
        int lineNum = 1;
        std::string debugOutput = "";
        while (std::getline(ss, line)) {
            debugOutput += std::to_string(lineNum++) + ": " + line + "\n";
        }
        TUCANO_CORE_ERROR("Preprocessed Source:\n{0}", debugOutput);
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
