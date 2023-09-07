#pragma once

#include <glm/gtx/compatibility.hpp>

class Engine;

// player system
struct Player
{
    // engine parent
    Engine& engine;
    // position
    glm::float3 x{0.0f, 20.0f, 0.0f};
    // velocity
    glm::float3 v{};
    // rotation
    glm::quat q{1.0f, 0.0f, 0.0f, 0.0f};
    // right vector
    glm::float3 right{1.0f, 0.0f, 0.0f};
    // up vector
    glm::float3 up{0.0f, 1.0f, 0.0f};
    // forward vector
    glm::float3 forward{0.0f, 0.0f, 1.0f};
    // Does the player move ...?
    bool movesForward{}, movesLeft{}, movesBackward{}, movesRight{};
    // Does the player run?
    bool runs{};

    // Return true if the player is moving. Return false otherwise.
    bool isMoving();
    // Update the player position.
    void move();
    // Update the player rotation.
    void rotate();
    // Update the player rotation s.t. the player looks at the star.
    void rotateToStar();
};