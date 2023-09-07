#pragma once

#include <glm/gtx/compatibility.hpp>
#include <string>
#include <unordered_map>

class Engine;

// third-person camera system with alternative animation mode
struct Camera
{
    // engine parent
    Engine& engine;
    // camera position
    glm::float3 position{};
    // target position
    glm::float3 target{};
    // reference vector
    glm::float3 ref{1.0f, 0.0f, 0.0f};
    // up vector
    glm::float3 up{0.0f, 1.0f, 0.0f};
    // distance between camera and target
    float distance{6.0f}, distanceMin{3.0f}, distanceMax{9.0f};
    // pitch angle
    float pitch{glm::radians(30.0f)}, pitchMin{glm::radians(-20.0f)}, pitchMax{glm::radians(60.0f)};
    // yaw angle
    float yaw{};
    // clip planes
    float nearPlane{0.1f}, farPlane{100.0f};

    // aperture [f]
    static constexpr float aperture{1.4f};
    // shutter speed [s]
    static constexpr float shutterSpeed{1.0f / 500.0f};
    // sensor sensitivity [ISO]
    static constexpr float sensorSensitivity{1600.0f};
    // exposure value at ISO 100
    const float EV100{std::log2f(aperture * aperture / shutterSpeed * 100.0 / sensorSensitivity)};
    // exposure [lx * s]
    const float exposure{1.0f / (1.2f * std::pow(2.0f, EV100))};

    // Update the camera and therefrom the coordinate space of the target player.
    void update();
    // Calculate the lookAt view matrix.
    glm::float4x4 view();
    // Calculate the perspective projection matrix given width and height of the viewport.
    glm::float4x4 projection(uint32_t width, uint32_t height);

    // frame backup
    glm::float3 _position, _target, _up;
    // keyframe
    struct Keyframe
    {
        // point in time [s]
        float time{};
        // camera position
        glm::float3 position{};
        // target position
        glm::float3 target{};
        // up vector
        glm::float3 up{0.0f, 1.0f, 0.0f};
    };
    // name => keyframe animation
    std::unordered_map<std::string, std::vector<Keyframe>> animations{
        {"intro",
         {
             {0.0f, {50.0f, 50.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
             {10.225f, {0.0f, 30.0f, 20.0f}, {0.0f, 25.0f, 0.0f}},
         }},
        {"finale",
         {
             {0.0f, {0.0f, -50.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
             {19.25f, {0.0f, 30.0f, 20.0f}, {0.0f, 25.0f, 0.0f}},
         }},
        {"credits",
         {
             {0.0f, {0.0f, 30.0f, 20.0f}, {0.0f, 25.0f, 0.0f}},
             {152.15f, {0.0f, 30.0f, 80.0f}, {0.0f, 25.0f, 0.0f}},
         }},
    };
    // Start a keyframe animation.
    void startAnimation(const std::string& name);
    // Update the camera frame using the specified animation.
    void animate(const std::string& name);
    // Stop the keyframe animation.
    void stopAnimation();
};