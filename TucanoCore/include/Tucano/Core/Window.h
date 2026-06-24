#pragma once

#include <string>
#include <cstdint>

struct GLFWwindow;

namespace Tucano {

struct WindowProps {
    std::string Title;
    uint32_t Width;
    uint32_t Height;

    WindowProps(const std::string& title = "Tucano Engine",
                uint32_t width = 1280,
                uint32_t height = 720)
        : Title(title), Width(width), Height(height) {}
};

class Window {
public:
    Window(const WindowProps& props);
    ~Window();

    void Update();
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    bool ShouldClose() const;
    GLFWwindow* GetNativeWindow() const;
    void SetTitle(const std::string& title);

private:
    GLFWwindow* m_Window;

    struct WindowData {
        std::string Title;
        uint32_t Width, Height;
    };

    WindowData m_Data;
};

} // namespace Tucano
