#pragma once

#include "Tucano/Render/Texture2D.h"
#include <glm/glm.hpp>
#include <memory>

namespace Tucano {

struct PBRProperties {
    glm::vec4 AlbedoFactor{1.0f};
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;
    float AO = 1.0f;
};

class Material {
public:
    Material() = default;
    ~Material() = default;

    void SetAlbedoMap(std::shared_ptr<Texture2D> texture) { m_AlbedoMap = texture; }
    void SetNormalMap(std::shared_ptr<Texture2D> texture) { m_NormalMap = texture; }
    void SetMetallicRoughnessMap(std::shared_ptr<Texture2D> texture) { m_MetallicRoughnessMap = texture; }

    std::shared_ptr<Texture2D> GetAlbedoMap() const { return m_AlbedoMap; }
    std::shared_ptr<Texture2D> GetNormalMap() const { return m_NormalMap; }
    std::shared_ptr<Texture2D> GetMetallicRoughnessMap() const { return m_MetallicRoughnessMap; }

    PBRProperties& GetProperties() { return m_Properties; }
    const PBRProperties& GetProperties() const { return m_Properties; }

    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
    void BuildDescriptorSet(class Renderer* renderer);

private:
    PBRProperties m_Properties;

    std::shared_ptr<Texture2D> m_AlbedoMap;
    std::shared_ptr<Texture2D> m_NormalMap;
    std::shared_ptr<Texture2D> m_MetallicRoughnessMap;

    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    bool m_IsDescriptorDirty = true;
};

} // namespace Tucano
