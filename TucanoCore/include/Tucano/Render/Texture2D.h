#pragma once

#include "Tucano/Render/VulkanContext.h"
#include <string>
#include <memory>

namespace Tucano {

class Texture2D {
public:
    Texture2D(VulkanContext* context, const std::string& filepath, bool srgb = true);
    Texture2D(VulkanContext* context, void* data, int width, int height, int channels, bool srgb = false);
    ~Texture2D();

    VkImageView GetImageView() const { return m_ImageView; }
    VkSampler GetSampler() const { return m_Sampler; }

    bool IsValid() const { return m_ImageView != VK_NULL_HANDLE && m_Sampler != VK_NULL_HANDLE; }

private:
    void LoadFromFile(const std::string& filepath, bool srgb);
    void CreateTextureImage(void* pixels, int width, int height, int channels);
    void CreateImageView();
    void CreateTextureSampler();

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    VulkanContext* m_Context;

    VkImage m_TextureImage = VK_NULL_HANDLE;
    VmaAllocation m_TextureImageMemory = VK_NULL_HANDLE;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;

    int m_Width = 0;
    int m_Height = 0;
    VkFormat m_Format = VK_FORMAT_R8G8B8A8_SRGB;
};

} // namespace Tucano
