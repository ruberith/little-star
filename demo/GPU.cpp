#include "GPU.h"

#include "GLFW.h"
#include "Utils.h"

using namespace vk;

GPU GPU::select(const vk::Instance& instance, const vk::SurfaceKHR& surface,
                const std::vector<const char*>& requiredExtensions)
{
    std::vector<GPU> gpus;
    std::vector<PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    gpus.reserve(devices.size());
    for (const PhysicalDevice& device : devices)
    {
        GPU gpu;
        gpu.device = device;

        // Check graphics, compute, transfer and presentation support.
        // If any of the queue families is not found, proceed to the next device.
        bool foundGraphicsQueueFamily = false;
        bool foundComputeQueueFamily = false;
        bool foundTransferQueueFamily = false;
        bool foundPresentQueueFamily = false;
        uint32_t queueFamilyIndex = 0;
        std::vector<QueueFamilyProperties> deviceQueueFamilies = device.getQueueFamilyProperties();
        for (const QueueFamilyProperties& queueFamily : deviceQueueFamilies)
        {
            bool hasGraphicsSupport = static_cast<bool>(queueFamily.queueFlags & QueueFlagBits::eGraphics);
            bool hasComputeSupport = static_cast<bool>(queueFamily.queueFlags & QueueFlagBits::eCompute);
            bool hasTransferSupport = static_cast<bool>(queueFamily.queueFlags & QueueFlagBits::eTransfer);
            bool hasPresentSupport = device.getSurfaceSupportKHR(queueFamilyIndex, surface);

            if (!foundGraphicsQueueFamily && hasGraphicsSupport && hasComputeSupport)
            {
                gpu.graphicsQueueFamilyIndex = queueFamilyIndex;
                foundGraphicsQueueFamily = true;
            }
            if (!foundComputeQueueFamily && !hasGraphicsSupport && hasComputeSupport)
            {
                gpu.computeQueueFamilyIndex = queueFamilyIndex;
                foundComputeQueueFamily = true;
            }
            if (!foundTransferQueueFamily && !hasGraphicsSupport && !hasComputeSupport && hasTransferSupport)
            {
                gpu.transferQueueFamilyIndex = queueFamilyIndex;
                foundTransferQueueFamily = true;
            }
            if (!foundPresentQueueFamily && hasPresentSupport)
            {
                gpu.presentQueueFamilyIndex = queueFamilyIndex;
                foundPresentQueueFamily = true;
            }
            if (foundGraphicsQueueFamily && foundComputeQueueFamily && foundTransferQueueFamily &&
                foundPresentQueueFamily)
            {
                break;
            }
            queueFamilyIndex++;
        }
        if (!foundGraphicsQueueFamily || !foundPresentQueueFamily)
        {
            continue;
        }
        if (!foundComputeQueueFamily)
        {
            gpu.computeQueueFamilyIndex = gpu.graphicsQueueFamilyIndex;
        }
        if (!foundTransferQueueFamily)
        {
            gpu.transferQueueFamilyIndex = gpu.graphicsQueueFamilyIndex;
        }

        // Check extension support.
        // If any of the required extensions is not supported, proceed to the next device.
        std::vector<ExtensionProperties> deviceExtensions = device.enumerateDeviceExtensionProperties();
        gpu.extensions.reserve(deviceExtensions.size());
        for (const ExtensionProperties& extension : deviceExtensions)
        {
            gpu.extensions.emplace_back(extension.extensionName);
        }
        bool missingExtensions = false;
        for (const char* extension : requiredExtensions)
        {
            if (std::find_if(gpu.extensions.begin(), gpu.extensions.end(),
                             [extension](const char* gpuExtension) -> bool {
                                 return strcmp(extension, gpuExtension) == 0;
                             }) == gpu.extensions.end())
            {
                missingExtensions = true;
                break;
            }
        }
        if (missingExtensions)
        {
            continue;
        }

        // Check feature support.
        // If any of the required features is not supported, proceed to the next device.
        PhysicalDeviceFeatures deviceFeatures = device.getFeatures();
        PhysicalDeviceDynamicRenderingFeatures deviceDynamicRenderingFeatures{};
        PhysicalDeviceVulkan12Features deviceVulkan12Features{
            .pNext = &deviceDynamicRenderingFeatures,
        };
        PhysicalDeviceFeatures2 deviceFeatures2{
            .pNext = &deviceVulkan12Features,
        };
        device.getFeatures2(&deviceFeatures2);

        if (!(deviceFeatures.samplerAnisotropy && deviceFeatures.sampleRateShading &&
              deviceVulkan12Features.timelineSemaphore && deviceDynamicRenderingFeatures.dynamicRendering))
        {
            continue;
        }

        // Check swapchain support.
        // If no surface formats or presentation modes are supported, proceed to the next device.
        gpu.querySwapchainSupport(surface);
        if (gpu.surfaceFormats.empty() || gpu.surfacePresentModes.empty())
        {
            continue;
        }

        // Query device properties and store the candidate GPU.
        gpu.properties = device.getProperties();
        gpu.name = std::string(gpu.properties.deviceName.begin(), gpu.properties.deviceName.end());
        gpu.vram = 0;
        if (gpu.properties.deviceType == PhysicalDeviceType::eDiscreteGpu)
        {
            PhysicalDeviceMemoryProperties physicalDeviceMemory = device.getMemoryProperties();
            for (uint32_t i = 0; i < physicalDeviceMemory.memoryHeapCount; i++)
            {
                if (physicalDeviceMemory.memoryHeaps[i].flags & MemoryHeapFlagBits::eDeviceLocal)
                {
                    gpu.vram = physicalDeviceMemory.memoryHeaps[i].size;
                    break;
                }
            }
        }
        gpus.emplace_back(gpu);
    }

    if (gpus.empty())
    {
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    // Select the candidate GPU with maximum dedicated VRAM.
    return *std::max_element(gpus.begin(), gpus.end(),
                             [](const GPU& a, const GPU& b) -> bool { return a.vram < b.vram; });
}

void GPU::querySwapchainSupport(const vk::SurfaceKHR& surface)
{
    surfaceCapabilities = device.getSurfaceCapabilitiesKHR(surface);
    surfaceFormats = device.getSurfaceFormatsKHR(surface);
    surfacePresentModes = device.getSurfacePresentModesKHR(surface);
}

vk::Extent2D GPU::selectSwapchainExtent(GLFW& glfw)
{
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        // Retrieve the extent from the surface capabilities.
        return surfaceCapabilities.currentExtent;
    }
    // Retrieve the extent from GLFW.
    int width, height;
    glfw.getFramebufferSize(width, height);
    return Extent2D{
        .width = std::clamp(static_cast<uint32_t>(width), surfaceCapabilities.minImageExtent.width,
                            surfaceCapabilities.maxImageExtent.width),
        .height = std::clamp(static_cast<uint32_t>(height), surfaceCapabilities.minImageExtent.height,
                             surfaceCapabilities.maxImageExtent.height),
    };
}

