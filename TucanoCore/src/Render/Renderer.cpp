#include "Tucano/Render/Renderer.h"
#include "Tucano/Render/ShaderCompiler.h"
#include "Tucano/Core/Logger.h"
#include "Tucano/Mesh/Mesh.h"
#include "Tucano/ECS/Components.h"
#include "Tucano/Render/Texture2D.h"
#include <array>
#include <cstring>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace Tucano {

struct PushConstants {
    glm::mat4 model;
    glm::vec4 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
    float padding[2];
};

static void InsertImageLayoutBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, uint32_t layerCount = 1) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static void TransitionImageLayoutImmediate(VulkanContext* context, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount = 1) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(context->GetDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context->GetGraphicsQueue());

    vkFreeCommandBuffers(context->GetDevice(), commandPool, 1, &commandBuffer);
}

static VkPipeline CreateComputePipelineHelper(VkDevice device, const std::string& filepath, VkPipelineLayout layout) {
    std::string source = ShaderCompiler::ReadFile(filepath);
    VkShaderModule module = ShaderCompiler::CompileShader(device, source, VK_SHADER_STAGE_COMPUTE_BIT, filepath);
    if (!module) {
        TUCANO_CORE_ERROR("Failed to compile compute shader {0}", filepath);
        return VK_NULL_HANDLE;
    }

    VkComputePipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    createInfo.stage.module = module;
    createInfo.stage.pName = "main";
    createInfo.layout = layout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
        TUCANO_CORE_ERROR("Failed to create compute pipeline for {0}", filepath);
        pipeline = VK_NULL_HANDLE;
    }

    vkDestroyShaderModule(device, module, nullptr);
    return pipeline;
}

Renderer::Renderer(VulkanContext* context) : m_Context(context) {
    CreateDepthResources(); TUCANO_CORE_TRACE("CreateDepthResources done");
    CreateRenderPass(); TUCANO_CORE_TRACE("CreateRenderPass done");
    CreateFramebuffers(); TUCANO_CORE_TRACE("CreateFramebuffers done");
    CreateUniformBuffers(); TUCANO_CORE_TRACE("CreateUniformBuffers done");
    CreateAtmosphereResources(); TUCANO_CORE_TRACE("CreateAtmosphereResources done");
    CreateDescriptorSetLayout(); TUCANO_CORE_TRACE("CreateDescriptorSetLayout done");
    CreateDescriptorPool(); TUCANO_CORE_TRACE("CreateDescriptorPool done");
    CreateDescriptorSets(); TUCANO_CORE_TRACE("CreateDescriptorSets done");
    CreateCommandPool(); TUCANO_CORE_TRACE("CreateCommandPool done");
    CreateCommandBuffers(); TUCANO_CORE_TRACE("CreateCommandBuffers done");
    CreateSyncObjects(); TUCANO_CORE_TRACE("CreateSyncObjects done");
    CreateDefaultTextures(); TUCANO_CORE_TRACE("CreateDefaultTextures done");
    InitImGui(); TUCANO_CORE_TRACE("InitImGui done");
    InitAtmosphere(); TUCANO_CORE_TRACE("InitAtmosphere done");
}

Renderer::~Renderer() {
    VkDevice device = m_Context->GetDevice();
    WaitIdle();

    CleanupAtmosphere();
    ShutdownImGui();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, m_RenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, m_ImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, m_InFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, m_CommandPool, nullptr);

    for (auto framebuffer : m_Framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyImageView(device, m_DepthImageView, nullptr);
    vmaDestroyImage(m_Context->GetAllocator(), m_DepthImage, m_DepthImageAllocation);

    if (m_GraphicsPipeline) vkDestroyPipeline(device, m_GraphicsPipeline, nullptr);
    if (m_PipelineLayout) vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
    if (m_RenderPass) vkDestroyRenderPass(device, m_RenderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(m_Context->GetAllocator(), m_UniformBuffers[i], m_UniformBuffersAllocation[i]);
    }
    vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);

    vkDestroyDescriptorPool(device, m_MaterialDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, m_MaterialDescriptorSetLayout, nullptr);
}

void Renderer::InitPipeline(const std::string& vertFilepath, const std::string& fragFilepath) {
    VkDevice device = m_Context->GetDevice();

    std::string vertSource = ShaderCompiler::ReadFile(vertFilepath);
    std::string fragSource = ShaderCompiler::ReadFile(fragFilepath);

    VkShaderModule vertShaderModule = ShaderCompiler::CompileShader(device, vertSource, VK_SHADER_STAGE_VERTEX_BIT, vertFilepath);
    VkShaderModule fragShaderModule = ShaderCompiler::CompileShader(device, fragSource, VK_SHADER_STAGE_FRAGMENT_BIT, fragFilepath);

    if (!vertShaderModule || !fragShaderModule) {
        TUCANO_CORE_ERROR("Skipping pipeline creation due to shader compilation failure.");
        return;
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    std::array<VkDescriptorSetLayout, 2> setLayouts = { m_DescriptorSetLayout, m_MaterialDescriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void Renderer::CreateDepthResources() {
    VkFormat depthFormat = FindDepthFormat();
    VkExtent2D swapChainExtent = m_Context->GetSwapchainExtent();

    CreateImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, m_DepthImage, m_DepthImageAllocation);
    m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_Context->GetSwapchainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create render pass!");
    }
}

void Renderer::CreateFramebuffers() {
    auto const& swapChainImageViews = m_Context->GetSwapchainImageViews();
    VkExtent2D extent = m_Context->GetSwapchainExtent();
    m_Framebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            m_DepthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
            TUCANO_CORE_CRITICAL("Failed to create framebuffer!");
        }
    }
}

