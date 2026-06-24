#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

namespace Tucano {

class Input {
public:
    static void Init(GLFWwindow* window);
    
    static bool IsKeyPressed(int keycode);
    static bool IsMouseButtonPressed(int button);
    static glm::vec2 GetMousePosition();
    static float GetMouseX();
    static float GetMouseY();

    static void SetCursorMode(int mode);

private:
    static GLFWwindow* s_Window;
};

} // namespace Tucano
