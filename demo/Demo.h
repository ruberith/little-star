#pragma once

#include "Engine.h"

// demo application
class Demo
{
  public:
    // application name
    const std::string name{"Little Star"};
    // application version
    const VersionNumber version{.major = 1, .minor = 0, .patch = 0};

  private:
    // game engine
    Engine engine;

  public:
    // Construct the Demo object.
    Demo();
    // Destruct the Demo object.
    ~Demo();
    // Run the demo application.
    int run();
};