void Renderer::CreateUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfoRes;
        if (vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &allocInfo, &m_UniformBuffers[i], &m_UniformBuffersAllocation[i], &allocInfoRes) != VK_SUCCESS) {
            TUCANO_CORE_CRITICAL("Failed to create uniform buffer!");
        }
        m_UniformBuffersMapped[i] = allocInfoRes.pMappedData;
    }
}

void Renderer::CreateDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
    
    // UBO
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Atmosphere UBO
    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Transmittance LUT
    bindings[2].binding = 2;
    bindings[2].descriptorCount = 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].pImmutableSamplers = nullptr;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // MultiScat LUT
    bindings[3].binding = 3;
    bindings[3].descriptorCount = 1;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].pImmutableSamplers = nullptr;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // SkyView LUT
    bindings[4].binding = 4;
    bindings[4].descriptorCount = 1;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].pImmutableSamplers = nullptr;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Aerial Perspective LUT (3D)
    bindings[5].binding = 5;
    bindings[5].descriptorCount = 1;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].pImmutableSamplers = nullptr;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create descriptor set layout!");
    }

    // Material Layout (Set 1)
    std::array<VkDescriptorSetLayoutBinding, 3> matBindings{};
    
    // Albedo
    matBindings[0].binding = 0;
    matBindings[0].descriptorCount = 1;
    matBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    matBindings[0].pImmutableSamplers = nullptr;
    matBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Normal
    matBindings[1].binding = 1;
    matBindings[1].descriptorCount = 1;
    matBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    matBindings[1].pImmutableSamplers = nullptr;
    matBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // MetallicRoughness
    matBindings[2].binding = 2;
    matBindings[2].descriptorCount = 1;
    matBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    matBindings[2].pImmutableSamplers = nullptr;
    matBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo matLayoutInfo{};
    matLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    matLayoutInfo.bindingCount = static_cast<uint32_t>(matBindings.size());
    matLayoutInfo.pBindings = matBindings.data();

    if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &matLayoutInfo, nullptr, &m_MaterialDescriptorSetLayout) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create material descriptor set layout!");
    }
}

void Renderer::CreateDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 4);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create descriptor pool!");
    }

    // Material Descriptor Pool (large enough for many materials)
    std::array<VkDescriptorPoolSize, 1> matPoolSizes{};
    matPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    matPoolSizes[0].descriptorCount = 3000; // 1000 materials * 3 textures

    VkDescriptorPoolCreateInfo matPoolInfo{};
    matPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    matPoolInfo.poolSizeCount = static_cast<uint32_t>(matPoolSizes.size());
    matPoolInfo.pPoolSizes = matPoolSizes.data();
    matPoolInfo.maxSets = 1000;
    // Allow freeing individual descriptor sets
    matPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(m_Context->GetDevice(), &matPoolInfo, nullptr, &m_MaterialDescriptorPool) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create material descriptor pool!");
    }
}

void Renderer::CreateDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // Binding 0: Scene UBO
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Binding 1: Atmosphere UBO
        VkDescriptorBufferInfo atmosBufferInfo{};
        atmosBufferInfo.buffer = m_AtmosphereBuffers[i];
        atmosBufferInfo.offset = 0;
        atmosBufferInfo.range = sizeof(AtmosphereParameters) + sizeof(glm::vec4) * 3 + sizeof(glm::mat4);

        // Binding 2: Transmittance LUT
        VkDescriptorImageInfo transmittanceInfo{};
        transmittanceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transmittanceInfo.imageView = m_TransmittanceLUT.ImageView;
        transmittanceInfo.sampler = m_TransmittanceLUT.Sampler;

        // Binding 3: MultiScat LUT
        VkDescriptorImageInfo multiScatInfo{};
        multiScatInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        multiScatInfo.imageView = m_MultiScatLUT.ImageView;
        multiScatInfo.sampler = m_MultiScatLUT.Sampler;

        // Binding 4: SkyView LUT
        VkDescriptorImageInfo skyViewInfo{};
        skyViewInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        skyViewInfo.imageView = m_SkyViewLUT.ImageView;
        skyViewInfo.sampler = m_SkyViewLUT.Sampler;

        // Binding 5: Aerial Perspective LUT (3D)
        VkDescriptorImageInfo aerialPerspectiveInfo{};
        aerialPerspectiveInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        aerialPerspectiveInfo.imageView = m_AerialPerspectiveLUT.ImageView;
        aerialPerspectiveInfo.sampler = m_AerialPerspectiveLUT.Sampler;

        std::array<VkWriteDescriptorSet, 6> descriptorWrites{};
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &atmosBufferInfo;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_DescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &transmittanceInfo;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = m_DescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &multiScatInfo;

        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = m_DescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &skyViewInfo;

        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = m_DescriptorSets[i];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pImageInfo = &aerialPerspectiveInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void Renderer::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
    
    uint32_t graphicsFamily = 0;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsFamily = i; break; }
    }
    poolInfo.queueFamilyIndex = graphicsFamily;

    if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create command pool!");
    }
}

