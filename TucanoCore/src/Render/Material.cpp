#include "Tucano/Render/Material.h"

#include "Tucano/Render/Renderer.h"
#include <array>

namespace Tucano {

void Material::BuildDescriptorSet(Renderer* renderer) {
    if (!m_IsDescriptorDirty && m_DescriptorSet != VK_NULL_HANDLE) {
        return;
    }

    VkDevice device = renderer->GetContext()->GetDevice();

    if (m_DescriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = renderer->GetMaterialDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        VkDescriptorSetLayout layout = renderer->GetMaterialDescriptorSetLayout();
        allocInfo.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
            // Handle error, maybe fallback
            return;
        }
    }

    auto albedo = (m_AlbedoMap && m_AlbedoMap->IsValid()) ? m_AlbedoMap : renderer->GetDefaultWhiteTexture();
    auto normal = (m_NormalMap && m_NormalMap->IsValid()) ? m_NormalMap : renderer->GetDefaultNormalTexture();
    auto metallicRoughness = (m_MetallicRoughnessMap && m_MetallicRoughnessMap->IsValid()) ? m_MetallicRoughnessMap : renderer->GetDefaultWhiteTexture(); // using white for now

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    albedoInfo.imageView = albedo->GetImageView();
    albedoInfo.sampler = albedo->GetSampler();

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_DescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &albedoInfo;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normalInfo.imageView = normal->GetImageView();
    normalInfo.sampler = normal->GetSampler();

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_DescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &normalInfo;

    VkDescriptorImageInfo mrInfo{};
    mrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    mrInfo.imageView = metallicRoughness->GetImageView();
    mrInfo.sampler = metallicRoughness->GetSampler();

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_DescriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &mrInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    m_IsDescriptorDirty = false;
}

} // namespace Tucano
