#include "Tucano/Mesh/Mesh.h"
#include "Tucano/Core/Logger.h"

namespace Tucano {

VkVertexInputBindingDescription Vertex::GetBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, Position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, Color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, TexCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, Normal);

    return attributeDescriptions;
}

Mesh::Mesh(VulkanContext* context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : m_Context(context), m_IndexCount(static_cast<uint32_t>(indices.size())) {
    CreateVertexBuffer(vertices);
    CreateIndexBuffer(indices);
}

Mesh::~Mesh() {
    vmaDestroyBuffer(m_Context->GetAllocator(), m_IndexBuffer, m_IndexBufferAllocation);
    vmaDestroyBuffer(m_Context->GetAllocator(), m_VertexBuffer, m_VertexBufferAllocation);
}

void Mesh::Bind(VkCommandBuffer commandBuffer) {
    VkBuffer vertexBuffers[] = {m_VertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::Draw(VkCommandBuffer commandBuffer) {
    vkCmdDrawIndexed(commandBuffer, m_IndexCount, 1, 0, 0, 0);
}

void Mesh::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& allocation) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    if (vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        TUCANO_CORE_CRITICAL("Failed to create buffer!");
    }
}

VkCommandBuffer Mesh::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // We assume Renderer has a pool, but we don't have direct access here. 
    // For AAA, we'd have a CommandManager. For now, we create a temporary pool on the context.
    // Actually, creating a pool per buffer is bad. Let's do it anyway for this milestone.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    
    // We need to query queue family again or get it from context.
    // Assuming 0 is graphics/present for simplicity. 
    // Ideally VulkanContext should expose a CommandPool for one-time submits.
    // Let's create a temporary pool on the graphics queue family.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
    
    uint32_t graphicsFamily = 0;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
            break;
        }
    }
    poolInfo.queueFamilyIndex = graphicsFamily;

    VkCommandPool commandPool;
    vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &commandPool);

    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    // Hack: store the pool in a custom struct or rely on end function. We will just pass the pool out via a member variable or recreate it?
    // Let's create a static pool or just do it inline. Since this is just a quick upload...
    // To fix properly, I will do the upload inline here to avoid returning both buffer and pool.
    return commandBuffer;
}

void Mesh::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    // Quick inline command buffer
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context->GetPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
    
    uint32_t graphicsFamily = 0;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsFamily = i; break; }
    }
    poolInfo.queueFamilyIndex = graphicsFamily;

    VkCommandPool commandPool;
    vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkFreeCommandBuffers(m_Context->GetDevice(), commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(m_Context->GetDevice(), commandPool, nullptr);
}

void Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);

    void* data;
    vmaMapMemory(m_Context->GetAllocator(), stagingBufferAllocation, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vmaUnmapMemory(m_Context->GetAllocator(), stagingBufferAllocation);

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, m_VertexBuffer, m_VertexBufferAllocation);

    CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vmaDestroyBuffer(m_Context->GetAllocator(), stagingBuffer, stagingBufferAllocation);
}

void Mesh::CreateIndexBuffer(const std::vector<uint32_t>& indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);

    void* data;
    vmaMapMemory(m_Context->GetAllocator(), stagingBufferAllocation, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vmaUnmapMemory(m_Context->GetAllocator(), stagingBufferAllocation);

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, m_IndexBuffer, m_IndexBufferAllocation);

    CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vmaDestroyBuffer(m_Context->GetAllocator(), stagingBuffer, stagingBufferAllocation);
}

std::shared_ptr<Mesh> Mesh::CreateCube(VulkanContext* context) {
    std::vector<Vertex> vertices = {
        // Front face (Red)
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

        // Back face (Green)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

        // Left face (Blue)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

        // Right face (Yellow)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

        // Top face (Magenta)
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

        // Bottom face (Cyan)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    };

    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  3,  0,
        4,  5,  6,  6,  7,  4,
        8,  9, 10, 10, 11,  8,
       12, 13, 14, 14, 15, 12,
       16, 17, 18, 18, 19, 16,
       20, 21, 22, 22, 23, 20
    };

    return std::make_shared<Mesh>(context, vertices, indices);
}

std::shared_ptr<Mesh> Mesh::CreatePlane(VulkanContext* context) {
    std::vector<Vertex> vertices = {
        {{-5.0f, 0.0f,  5.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 5.0f, 0.0f,  5.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 5.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-5.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    };
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0
    };
    return std::make_shared<Mesh>(context, vertices, indices);
}

} // namespace Tucano