void Renderer::CreateCommandBuffers() {
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

    if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to allocate command buffers!");
    }
}

void Renderer::CreateSyncObjects() {
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_ImagesInFlight.resize(m_Context->GetSwapchainImageViews().size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkDevice device = m_Context->GetDevice();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
            TUCANO_CORE_CRITICAL("Failed to create synchronization objects for a frame!");
        }
    }
}

void Renderer::CreateDefaultTextures() {
    uint8_t whitePixel[4] = { 255, 255, 255, 255 };
    m_DefaultWhiteTexture = std::make_shared<Texture2D>(m_Context, whitePixel, 1, 1, 4, true);

    uint8_t normalPixel[4] = { 128, 128, 255, 255 };
    m_DefaultNormalTexture = std::make_shared<Texture2D>(m_Context, normalPixel, 1, 1, 4, false);

    m_DefaultMaterial = std::make_shared<Material>();
    m_DefaultMaterial->BuildDescriptorSet(this);
}

void Renderer::DrawFrame(Camera* camera, World* world) {
    // Start ImGui frame
    // ImGui_ImplVulkan_NewFrame();
    // ImGui_ImplGlfw_NewFrame();
    // ImGui::NewFrame();

    // Show Atmosphere parameter control panel
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::Begin("Atmosfera e Ceu (Hillaire)");
        bool precomputeNeeded = false;

        ImGui::SeparatorText("Propriedades do Sol");
        if (ImGui::DragFloat("Intensidade do Sol", &m_AtmosphereSettings.SunIntensity, 0.1f, 0.0f, 100.0f)) precomputeNeeded = true;
        if (ImGui::ColorEdit3("Cor do Sol", &m_AtmosphereSettings.SunColor[0])) precomputeNeeded = true;
        if (ImGui::SliderFloat("Altitude do Sol", &m_AtmosphereSettings.SunElevation, -90.0f, 90.0f)) precomputeNeeded = true;
        if (ImGui::SliderFloat("Azimute do Sol", &m_AtmosphereSettings.SunAzimuth, 0.0f, 360.0f)) precomputeNeeded = true;

        ImGui::SeparatorText("Densidades Atmosfericas");
        if (ImGui::DragFloat("Densidade Rayleigh", &m_AtmosphereSettings.RayleighDensity, 0.05f, 0.0f, 10.0f)) precomputeNeeded = true;
        if (ImGui::DragFloat("Densidade Mie", &m_AtmosphereSettings.MieDensity, 0.05f, 0.0f, 10.0f)) precomputeNeeded = true;
        if (ImGui::SliderFloat("Anisotropia Mie (g)", &m_AtmosphereSettings.MieAnisotropy, 0.0f, 0.999f)) precomputeNeeded = true;
        if (ImGui::DragFloat("Densidade Ozonio", &m_AtmosphereSettings.OzoneDensity, 0.05f, 0.0f, 10.0f)) precomputeNeeded = true;

        ImGui::SeparatorText("Dimensoes do Planeta");
        if (ImGui::DragFloat("Raio do Planeta (km)", &m_AtmosphereSettings.PlanetRadius, 10.0f, 100.0f, 10000.0f)) precomputeNeeded = true;
        if (ImGui::DragFloat("Altura da Atmosfera (km)", &m_AtmosphereSettings.AtmosphereHeight, 1.0f, 10.0f, 500.0f)) precomputeNeeded = true;

        ImGui::SeparatorText("Parametros do Solo");
        if (ImGui::ColorEdit3("Albedo do Solo", &m_AtmosphereSettings.GroundAlbedo[0])) precomputeNeeded = true;

        ImGui::SeparatorText("Pos-Processamento");
        ImGui::DragFloat("Intensidade do Ceu", &m_AtmosphereSettings.SkyIntensity, 0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Exposicao", &m_AtmosphereSettings.Exposure, 0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Gamma", &m_AtmosphereSettings.Gamma, 0.05f, 1.0f, 5.0f);
        if (ImGui::Checkbox("Habilitar Multiple Scattering", &m_AtmosphereSettings.EnableMultiScattering)) precomputeNeeded = true;
        ImGui::Checkbox("Habilitar Perspectiva Aerea", &m_AtmosphereSettings.EnableAerialPerspective);

        if (precomputeNeeded) {
            m_AtmosphereLutsNeedPrecompute = true;
        }

        ImGui::End();

    if (!m_GraphicsPipeline) return; // Skip if pipeline failed

    VkDevice device = m_Context->GetDevice();
    vkWaitForFences(device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, m_Context->GetSwapchain(), UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    m_ImagesInFlight[imageIndex] = m_InFlightFences[m_CurrentFrame];

    // Update UBOs
    UniformBufferObject ubo{};
    ubo.view = camera->GetView();
    ubo.proj = camera->GetProjection();
    memcpy(m_UniformBuffersMapped[m_CurrentFrame], &ubo, sizeof(ubo));

    UpdateAtmosphereUBOs(camera);
    TUCANO_CORE_TRACE("UBOs Updated");

    vkResetFences(device, 1, &m_InFlightFences[m_CurrentFrame]);
    vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to begin recording command buffer!");
    }
    TUCANO_CORE_TRACE("Command Buffer Begun");

    // --- ATMOSPHERE COMPUTE PASSES ---
    if (m_AtmosphereInitialized) {
        // 1. Precompute static LUTs (Transmittance & Multi-Scattering) if needed
        if (m_AtmosphereLutsNeedPrecompute) {
            // Re-bind the per-frame atmosphere UBO to the static compute sets so they
            // read the latest settings (these sets were initially bound to buffers[0]).
            {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = m_AtmosphereBuffers[m_CurrentFrame];
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(AtmosphereParameters);

                std::array<VkWriteDescriptorSet, 1> write{};
                write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write[0].dstSet = m_TransmittanceComputeSet;
                write[0].dstBinding = 1;
                write[0].dstArrayElement = 0;
                write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write[0].descriptorCount = 1;
                write[0].pBufferInfo = &bufferInfo;
                vkUpdateDescriptorSets(m_Context->GetDevice(), 1, write.data(), 0, nullptr);

                write[0].dstSet = m_MultiScatComputeSet;
                write[0].dstBinding = 2;
                vkUpdateDescriptorSets(m_Context->GetDevice(), 1, write.data(), 0, nullptr);
            }

            InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_TransmittanceLUT.Image, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

            vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_TransmittanceComputePipeline);
            vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_TransmittanceComputePipelineLayout, 0, 1, &m_TransmittanceComputeSet, 0, nullptr);
            vkCmdDispatch(m_CommandBuffers[m_CurrentFrame], 256 / 16, 64 / 16, 1);

            InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_TransmittanceLUT.Image,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

            InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_MultiScatLUT.Image, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

            vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_MultiScatComputePipeline);
            vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_MultiScatComputePipelineLayout, 0, 1, &m_MultiScatComputeSet, 0, nullptr);
            vkCmdDispatch(m_CommandBuffers[m_CurrentFrame], 32 / 8, 32 / 8, 1);

            InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_MultiScatLUT.Image,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

            m_AtmosphereLutsNeedPrecompute = false;
        }

        // 2. Compute dynamic LUTs (SkyView & Aerial Perspective)
        TUCANO_CORE_TRACE("Dispatching SkyView");
        InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_SkyViewLUT.Image, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

        vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_SkyViewComputePipeline);
        vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_SkyViewComputePipelineLayout, 0, 1, &m_SkyViewComputeSets[m_CurrentFrame], 0, nullptr);
        vkCmdDispatch(m_CommandBuffers[m_CurrentFrame], (192 + 7) / 8, (108 + 7) / 8, 1);

        InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_SkyViewLUT.Image,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

        InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_AerialPerspectiveLUT.Image, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

        vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_AerialPerspectiveComputePipeline);
        vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, m_AerialPerspectiveComputePipelineLayout, 0, 1, &m_AerialPerspectiveComputeSets[m_CurrentFrame], 0, nullptr);
        vkCmdDispatch(m_CommandBuffers[m_CurrentFrame], 32 / 8, 32 / 8, 32 / 4);

        InsertImageLayoutBarrier(m_CommandBuffers[m_CurrentFrame], m_AerialPerspectiveLUT.Image,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

        TUCANO_CORE_TRACE("Compute Done, beginning Render Pass");
    }

    // --- BEGIN RENDER PASS ---
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_Framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_Context->GetSwapchainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);

    // --- SKY RENDER PASS (Draw on depth == 1.0, before geometry so geometry overwrites where present) ---
    RenderSky(m_CommandBuffers[m_CurrentFrame], imageIndex);

    // Rebind geometry pipeline (RenderSky may have left sky pipeline bound)
    vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

    // Restore dynamic state (sky pipeline also set these, but be safe for clarity)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_Context->GetSwapchainExtent().width;
    viewport.height = (float)m_Context->GetSwapchainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_Context->GetSwapchainExtent();
    vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

    auto view = world->GetRegistry().view<TransformComponent, MeshComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& meshComp = view.get<MeshComponent>(entity);

        if (!meshComp.Mesh) continue;

        PushConstants push{};
        push.model = transform.Transform.GetModelMatrix();
        
        if (world->GetRegistry().all_of<MaterialComponent>(entity)) {
            auto& matComp = world->GetRegistry().get<MaterialComponent>(entity);
            if (matComp.Material) {
                push.albedoFactor = matComp.Material->GetProperties().AlbedoFactor;
                push.metallicFactor = matComp.Material->GetProperties().MetallicFactor;
                push.roughnessFactor = matComp.Material->GetProperties().RoughnessFactor;

                matComp.Material->BuildDescriptorSet(this);
                VkDescriptorSet matSet = matComp.Material->GetDescriptorSet();
                if (matSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 1, 1, &matSet, 0, nullptr);
                }
            }
        } else {
            push.albedoFactor = glm::vec4(1.0f);
            push.metallicFactor = 0.0f;
            push.roughnessFactor = 0.5f;
            
            VkDescriptorSet defSet = m_DefaultMaterial->GetDescriptorSet();
            if (defSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 1, 1, &defSet, 0, nullptr);
            }
        }

        vkCmdPushConstants(m_CommandBuffers[m_CurrentFrame], m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &push);

        meshComp.Mesh->Bind(m_CommandBuffers[m_CurrentFrame]);
        meshComp.Mesh->Draw(m_CommandBuffers[m_CurrentFrame]);
    }

    // Sky was rendered before geometry so geometry overwrites the sky where present
    // (uses depth == 1.0 + depthCompareOp LESS_OR_EQUAL + depthWrite FALSE)

    // Draw ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_CommandBuffers[m_CurrentFrame]);

    vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);

    if (vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

    VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_Context->GetSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::WaitIdle() {
    vkDeviceWaitIdle(m_Context->GetDevice());
}

// Helpers
void Renderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& allocation) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    if (vmaCreateImage(m_Context->GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create image!");
    }
}

