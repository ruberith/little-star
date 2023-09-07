#include "Demo.h"

#include <iostream>

Demo::Demo() : engine(*this)
{
}

Demo::~Demo() = default;

int Demo::run()
{
    try
    {
        engine.runApplication();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
