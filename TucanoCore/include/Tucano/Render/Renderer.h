#pragma once

#include "Tucano/Render/VulkanContext.h"
#include "Tucano/Camera/Camera.h"
#include "Tucano/World/World.h"
#include <vector>

namespace Tucano {

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};

class Renderer {
public:
    Renderer(VulkanContext* context);
    ~Renderer();

    void InitPipeline(const std::string& vertFilepath, const std::string& fragFilepath);
    void DrawFrame(Camera* camera, World* world);
    void WaitIdle();

private:
    void CreateDepthResources();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateUniformBuffers();
    void CreateDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();

    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& allocation);
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();

private:
    VulkanContext* m_Context;

    VkImage m_DepthImage;
    VmaAllocation m_DepthImageAllocation;
    VkImageView m_DepthImageView;

    VkRenderPass m_RenderPass;
    VkDescriptorSetLayout m_DescriptorSetLayout;
    VkPipelineLayout m_PipelineLayout;
    VkPipeline m_GraphicsPipeline;

    std::vector<VkFramebuffer> m_Framebuffers;

    std::vector<VkBuffer> m_UniformBuffers;
    std::vector<VmaAllocation> m_UniformBuffersAllocation;
    std::vector<void*> m_UniformBuffersMapped;

    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    uint32_t m_CurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace Tucano
