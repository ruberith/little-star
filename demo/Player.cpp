#include "Player.h"

#include "Engine.h"
#include <glm/gtx/quaternion.hpp>

using namespace glm;

bool Player::isMoving()
{
    return (movesForward ^ movesBackward) || (movesLeft ^ movesRight);
}

void Player::move()
{
    // time step
    static constexpr float dt = Engine::deltaTime;
    static constexpr float dt_sq_g = dt * dt * Engine::gravity;
    // desired radius
    static constexpr float r = 19.93f;

    v = {};
    if (isMoving())
    {
        if (movesForward)
        {
            v += forward;
        }
        if (movesLeft)
        {
            v -= right;
        }
        if (movesBackward)
        {
            v -= forward;
        }
        if (movesRight)
        {
            v += right;
        }
        v = (runs ? 2.0f : 1.0f) * normalize(v);
    }

    // Predict the position at the next time step.
    x += dt * v + dt_sq_g * normalize(x);
    // Determine the penetration of the moon surface.
    const float d = r - length(x);
    if (d > 0.0f)
    {
        // Resolve the penetration.
        const float3 n = normalize(x);
        x += d * n;
    }
}

void Player::rotate()
{
    if (isMoving())
    {
        float3 direction{};
        if (movesForward)
        {
            direction += forward;
        }
        if (movesLeft)
        {
            direction -= right;
        }
        if (movesBackward)
        {
            direction -= forward;
        }
        if (movesRight)
        {
            direction += right;
        }
        // Smoothly interpolate the rotation.
        q = normalize(slerp(q, quatLookAtLH(normalize(direction), up), 4.0f * engine.deltaTime));
    }
}

void Player::rotateToStar()
{
    float3 direction = engine.vulkan.starPosition - x;
    if (abs(direction.x) < 1e-5f && abs(direction.z) < 1e-5f)
    {
        // The player is standing directly under the star, so look away from the camera.
        direction = {0.0f, 0.0f, -1.0f};
    }
    else
    {
        // Reorthogonalize the direction vector w.r.t. the up vector.
        direction = normalize(direction - dot(direction, up) * up);
    }
    // Smoothly interpolate the rotation.
    q = normalize(slerp(q, quatLookAtLH(direction, up), 4.0f * engine.deltaTime));
}