vk::SurfaceFormatKHR GPU::selectSwapchainSurfaceFormat()
{
    // Select the 8-bit BGRA sRGB surface format.
    for (const SurfaceFormatKHR& surfaceFormat : surfaceFormats)
    {
        if (surfaceFormat.format == Format::eB8G8R8A8Srgb && surfaceFormat.colorSpace == ColorSpaceKHR::eSrgbNonlinear)
        {
            return surfaceFormat;
        }
    }
    // Fall back to the first listed surface format.
    return surfaceFormats[0];
}

vk::PresentModeKHR GPU::selectSwapchainPresentMode()
{
    // Select the mailbox v-sync presentation mode.
    for (const PresentModeKHR& presentMode : surfacePresentModes)
    {
        if (presentMode == PresentModeKHR::eMailbox)
        {
            return presentMode;
        }
    }
    // Fall back to the FIFO v-sync presentation mode.
    return PresentModeKHR::eFifo;
}

uint32_t GPU::selectSwapchainImageCount()
{
    // Select triple buffering.
    if (surfaceCapabilities.minImageCount <= 3 && 3 <= surfaceCapabilities.maxImageCount)
    {
        return 3;
    }
    // Fall back to the minimum image count (+1, if possible).
    // Usually, this means double buffering.
    if (surfaceCapabilities.minImageCount == surfaceCapabilities.maxImageCount)
    {
        return surfaceCapabilities.minImageCount;
    }
    return surfaceCapabilities.minImageCount + 1;
}

vk::Format GPU::selectFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                             vk::FormatFeatureFlags features)
{
    for (const Format& format : candidates)
    {
        FormatProperties formatProperties = device.getFormatProperties(format);
        if ((tiling == ImageTiling::eLinear && (features & formatProperties.linearTilingFeatures) == features) ||
            (tiling == ImageTiling::eOptimal && (features & formatProperties.optimalTilingFeatures) == features))
        {
            return format;
        }
    }
    throw std::runtime_error("Failed to find a supported format");
}

