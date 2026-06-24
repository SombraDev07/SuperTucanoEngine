#pragma once

#include "Tucano/Render/VulkanContext.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Tucano {

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Color;
    glm::vec2 TexCoord;
    glm::vec3 Normal;

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

class Mesh {
public:
    Mesh(VulkanContext* context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();

    void Bind(VkCommandBuffer commandBuffer);
    void Draw(VkCommandBuffer commandBuffer);

    static std::shared_ptr<Mesh> CreateCube(VulkanContext* context);
    static std::shared_ptr<Mesh> CreatePlane(VulkanContext* context);

private:
    void CreateVertexBuffer(const std::vector<Vertex>& vertices);
    void CreateIndexBuffer(const std::vector<uint32_t>& indices);

    // Helpers
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& allocation);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    VulkanContext* m_Context;

    VkBuffer m_VertexBuffer;
    VmaAllocation m_VertexBufferAllocation;

    VkBuffer m_IndexBuffer;
    VmaAllocation m_IndexBufferAllocation;

    uint32_t m_IndexCount;
};

} // namespace Tucano
