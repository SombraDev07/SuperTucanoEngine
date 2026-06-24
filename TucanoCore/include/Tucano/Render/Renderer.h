#pragma once

#include "Tucano/Render/VulkanContext.h"
#include "Tucano/Camera/Camera.h"
#include "Tucano/World/World.h"
#include "Tucano/Render/Atmosphere.h"
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

    VkDescriptorSetLayout GetMaterialDescriptorSetLayout() const { return m_MaterialDescriptorSetLayout; }
    VkDescriptorPool GetMaterialDescriptorPool() const { return m_MaterialDescriptorPool; }
    std::shared_ptr<class Texture2D> GetDefaultWhiteTexture() const { return m_DefaultWhiteTexture; }
    std::shared_ptr<class Texture2D> GetDefaultNormalTexture() const { return m_DefaultNormalTexture; }
    VulkanContext* GetContext() const { return m_Context; }

    AtmosphereSettings& GetAtmosphereSettings() { return m_AtmosphereSettings; }

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
    void CreateDefaultTextures();
    void InitImGui();
    void ShutdownImGui();

    // Atmosphere implementations
    void InitAtmosphere();
    void CleanupAtmosphere();
    void CreateAtmosphereResources();
    void CreateAtmospherePipelines();
    void CreateAtmosphereDescriptorSets();
    void UpdateAtmosphereUBOs(Camera* camera);
    void ComputeAtmosphereLUTs();
    void RenderSky(VkCommandBuffer cmd, uint32_t imageIndex);

    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& allocation);
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();

    // 3D Texture & Sampler Helpers
    void CreateImage3D(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& allocation);
    VkImageView CreateImageView3D(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkSampler CreateSampler(VkFilter filter, VkSamplerAddressMode addressMode);

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

    VkDescriptorSetLayout m_MaterialDescriptorSetLayout;
    VkDescriptorPool m_MaterialDescriptorPool;
    VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;

    std::shared_ptr<class Texture2D> m_DefaultWhiteTexture;
    std::shared_ptr<class Texture2D> m_DefaultNormalTexture;
    std::shared_ptr<class Material> m_DefaultMaterial;

    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;

    uint32_t m_CurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    // Atmosphere GPU structures
    struct AtmosphereTexture {
        VkImage Image = VK_NULL_HANDLE;
        VmaAllocation Allocation = VK_NULL_HANDLE;
        VkImageView ImageView = VK_NULL_HANDLE;
        VkSampler Sampler = VK_NULL_HANDLE;
    };

    AtmosphereTexture m_TransmittanceLUT;
    AtmosphereTexture m_MultiScatLUT;
    AtmosphereTexture m_SkyViewLUT;
    AtmosphereTexture m_AerialPerspectiveLUT;

    // Atmosphere Uniform Buffers (per frame)
    std::vector<VkBuffer> m_AtmosphereBuffers;
    std::vector<VmaAllocation> m_AtmosphereBuffersAllocations;
    std::vector<void*> m_AtmosphereBuffersMapped;

    VkDescriptorPool m_AtmosphereComputePool = VK_NULL_HANDLE;

    // Compute Pipelines
    VkDescriptorSetLayout m_TransmittanceComputeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_TransmittanceComputePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_TransmittanceComputePipeline = VK_NULL_HANDLE;
    VkDescriptorSet m_TransmittanceComputeSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_MultiScatComputeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_MultiScatComputePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_MultiScatComputePipeline = VK_NULL_HANDLE;
    VkDescriptorSet m_MultiScatComputeSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_SkyViewComputeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_SkyViewComputePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_SkyViewComputePipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_SkyViewComputeSets;

    VkDescriptorSetLayout m_AerialPerspectiveComputeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_AerialPerspectiveComputePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_AerialPerspectiveComputePipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_AerialPerspectiveComputeSets;

    // Sky Graphics Pipeline
    VkPipelineLayout m_SkyPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_SkyPipeline = VK_NULL_HANDLE;

    // Settings
    AtmosphereSettings m_AtmosphereSettings;
    bool m_AtmosphereInitialized = false;
    bool m_AtmosphereLutsNeedPrecompute = true;
};

} // namespace Tucano
