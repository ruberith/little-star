#include "Camera.h"

#include "Engine.h"
#include <glm/gtx/quaternion.hpp>

using namespace glm;

void Camera::update()
{
    pitch = clamp(pitch, pitchMin, pitchMax);
    yaw = std::fmod(yaw, radians(360.0f));
    if (yaw < 0.0f)
    {
        yaw += radians(360.0f);
    }

    up = normalize(engine.player.x);
    ref = normalize(ref - dot(ref, up) * up);

    // Combine the rotation around the player with the rotation around the moon.
    const quat q = normalize(quat_cast(float3x3{ref, up, cross(ref, up)})) * normalize(quat{{pitch, yaw, 0.0f}});

    target = engine.player.x + 2.5f * up;
    distance = clamp(distance, distanceMin, distanceMax);
    const float3 forward = normalize(q * float3{0.0f, 0.0f, 1.0f});
    position = target - distance * forward;

    engine.player.up = up;
    engine.player.forward = normalize(forward - dot(forward, up) * up);
    engine.player.right = cross(engine.player.forward, up);
}

glm::float4x4 Camera::view()
{
    return lookAt(position, target, up);
}

glm::float4x4 Camera::projection(uint32_t width, uint32_t height)
{
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    return perspective(radians(45.0f), aspect, nearPlane, farPlane);
}

void Camera::startAnimation(const std::string& name)
{
    // Backup the third-person camera frame.
    _position = position;
    _target = target;
    _up = up;

    const std::vector<Keyframe>& animation = animations[name];
    position = animation.front().position;
    target = animation.front().target;
    up = animation.front().up;
}

void Camera::animate(const std::string& name)
{
    const std::vector<Keyframe>& animation = animations[name];
    const float time = engine.stateTime;
    if (time >= animation.back().time)
    {
        position = animation.back().position;
        target = animation.back().target;
        up = animation.back().up;
    }
    else
    {
        // Interpolate between the keyframes.
        const auto high =
            std::upper_bound(animation.begin(), animation.end(), time,
                             [](const float time, const Keyframe& keyframe) -> bool { return time < keyframe.time; });
        const auto low = high - 1;
        const float t = (time - low->time) / (high->time - low->time);
        position = lerp(low->position, high->position, t);
        target = lerp(low->target, high->target, t);
        up = normalize(lerp(low->up, high->up, t));
    }
}

void Camera::stopAnimation()
{
    // Restore the third-person camera frame.
    position = _position;
    target = _target;
    up = _up;
}