VkImageView Renderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create texture image view!");
    }
    return imageView;
}

VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_Context->GetPhysicalDevice(), format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    TUCANO_CORE_CRITICAL("Failed to find supported format!");
    return VK_FORMAT_UNDEFINED;
}

VkFormat Renderer::FindDepthFormat() {
    return FindSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void Renderer::InitImGui() {
    // ImGui Descriptor Pool sizes
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * (uint32_t)std::size(poolSizes);
    poolInfo.poolSizeCount = (uint32_t)std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create ImGui descriptor pool!");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    // Query queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
    uint32_t graphicsFamily = 0;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsFamily = i; break; }
    }

    ImGui_ImplGlfw_InitForVulkan(m_Context->GetWindow()->GetNativeWindow(), true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_Context->GetInstance();
    initInfo.PhysicalDevice = m_Context->GetPhysicalDevice();
    initInfo.Device = m_Context->GetDevice();
    initInfo.QueueFamily = graphicsFamily;
    initInfo.Queue = m_Context->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_ImGuiDescriptorPool;
    initInfo.RenderPass = m_RenderPass;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = static_cast<uint32_t>(m_Context->GetSwapchainImageViews().size());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    
    TUCANO_CORE_TRACE("ImGui Vulkan Init Info:");
    TUCANO_CORE_TRACE("  MinImageCount: {0}", initInfo.MinImageCount);
    TUCANO_CORE_TRACE("  ImageCount: {0}", initInfo.ImageCount);
    TUCANO_CORE_TRACE("  Instance valid: {0}", initInfo.Instance != VK_NULL_HANDLE);
    TUCANO_CORE_TRACE("  Device valid: {0}", initInfo.Device != VK_NULL_HANDLE);
    TUCANO_CORE_TRACE("  Queue valid: {0}", initInfo.Queue != VK_NULL_HANDLE);
    TUCANO_CORE_TRACE("  Pool valid: {0}", initInfo.DescriptorPool != VK_NULL_HANDLE);
    TUCANO_CORE_TRACE("  RenderPass valid: {0}", initInfo.RenderPass != VK_NULL_HANDLE);
    
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
    }, &initInfo.Instance);

    TUCANO_CORE_TRACE("ImGui_ImplVulkan_Init starting...");
    ImGui_ImplVulkan_Init(&initInfo);
    TUCANO_CORE_TRACE("ImGui_ImplVulkan_Init done");

    // Font upload
    // ImGui_ImplVulkan_CreateFontsTexture();
}

