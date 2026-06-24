#include "Tucano/Core/Logger.h"
#include "Tucano/Core/Window.h"
#include "Tucano/Core/Input.h"
#include "Tucano/Render/VulkanContext.h"
#include "Tucano/Render/Renderer.h"
#include "Tucano/Camera/Camera.h"
#include "Tucano/Camera/FPSCameraController.h"
#include "Tucano/World/World.h"
#include "Tucano/Mesh/Mesh.h"

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

    renderer.InitPipeline("assets/shaders/basic_3d.vert", "assets/shaders/basic_3d.frag");

    World world;
    
    // Create Meshes
    auto cubeMesh = Mesh::CreateCube(&vulkanContext);
    auto planeMesh = Mesh::CreatePlane(&vulkanContext);

    // Add Ground
    RenderableObject ground(planeMesh);
    ground.Transform.Scale = glm::vec3(10.0f, 1.0f, 10.0f);
    ground.Transform.Position = glm::vec3(0.0f, -1.0f, 0.0f);
    world.AddObject(ground);

    // Add some cubes
    for (int x = -2; x <= 2; x += 2) {
        for (int z = -2; z <= 2; z += 2) {
            RenderableObject cube(cubeMesh);
            cube.Transform.Position = glm::vec3(x, 0.0f, z);
            world.AddObject(cube);
        }
    }

    Camera camera(45.0f, (float)window.GetWidth() / (float)window.GetHeight(), 0.1f, 100.0f);
    FPSCameraController cameraController(&camera);
    cameraController.SetPosition(glm::vec3(0.0f, 2.0f, 10.0f));

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
        for (size_t i = 1; i < world.GetObjects().size(); i++) {
            auto& obj = world.GetObjects()[i];
            obj.Transform.Rotate(glm::vec3(0.0f, 1.0f * deltaTime, 0.0f));
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
