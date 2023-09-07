#include "GLFW.h"

#include "Demo.h"
#include <imgui_impl_glfw.h>

GLFW::GLFW(Engine& engine)
    : engine(engine),
      title(engine.demo.name)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    window = glfwCreateWindow(800, 600, title.data(), {}, {});
    glfwSetWindowUserPointer(window, &engine);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) -> void {
        Vulkan& vulkan = static_cast<Engine*>(glfwGetWindowUserPointer(window))->vulkan;
        vulkan.resizedFramebuffer = true;
    });
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) -> void {
        Engine& engine = *static_cast<Engine*>(glfwGetWindowUserPointer(window));
        engine.handleKey(engine.glfw.keyAction(key, action));
    });
    lockCursor();
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) -> void {
        Engine& engine = *static_cast<Engine*>(glfwGetWindowUserPointer(window));
        engine.handleCursor({x, y});
    });
    glfwSetScrollCallback(window, [](GLFWwindow* window, double dx, double dy) -> void {
        Engine& engine = *static_cast<Engine*>(glfwGetWindowUserPointer(window));
        engine.handleScroll(dy);
    });
}

GLFW::~GLFW()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void GLFW::addRequiredInstanceExtensions(std::vector<const char*>& extensions)
{
    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    extensions.reserve(extensions.size() + glfwExtensionCount);
    for (uint32_t i = 0; i < glfwExtensionCount; i++)
    {
        extensions.emplace_back(glfwExtensions[i]);
    }
}

vk::SurfaceKHR GLFW::createWindowSurface(const vk::Instance& instance)
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, {}, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Vulkan surface for the GLFW window");
    }
    return surface;
}

void GLFW::getFramebufferSize(int& width, int& height)
{
    glfwGetFramebufferSize(window, &width, &height);
}

void GLFW::initializeGuiWindow()
{
    ImGui_ImplGlfw_InitForVulkan(window, true);
}

bool GLFW::windowShouldClose()
{
    return glfwWindowShouldClose(window);
}

void GLFW::pollEvents()
{
    glfwPollEvents();
}

void GLFW::waitEvents()
{
    glfwWaitEvents();
}

KeyAction GLFW::keyAction(int key, int action)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        return KeyAction::pressEscape;
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
        return KeyAction::pressC;
    if (key == GLFW_KEY_W && action == GLFW_PRESS)
        return KeyAction::pressW;
    if (key == GLFW_KEY_W && action == GLFW_RELEASE)
        return KeyAction::releaseW;
    if (key == GLFW_KEY_A && action == GLFW_PRESS)
        return KeyAction::pressA;
    if (key == GLFW_KEY_A && action == GLFW_RELEASE)
        return KeyAction::releaseA;
    if (key == GLFW_KEY_S && action == GLFW_PRESS)
        return KeyAction::pressS;
    if (key == GLFW_KEY_S && action == GLFW_RELEASE)
        return KeyAction::releaseS;
    if (key == GLFW_KEY_D && action == GLFW_PRESS)
        return KeyAction::pressD;
    if (key == GLFW_KEY_D && action == GLFW_RELEASE)
        return KeyAction::releaseD;
    if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
        return KeyAction::pressShift;
    if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE)
        return KeyAction::releaseShift;

    return KeyAction::unmapped;
}

void GLFW::lockCursor()
{
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    cursorLocked = true;
    firstCursorEventAfterLock = true;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    _cursorPosition = {x, y};
}

void GLFW::unlockCursor()
{
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    cursorLocked = false;
}