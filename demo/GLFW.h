#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on
#include <glm/gtx/compatibility.hpp>

class Engine;

// key action
enum struct KeyAction
{
    unmapped,
    pressEscape,
    pressC,
    pressW,
    releaseW,
    pressA,
    releaseA,
    pressS,
    releaseS,
    pressD,
    releaseD,
    pressShift,
    releaseShift,
};

// window/input/event system
class GLFW
{
  private:
    // engine parent
    Engine& engine;
    // main window
    GLFWwindow* window{};
    // window title
    std::string title;

  public:
    // Is the cursor hidden and locked to the center of the window?
    bool cursorLocked{};
    // Is the next cursor event the first after the cursor has been locked?
    bool firstCursorEventAfterLock{};
    // previous cursor position
    glm::float2 _cursorPosition;

    // Construct the GLFW object given the engine.
    GLFW(Engine& engine);
    // Destruct the GLFW object.
    ~GLFW();
    // Add the Vulkan instance extensions required by GLFW to the given extensions.
    void addRequiredInstanceExtensions(std::vector<const char*>& extensions);
    // Create a surface for the main window and the given Vulkan instance.
    vk::SurfaceKHR createWindowSurface(const vk::Instance& instance);
    // Get width and height of the framebuffer.
    void getFramebufferSize(int& width, int& height);
    // Initialize the GUI window.
    void initializeGuiWindow();
    // Return true if the close flag of the window is set. Return false otherwise.
    bool windowShouldClose();
    // Process all pending events in the queue.
    void pollEvents();
    // Wait for and process events.
    void waitEvents();
    // Generate a combined key action from the given individual tokens.
    KeyAction keyAction(int key, int action);
    // Hide the cursor and lock it to the center of the window.
    void lockCursor();
    // Show and unlock the cursor.
    void unlockCursor();
};