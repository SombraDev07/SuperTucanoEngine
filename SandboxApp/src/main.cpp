#include "Tucano/Core/Logger.h"
#include "Tucano/Core/Window.h"
#include "Tucano/Core/Input.h"
#include "Tucano/Render/VulkanContext.h"
#include "Tucano/Render/Renderer.h"
#include "Tucano/Camera/Camera.h"
#include "Tucano/Camera/FPSCameraController.h"
#include "Tucano/World/World.h"
#include "Tucano/Mesh/Mesh.h"

#include "Tucano/ECS/Components.h"
#include "Tucano/Asset/ModelLoader.h"

#include <GLFW/glfw3.h>
#include <chrono>

using namespace Tucano;

int main() {
    Logger::Init();
    TUCANO_INFO("SandboxApp started.");

    Window window(WindowProps("Tucano Engine Sandbox (3D)", 1280, 720));
    Input::Init(window.GetNativeWindow());

    VulkanContext vulkanContext(&window);
    Renderer renderer(&vulkanContext);

    renderer.InitPipeline("assets/shaders/pbr.vert", "assets/shaders/pbr.frag");

    World world;
    
    // Add Ground
    auto planeMesh = Mesh::CreatePlane(&vulkanContext);
    auto groundEntity = world.CreateEntity("Ground");
    auto& groundTransform = world.GetRegistry().get<TransformComponent>(groundEntity).Transform;
    groundTransform.Scale = glm::vec3(10.0f, 1.0f, 10.0f);
    groundTransform.Position = glm::vec3(0.0f, -1.0f, 0.0f);
    world.GetRegistry().emplace<MeshComponent>(groundEntity, planeMesh);

    // Load glTF Box
    ModelLoader::LoadGLTF("assets/models/Industrial_BrickRuin_Window_Brick_Straight_01_vdcjcfx_Raw.gltf", world, &vulkanContext);

    Camera camera(45.0f, (float)window.GetWidth() / (float)window.GetHeight(), 0.1f, 10000.0f);
    FPSCameraController cameraController(&camera);
    cameraController.SetPosition(glm::vec3(0.0f, 10.0f, 30.0f));

    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;
    auto fpsTimer = lastTime;

    while (!window.ShouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        window.Update();

        // Update Camera
        cameraController.Update(deltaTime);

        // Update some objects (e.g., rotate cubes)
        auto view = world.GetRegistry().view<TransformComponent, TagComponent>();
        for (auto entity : view) {
            auto& tag = view.get<TagComponent>(entity);
            if (tag.Tag == "Cube") {
                auto& transform = view.get<TransformComponent>(entity).Transform;
                transform.Rotate(glm::vec3(0.0f, 1.0f * deltaTime, 0.0f));
            }
        }

        renderer.DrawFrame(&camera, &world);

        frames++;
        if (std::chrono::duration<float, std::chrono::seconds::period>(currentTime - fpsTimer).count() >= 1.0f) {
            window.SetTitle("Tucano Engine Sandbox (3D) - FPS: " + std::to_string(frames));
            frames = 0;
            fpsTimer = currentTime;
        }

        // Handle window resize logic here eventually (skip for now to focus on base)
        // Note: resizing breaks depth buffer without proper recreation logic. We'll stick to fixed or ignore.
        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
            break;
        }
    }

    renderer.WaitIdle();
    return 0;
}
