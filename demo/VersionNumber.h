#pragma once

#include <vulkan/vulkan_core.h>

// version number
struct VersionNumber
{
    // variant version
    uint32_t variant{};
    // major version
    uint32_t major{};
    // minor version
    uint32_t minor{};
    // patch version
    uint32_t patch{};

    // Return the Vulkan version number.
    uint32_t vulkan() const
    {
        return VK_MAKE_API_VERSION(variant, major, minor, patch);
    }
};