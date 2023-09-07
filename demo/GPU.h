#pragma once

#include <vulkan/vulkan.hpp>

class GLFW;
struct WorkgroupDimensions;

// GPU (physical device)
struct GPU
{
    // physical device name
    std::string name;
    // size of dedicated VRAM
    uint64_t vram{};
    // physical device handle
    vk::PhysicalDevice device;
    // physical device properties
    vk::PhysicalDeviceProperties properties;
    // index of the graphics queue family
    uint32_t graphicsQueueFamilyIndex{-1u};
    // index of the compute queue family
    uint32_t computeQueueFamilyIndex{-1u};
    // index of the transfer queue family
    uint32_t transferQueueFamilyIndex{-1u};
    // index of the presentation queue family
    uint32_t presentQueueFamilyIndex{-1u};
    // supported extensions
    std::vector<const char*> extensions;
    // supported surface capabilities
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    // supported surface formats
    std::vector<vk::SurfaceFormatKHR> surfaceFormats;
    // supported surface presentation modes
    std::vector<vk::PresentModeKHR> surfacePresentModes;

    // Select the most suited GPU supporting the given instance, surface, and required extensions.
    static GPU select(const vk::Instance& instance, const vk::SurfaceKHR& surface,
                      const std::vector<const char*>& requiredExtensions);
    // Update the surface capabilities, formats, and presentation modes for the created swapchain.
    void querySwapchainSupport(const vk::SurfaceKHR& surface);
    // Select the image extent for the swapchain.
    vk::Extent2D selectSwapchainExtent(GLFW& glfw);
    // Select the surface format for the swapchain.
    vk::SurfaceFormatKHR selectSwapchainSurfaceFormat();
    // Select the presentation mode for the swapchain.
    vk::PresentModeKHR selectSwapchainPresentMode();
    // Select the image count for the swapchain.
    uint32_t selectSwapchainImageCount();
    // Select the first supported format in a list of candidates.
    vk::Format selectFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                            vk::FormatFeatureFlags features);
    // Select the format for depth textures.
    vk::Format selectDepthFormat();
    // Select the sample count for MSAA.
    vk::SampleCountFlagBits selectMsaaSampleCount();
    // Add the portability subset extension to the required extensions if it is supported.
    void checkPortabilitySubset(std::vector<const char*>& requiredExtensions);
    // Align the given size with the minimum uniform buffer offset alignment.
    vk::DeviceSize alignedUniformSize(vk::DeviceSize size);
    // Align the given size with the minimum storage buffer offset alignment.
    vk::DeviceSize alignedStorageSize(vk::DeviceSize size);
    // Return the create info for a sampler with the given filter, address mode, and optional anisotropy.
    vk::SamplerCreateInfo sampler(vk::Filter filter = vk::Filter::eLinear,
                                  vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat,
                                  bool anisotropy = true);
    // Select the workgroup dimensions respecting the given invocation count, desired group size,
    // and shared size per invocation.
    WorkgroupDimensions selectWorkgroupDimensions(uint32_t invocationCount, uint32_t desiredGroupSize,
                                                  uint32_t sharedSizePerInvocation);
};
