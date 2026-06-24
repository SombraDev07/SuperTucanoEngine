#include "Tucano/Core/Window.h"
#include "Tucano/Core/Logger.h"
#include <GLFW/glfw3.h>

namespace Tucano {

static bool s_GLFWInitialized = false;

Window::Window(const WindowProps& props) {
    m_Data.Title = props.Title;
    m_Data.Width = props.Width;
    m_Data.Height = props.Height;

    TUCANO_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

    if (!s_GLFWInitialized) {
        int success = glfwInit();
        if (!success) {
            TUCANO_CORE_CRITICAL("Could not initialize GLFW!");
        }
        s_GLFWInitialized = true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Disable OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Keep it simple for the first milestone

    m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, &m_Data);
}

Window::~Window() {
    glfwDestroyWindow(m_Window);
}

void Window::Update() {
    glfwPollEvents();
}

uint32_t Window::GetWidth() const { return m_Data.Width; }
uint32_t Window::GetHeight() const { return m_Data.Height; }
bool Window::ShouldClose() const { return glfwWindowShouldClose(m_Window); }
GLFWwindow* Window::GetNativeWindow() const { return m_Window; }

void Window::SetTitle(const std::string& title) {
    m_Data.Title = title;
    glfwSetWindowTitle(m_Window, title.c_str());
}

} // namespace Tucano
