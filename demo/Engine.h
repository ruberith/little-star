#pragma once

#include "Audio.h"
#include "Camera.h"
#include "GLFW.h"
#include "GUI.h"
#include "Player.h"
#include "Vulkan.h"

class Demo;

// game engine
class Engine
{
    friend GLFW;
    friend Vulkan;
    friend GUI;
    friend Audio;
    friend Camera;
    friend Player;

  private:
    // demo parent
    Demo& demo;
    // GLFW instance
    GLFW glfw;
    // Vulkan instance
    Vulkan vulkan;
    // GUI instance
    GUI gui;
    // audio instance
    Audio audio;
    // camera instance
    Camera camera;
    // player instance
    Player player;
    // delta time / time step
    static constexpr float deltaTime{1.0f / 60.0f};
    // maximum count of consecutive game updates
    static constexpr uint32_t maxUpdateCount{5};
    // moon gravity
    static constexpr float gravity{-1.625f};
    // game time
    std::chrono::time_point<std::chrono::steady_clock> time;
    // game state
    enum struct State
    {
        Intro,
        Main,
        Finale,
        Credits,
    };
    // active game state
    State state{State::Intro};
    // elapsed seconds in current game state
    float stateTime{};

    // Handle a key event given the key action.
    void handleKey(KeyAction keyAction);
    // Handle a cursor event given the position.
    void handleCursor(const glm::float2& position);
    // Handle a scroll event given the vertical delta.
    void handleScroll(float dy);

  public:
    // engine name
    const std::string name{"Engine"};
    // engine version
    const VersionNumber version{.major = 1, .minor = 0, .patch = 0};

    // Construct the Engine object given the demo.
    Engine(Demo& demo);
    // Destruct the Engine object.
    ~Engine();
    // Run the application.
    void runApplication();
};