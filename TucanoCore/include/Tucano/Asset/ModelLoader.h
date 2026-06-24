#pragma once

#include "Tucano/World/World.h"
#include "Tucano/Render/VulkanContext.h"
#include <string>

namespace Tucano {

class ModelLoader {
public:
    static bool LoadGLTF(const std::string& filepath, World& world, VulkanContext* context);
    static bool LoadFBX(const std::string& filepath, World& world, VulkanContext* context);
};

} // namespace Tucano