vk::Format GPU::selectDepthFormat()
{
    return selectFormat({Format::eD32Sfloat, Format::eD32SfloatS8Uint}, ImageTiling::eOptimal,
                        FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::SampleCountFlagBits GPU::selectMsaaSampleCount()
{
    // Select up to MSAAx4 if supported.
    SampleCountFlags sampleCounts =
        properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    return (sampleCounts & SampleCountFlagBits::e4)   ? SampleCountFlagBits::e4
           : (sampleCounts & SampleCountFlagBits::e2) ? SampleCountFlagBits::e2
                                                      : SampleCountFlagBits::e1;
}

void GPU::checkPortabilitySubset(std::vector<const char*>& requiredExtensions)
{
    if (std::find_if(extensions.begin(), extensions.end(), [](const char* gpuExtension) -> bool {
            return strcmp("VK_KHR_portability_subset", gpuExtension) == 0;
        }) != extensions.end())
    {
        requiredExtensions.emplace_back("VK_KHR_portability_subset");
    }
}

vk::DeviceSize GPU::alignedUniformSize(vk::DeviceSize size)
{
    return alignedSize(size, properties.limits.minUniformBufferOffsetAlignment);
}

vk::DeviceSize GPU::alignedStorageSize(vk::DeviceSize size)
{
    return alignedSize(size, properties.limits.minStorageBufferOffsetAlignment);
}

vk::SamplerCreateInfo GPU::sampler(vk::Filter filter, vk::SamplerAddressMode addressMode, bool anisotropy)
{
    return SamplerCreateInfo{
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = (filter == Filter::eLinear) ? SamplerMipmapMode::eLinear : SamplerMipmapMode::eNearest,
        .addressModeU = addressMode,
        .addressModeV = addressMode,
        .addressModeW = addressMode,
        .anisotropyEnable = anisotropy,
        .maxAnisotropy = anisotropy ? properties.limits.maxSamplerAnisotropy : 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
}

WorkgroupDimensions GPU::selectWorkgroupDimensions(uint32_t invocationCount, uint32_t desiredGroupSize,
                                                   uint32_t sharedSizePerInvocation)
{
    const uint32_t maxSharedSize = properties.limits.maxComputeSharedMemorySize;
    const uint32_t maxGroupCount = properties.limits.maxComputeWorkGroupCount[0];
    const uint32_t maxGroupSize =
        std::min(properties.limits.maxComputeWorkGroupInvocations, properties.limits.maxComputeWorkGroupSize[0]);

    WorkgroupDimensions workgroup{};
    // Select the desired or maximum group size and the group count required to include all invocations.
    workgroup.size = std::bit_floor((desiredGroupSize != -1) ? desiredGroupSize : maxGroupSize);
    workgroup.count = invocationCount / workgroup.size + (invocationCount % workgroup.size != 0);
    // Either decrease or increase the group size until the bounds are satisfied.
    bool decreaseGroupSize = sharedSizePerInvocation * workgroup.size > maxSharedSize || workgroup.size > maxGroupSize;
    bool increaseGroupSize = workgroup.count > maxGroupCount;
    if (decreaseGroupSize && increaseGroupSize)
    {
        throw std::runtime_error("Failed to find supported workgroup dimensions");
    }
    while (decreaseGroupSize || increaseGroupSize)
    {
        if (decreaseGroupSize)
        {
            workgroup.size /= 2;
        }
        else if (increaseGroupSize)
        {
            workgroup.size *= 2;
        }
        if (workgroup.size == 0 || workgroup.size == -1)
        {
            throw std::runtime_error("Failed to find supported workgroup dimensions");
        }
        workgroup.count = invocationCount / workgroup.size + (invocationCount % workgroup.size != 0);

        bool _decreaseGroupSize = decreaseGroupSize;
        bool _increaseGroupSize = increaseGroupSize;
        decreaseGroupSize = sharedSizePerInvocation * workgroup.size > maxSharedSize || workgroup.size > maxGroupSize;
        increaseGroupSize = workgroup.count > maxGroupCount;
        if (_decreaseGroupSize && increaseGroupSize ||
            _increaseGroupSize && decreaseGroupSize) // prevent infinite loops
        {
            throw std::runtime_error("Failed to find supported workgroup dimensions");
        }
    }

    return workgroup;
}