void Renderer::ShutdownImGui() {
    if (m_ImGuiDescriptorPool != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(m_Context->GetDevice(), m_ImGuiDescriptorPool, nullptr);
        m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    }
}



void Renderer::InitAtmosphere() {
    CreateAtmospherePipelines();
    CreateAtmosphereDescriptorSets();
    
    // Transition images to shader read layouts initially
    VkCommandPool pool = m_Context->GetCommandPool();
    TransitionImageLayoutImmediate(m_Context, pool, m_TransmittanceLUT.Image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    TransitionImageLayoutImmediate(m_Context, pool, m_MultiScatLUT.Image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    TransitionImageLayoutImmediate(m_Context, pool, m_SkyViewLUT.Image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    TransitionImageLayoutImmediate(m_Context, pool, m_AerialPerspectiveLUT.Image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_AtmosphereInitialized = true;
    m_AtmosphereLutsNeedPrecompute = true;
}

void Renderer::CleanupAtmosphere() {
    VkDevice device = m_Context->GetDevice();
    VmaAllocator allocator = m_Context->GetAllocator();

    auto destroyTexture = [&](AtmosphereTexture& tex) {
        if (tex.Sampler != VK_NULL_HANDLE) vkDestroySampler(device, tex.Sampler, nullptr);
        if (tex.ImageView != VK_NULL_HANDLE) vkDestroyImageView(device, tex.ImageView, nullptr);
        if (tex.Image != VK_NULL_HANDLE) vmaDestroyImage(allocator, tex.Image, tex.Allocation);
        tex = AtmosphereTexture{};
    };

    destroyTexture(m_TransmittanceLUT);
    destroyTexture(m_MultiScatLUT);
    destroyTexture(m_SkyViewLUT);
    destroyTexture(m_AerialPerspectiveLUT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_AtmosphereBuffers[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, m_AtmosphereBuffers[i], m_AtmosphereBuffersAllocations[i]);
        }
    }

    if (m_TransmittanceComputeLayout) vkDestroyDescriptorSetLayout(device, m_TransmittanceComputeLayout, nullptr);
    if (m_TransmittanceComputePipelineLayout) vkDestroyPipelineLayout(device, m_TransmittanceComputePipelineLayout, nullptr);
    if (m_TransmittanceComputePipeline) vkDestroyPipeline(device, m_TransmittanceComputePipeline, nullptr);

    if (m_MultiScatComputeLayout) vkDestroyDescriptorSetLayout(device, m_MultiScatComputeLayout, nullptr);
    if (m_MultiScatComputePipelineLayout) vkDestroyPipelineLayout(device, m_MultiScatComputePipelineLayout, nullptr);
    if (m_MultiScatComputePipeline) vkDestroyPipeline(device, m_MultiScatComputePipeline, nullptr);

    if (m_SkyViewComputeLayout) vkDestroyDescriptorSetLayout(device, m_SkyViewComputeLayout, nullptr);
    if (m_SkyViewComputePipelineLayout) vkDestroyPipelineLayout(device, m_SkyViewComputePipelineLayout, nullptr);
    if (m_SkyViewComputePipeline) vkDestroyPipeline(device, m_SkyViewComputePipeline, nullptr);

    if (m_AerialPerspectiveComputeLayout) vkDestroyDescriptorSetLayout(device, m_AerialPerspectiveComputeLayout, nullptr);
    if (m_AerialPerspectiveComputePipelineLayout) vkDestroyPipelineLayout(device, m_AerialPerspectiveComputePipelineLayout, nullptr);
    if (m_AerialPerspectiveComputePipeline) vkDestroyPipeline(device, m_AerialPerspectiveComputePipeline, nullptr);

    if (m_SkyPipelineLayout) vkDestroyPipelineLayout(device, m_SkyPipelineLayout, nullptr);
    if (m_SkyPipeline) vkDestroyPipeline(device, m_SkyPipeline, nullptr);

    if (m_AtmosphereComputePool) vkDestroyDescriptorPool(device, m_AtmosphereComputePool, nullptr);
}

void Renderer::CreateAtmosphereResources() {
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // Transmittance (2D, 256x64)
    CreateImage(256, 64, format, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY, m_TransmittanceLUT.Image, m_TransmittanceLUT.Allocation);
    m_TransmittanceLUT.ImageView = CreateImageView(m_TransmittanceLUT.Image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    m_TransmittanceLUT.Sampler = CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // MultiScat (2D, 32x32)
    CreateImage(32, 32, format, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY, m_MultiScatLUT.Image, m_MultiScatLUT.Allocation);
    m_MultiScatLUT.ImageView = CreateImageView(m_MultiScatLUT.Image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    m_MultiScatLUT.Sampler = CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // SkyView (2D, 192x108)
    CreateImage(192, 108, format, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY, m_SkyViewLUT.Image, m_SkyViewLUT.Allocation);
    m_SkyViewLUT.ImageView = CreateImageView(m_SkyViewLUT.Image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    m_SkyViewLUT.Sampler = CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // Aerial Perspective (3D, 32x32x32)
    CreateImage3D(32, 32, 32, format, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY, m_AerialPerspectiveLUT.Image, m_AerialPerspectiveLUT.Allocation);
    m_AerialPerspectiveLUT.ImageView = CreateImageView3D(m_AerialPerspectiveLUT.Image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    m_AerialPerspectiveLUT.Sampler = CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // Atmosphere buffers (per frame)
    VkDeviceSize bufferSize = sizeof(AtmosphereParameters) + sizeof(glm::vec4) * 3 + sizeof(glm::mat4);
    m_AtmosphereBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_AtmosphereBuffersAllocations.resize(MAX_FRAMES_IN_FLIGHT);
    m_AtmosphereBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfoRes;
        if (vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &allocInfo, &m_AtmosphereBuffers[i], &m_AtmosphereBuffersAllocations[i], &allocInfoRes) != VK_SUCCESS) {
            TUCANO_CORE_CRITICAL("Failed to create atmosphere uniform buffer!");
        }
        m_AtmosphereBuffersMapped[i] = allocInfoRes.pMappedData;
    }
}

void Renderer::CreateAtmospherePipelines() {
    VkDevice device = m_Context->GetDevice();

    // Transmittance Compute Layout
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_TransmittanceComputeLayout);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_TransmittanceComputeLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_TransmittanceComputePipelineLayout);

        m_TransmittanceComputePipeline = CreateComputePipelineHelper(device, "assets/shaders/transmittance_lut.comp", m_TransmittanceComputePipelineLayout);
    }

    // MultiScat Compute Layout
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[2].binding = 2;
        bindings[2].descriptorCount = 1;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_MultiScatComputeLayout);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_MultiScatComputeLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_MultiScatComputePipelineLayout);

        m_MultiScatComputePipeline = CreateComputePipelineHelper(device, "assets/shaders/multiscat_lut.comp", m_MultiScatComputePipelineLayout);
    }

    // SkyView Compute Layout
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[2].binding = 2;
        bindings[2].descriptorCount = 1;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[3].binding = 3;
        bindings[3].descriptorCount = 1;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_SkyViewComputeLayout);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_SkyViewComputeLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_SkyViewComputePipelineLayout);

        m_SkyViewComputePipeline = CreateComputePipelineHelper(device, "assets/shaders/skyview_lut.comp", m_SkyViewComputePipelineLayout);
    }

    // Aerial Perspective Compute Layout
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[2].binding = 2;
        bindings[2].descriptorCount = 1;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[3].binding = 3;
        bindings[3].descriptorCount = 1;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_AerialPerspectiveComputeLayout);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_AerialPerspectiveComputeLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_AerialPerspectiveComputePipelineLayout);

        m_AerialPerspectiveComputePipeline = CreateComputePipelineHelper(device, "assets/shaders/aerial_perspective_lut.comp", m_AerialPerspectiveComputePipelineLayout);
    }

    // Sky graphics pass
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_SkyPipelineLayout);

        std::string vertSource = ShaderCompiler::ReadFile("assets/shaders/sky.vert");
        std::string fragSource = ShaderCompiler::ReadFile("assets/shaders/sky.frag");

        VkShaderModule vertShaderModule = ShaderCompiler::CompileShader(device, vertSource, VK_SHADER_STAGE_VERTEX_BIT, "assets/shaders/sky.vert");
        VkShaderModule fragShaderModule = ShaderCompiler::CompileShader(device, fragSource, VK_SHADER_STAGE_FRAGMENT_BIT, "assets/shaders/sky.frag");

        if (!vertShaderModule || !fragShaderModule) {
            TUCANO_CORE_CRITICAL("Failed to compile sky pass shaders!");
        }

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_SkyPipelineLayout;
        pipelineInfo.renderPass = m_RenderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_SkyPipeline) != VK_SUCCESS) {
            TUCANO_CORE_CRITICAL("Failed to create sky graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }
}

void Renderer::CreateAtmosphereDescriptorSets() {
    VkDevice device = m_Context->GetDevice();

    // Create a dedicated compute descriptor pool
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 10;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 10;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = 10;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 20;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_AtmosphereComputePool);

    // 1. Transmittance Compute Set
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_AtmosphereComputePool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_TransmittanceComputeLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &m_TransmittanceComputeSet);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = m_TransmittanceLUT.ImageView;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_AtmosphereBuffers[0]; // Use first frame since only settings are used
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(AtmosphereParameters);

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_TransmittanceComputeSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_TransmittanceComputeSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    // 2. MultiScat Compute Set
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_AtmosphereComputePool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_MultiScatComputeLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &m_MultiScatComputeSet);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = m_MultiScatLUT.ImageView;

        VkDescriptorImageInfo transmittanceInfo{};
        transmittanceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transmittanceInfo.imageView = m_TransmittanceLUT.ImageView;
        transmittanceInfo.sampler = m_TransmittanceLUT.Sampler;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_AtmosphereBuffers[0];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(AtmosphereParameters);

        std::array<VkWriteDescriptorSet, 3> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_MultiScatComputeSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_MultiScatComputeSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &transmittanceInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = m_MultiScatComputeSet;
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].descriptorCount = 1;
        writes[2].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    // 3. SkyView and Aerial Perspective Sets (per frame)
    m_SkyViewComputeSets.resize(MAX_FRAMES_IN_FLIGHT);
    m_AerialPerspectiveComputeSets.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // SkyView
        {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_AtmosphereComputePool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &m_SkyViewComputeLayout;
            vkAllocateDescriptorSets(device, &allocInfo, &m_SkyViewComputeSets[i]);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView = m_SkyViewLUT.ImageView;

            VkDescriptorImageInfo transmittanceInfo{};
            transmittanceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            transmittanceInfo.imageView = m_TransmittanceLUT.ImageView;
            transmittanceInfo.sampler = m_TransmittanceLUT.Sampler;

            VkDescriptorImageInfo multiScatInfo{};
            multiScatInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            multiScatInfo.imageView = m_MultiScatLUT.ImageView;
            multiScatInfo.sampler = m_MultiScatLUT.Sampler;

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_AtmosphereBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(AtmosphereParameters) + sizeof(glm::vec4) * 3 + sizeof(glm::mat4);

            std::array<VkWriteDescriptorSet, 4> writes{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = m_SkyViewComputeSets[i];
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &imageInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_SkyViewComputeSets[i];
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &transmittanceInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = m_SkyViewComputeSets[i];
            writes[2].dstBinding = 2;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo = &multiScatInfo;

            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = m_SkyViewComputeSets[i];
            writes[3].dstBinding = 3;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[3].descriptorCount = 1;
            writes[3].pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        // Aerial Perspective
        {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_AtmosphereComputePool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &m_AerialPerspectiveComputeLayout;
            vkAllocateDescriptorSets(device, &allocInfo, &m_AerialPerspectiveComputeSets[i]);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView = m_AerialPerspectiveLUT.ImageView;

            VkDescriptorImageInfo transmittanceInfo{};
            transmittanceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            transmittanceInfo.imageView = m_TransmittanceLUT.ImageView;
            transmittanceInfo.sampler = m_TransmittanceLUT.Sampler;

            VkDescriptorImageInfo multiScatInfo{};
            multiScatInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            multiScatInfo.imageView = m_MultiScatLUT.ImageView;
            multiScatInfo.sampler = m_MultiScatLUT.Sampler;

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_AtmosphereBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(AtmosphereParameters) + sizeof(glm::vec4) * 3 + sizeof(glm::mat4);

            std::array<VkWriteDescriptorSet, 4> writes{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = m_AerialPerspectiveComputeSets[i];
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &imageInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_AerialPerspectiveComputeSets[i];
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &transmittanceInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = m_AerialPerspectiveComputeSets[i];
            writes[2].dstBinding = 2;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo = &multiScatInfo;

            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = m_AerialPerspectiveComputeSets[i];
            writes[3].dstBinding = 3;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[3].descriptorCount = 1;
            writes[3].pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}

void Renderer::UpdateAtmosphereUBOs(Camera* camera) {
    if (!m_AtmosphereInitialized) return;

    AtmosphereParameters params{};
    params.PlanetRadius = m_AtmosphereSettings.PlanetRadius;
    params.AtmosphereHeight = m_AtmosphereSettings.AtmosphereHeight;

    // Scale Rayleigh (sea level base coefficient is approx 5.8e-3, 13.5e-3, 33.1e-3)
    glm::vec3 baseRayleighScattering{5.802e-3f, 13.558e-3f, 33.100e-3f};
    params.RayleighScattering = glm::vec4(baseRayleighScattering * m_AtmosphereSettings.RayleighDensity, m_AtmosphereSettings.RayleighScaleHeight);

    // Scale Mie (sea level base scattering is approx 3.996e-3)
    glm::vec3 baseMieScattering{3.996e-3f, 3.996e-3f, 3.996e-3f};
    params.MieScattering = glm::vec4(baseMieScattering * m_AtmosphereSettings.MieDensity, m_AtmosphereSettings.MieScaleHeight);
    
    glm::vec3 baseMieExtinction = baseMieScattering / 0.9f;
    params.MieExtinction = glm::vec4(baseMieExtinction * m_AtmosphereSettings.MieDensity, m_AtmosphereSettings.MieAnisotropy);

    // Ozone absorption
    glm::vec3 baseOzoneExtinction{0.650e-3f, 1.881e-3f, 0.085e-3f};
    params.AbsorptionExtinction = glm::vec4(baseOzoneExtinction * m_AtmosphereSettings.OzoneDensity, 25.0f);
    params.AbsorptionDensity = glm::vec4(15.0f, 0.0f, 0.0f, 0.0f);

    params.GroundAlbedo = glm::vec4(m_AtmosphereSettings.GroundAlbedo, 1.0f);

    // Compute sun direction vector
    float pitch = glm::radians(m_AtmosphereSettings.SunElevation);
    float yaw = glm::radians(m_AtmosphereSettings.SunAzimuth);
    glm::vec3 sunDir{
        cos(pitch) * sin(yaw),
        sin(pitch),
        cos(pitch) * cos(yaw)
    };
    sunDir = glm::normalize(sunDir);

    params.SunDirectionAndIntensity = glm::vec4(sunDir, m_AtmosphereSettings.SunIntensity);
    params.SunColor = glm::vec4(m_AtmosphereSettings.SunColor, 1.0f);

    uint8_t* dest = static_cast<uint8_t*>(m_AtmosphereBuffersMapped[m_CurrentFrame]);
    
    std::memcpy(dest, &params, sizeof(AtmosphereParameters));
    dest += sizeof(AtmosphereParameters);
    glm::vec3 camPos = glm::vec3(glm::inverse(camera->GetView())[3]);
    // Converter de metros para km
    glm::vec3 camPosKm = camPos * 0.001f;
    // O centro do planeta está em (0, -PlanetRadius, 0)
    camPosKm.y += m_AtmosphereSettings.PlanetRadius;
    
    glm::vec4 camPosVec4(camPosKm, 1.0f);
    std::memcpy(dest, &camPosVec4, sizeof(glm::vec4));
    dest += sizeof(glm::vec4);

    glm::mat4 invViewProj = glm::inverse(camera->GetProjection() * camera->GetView());
    std::memcpy(dest, &invViewProj, sizeof(glm::mat4));
    dest += sizeof(glm::mat4);

    float viewportWidth = (float)m_Context->GetSwapchainExtent().width;
    float viewportHeight = (float)m_Context->GetSwapchainExtent().height;
    glm::vec4 postProcessParams(
        m_AtmosphereSettings.Exposure,
        m_AtmosphereSettings.Gamma,
        viewportWidth,
        viewportHeight
    );
    std::memcpy(dest, &postProcessParams, sizeof(glm::vec4));
    dest += sizeof(glm::vec4);

    glm::vec4 atmosphereFlags(
        m_AtmosphereSettings.EnableMultiScattering ? 1.0f : 0.0f,
        m_AtmosphereSettings.EnableAerialPerspective ? 1.0f : 0.0f,
        m_AtmosphereSettings.SkyIntensity,
        0.0f
    );
    std::memcpy(dest, &atmosphereFlags, sizeof(glm::vec4));
    
    // Ensure memory is visible to GPU if not host coherent
    vmaFlushAllocation(m_Context->GetAllocator(), m_AtmosphereBuffersAllocations[m_CurrentFrame], 0, VK_WHOLE_SIZE);
}

void Renderer::ComputeAtmosphereLUTs() {
    // Left empty since we process dispatches directly inside the command buffer recording in DrawFrame
}

void Renderer::RenderSky(VkCommandBuffer cmd, uint32_t imageIndex) {
    if (!m_AtmosphereInitialized) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyPipeline);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_Context->GetSwapchainExtent().width;
    viewport.height = (float)m_Context->GetSwapchainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_Context->GetSwapchainExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyPipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void Renderer::CreateImage3D(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& allocation) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    if (vmaCreateImage(m_Context->GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create 3D image!");
    }
}

VkImageView Renderer::CreateImageView3D(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create 3D image view!");
    }
    return imageView;
}

VkSampler Renderer::CreateSampler(VkFilter filter, VkSamplerAddressMode addressMode) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    VkSampler sampler;
    if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create sampler!");
    }
    return sampler;
}

} // namespace Tucano
