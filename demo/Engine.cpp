#include "Engine.h"

namespace chrono = std::chrono;

Engine::Engine(Demo& demo)
    : demo(demo),
      glfw(*this),
      vulkan(*this),
      gui(*this),
      audio(*this),
      camera{.engine = *this},
      player{.engine = *this}
{
}

Engine::~Engine()
{
}

void Engine::runApplication()
{
    audio.startQueue("intro");
    for (uint32_t i = 0; i < 5; i++)
    {
        audio.extendQueue("main");
    }
    camera.startAnimation("intro");

    time = chrono::steady_clock::now();
    constexpr auto timeStep = chrono::duration_cast<chrono::steady_clock::duration>(
        chrono::duration<float, chrono::seconds::period>(deltaTime));
    uint32_t updateCount;
    while (!glfw.windowShouldClose())
    {
        updateCount = 0;
        while (time < chrono::steady_clock::now() && updateCount < maxUpdateCount)
        {
            // Update the game.
            glfw.pollEvents();
            if (state == State::Intro)
            {
                camera.animate("intro");
                if (stateTime >= 13.5f)
                {
                    camera.stopAnimation();
                    state = State::Main;
                    stateTime = 0.0f;
                }
            }
            else if (state == State::Main)
            {
                camera.update();
                player.move();
                camera.update();
                player.rotate();
                if (vulkan.starProgress > 0.99999f)
                {
                    audio.stopQueue();
                    audio.play("finale");
                    camera.startAnimation("finale");
                    state = State::Finale;
                    stateTime = 0.0f;
                }
            }
            else if (state == State::Finale)
            {
                camera.animate("finale");
                player.rotateToStar();
                if (stateTime >= 24.5f)
                {
                    audio.play("credits");
                    camera.startAnimation("credits");
                    state = State::Credits;
                    stateTime = 0.0f;
                }
            }
            else if (state == State::Credits)
            {
                camera.animate("credits");
            }
            vulkan.sim();
            time += timeStep;
            stateTime += deltaTime;
            updateCount++;
        }

        // Display the game.
        gui.create();
        vulkan.render();

        if (state == State::Credits && stateTime >= 160.0f)
        {
            break;
        }
    }
    vulkan.deviceWaitIdle();
}

void Engine::handleKey(KeyAction keyAction)
{
    if (keyAction == KeyAction::unmapped)
    {
        return;
    }

    if (keyAction == KeyAction::pressEscape)
    {
        if (glfw.cursorLocked)
        {
            glfw.unlockCursor();
        }
        else
        {
            glfw.lockCursor();
        }
        return;
    }

    if (keyAction == KeyAction::pressC)
    {
        vulkan.cel = !vulkan.cel;
        return;
    }

    if (glfw.cursorLocked)
    {
        if (keyAction == KeyAction::pressW)
        {
            player.movesForward = true;
        }
        else if (keyAction == KeyAction::releaseW)
        {
            player.movesForward = false;
        }
        else if (keyAction == KeyAction::pressA)
        {
            player.movesLeft = true;
        }
        else if (keyAction == KeyAction::releaseA)
        {
            player.movesLeft = false;
        }
        else if (keyAction == KeyAction::pressS)
        {
            player.movesBackward = true;
        }
        else if (keyAction == KeyAction::releaseS)
        {
            player.movesBackward = false;
        }
        else if (keyAction == KeyAction::pressD)
        {
            player.movesRight = true;
        }
        else if (keyAction == KeyAction::releaseD)
        {
            player.movesRight = false;
        }
        else if (keyAction == KeyAction::pressShift)
        {
            player.runs = true;
        }
        else if (keyAction == KeyAction::releaseShift)
        {
            player.runs = false;
        }
    }
}

void Engine::handleCursor(const glm::float2& position)
{
    if (!glfw.cursorLocked)
    {
        return;
    }
    if (glfw.firstCursorEventAfterLock)
    {
        glfw._cursorPosition = position;
        glfw.firstCursorEventAfterLock = false;
        return;
    }

    const glm::float2 delta = position - glfw._cursorPosition;
    camera.yaw -= 0.0025f * delta.x;
    camera.pitch += 0.0025f * delta.y;
    glfw._cursorPosition = position;
}

void Engine::handleScroll(float dy)
{
    if (!glfw.cursorLocked)
    {
        return;
    }

    camera.distance -= (dy > 0.0f ? 3.0f : -3.0f);
}