#pragma once

#include "Tucano/Core/Window.h"
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>
#include <optional>

namespace Tucano {

struct QueueFamilyIndices {
    std::optional<uint32_t> GraphicsFamily;
    std::optional<uint32_t> PresentFamily;
    bool IsComplete() { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;
};

class VulkanContext {
public:
    VulkanContext(Window* window);
    ~VulkanContext();

    inline VkInstance GetInstance() const { return m_Instance; }
    inline VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    inline VkDevice GetDevice() const { return m_Device; }
    inline VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    inline VkQueue GetPresentQueue() const { return m_PresentQueue; }
    inline VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
    inline VkFormat GetSwapchainImageFormat() const { return m_SwapchainImageFormat; }
    inline VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }
    inline const std::vector<VkImageView>& GetSwapchainImageViews() const { return m_SwapchainImageViews; }
    inline VmaAllocator GetAllocator() const { return m_Allocator; }

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateAllocator();
    void CreateSwapchain();
    void CreateImageViews();

    // Helpers
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();
    bool IsDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

private:
    Window* m_Window;

    VkInstance m_Instance;
    VkDebugUtilsMessengerEXT m_DebugMessenger;
    VkSurfaceKHR m_Surface;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device;

    VkQueue m_GraphicsQueue;
    VkQueue m_PresentQueue;

    VmaAllocator m_Allocator;

    VkSwapchainKHR m_Swapchain;
    std::vector<VkImage> m_SwapchainImages;
    VkFormat m_SwapchainImageFormat;
    VkExtent2D m_SwapchainExtent;
    std::vector<VkImageView> m_SwapchainImageViews;

    const std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> m_DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif
};

} // namespace Tucano
