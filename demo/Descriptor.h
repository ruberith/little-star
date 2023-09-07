#pragma once

#include <vulkan/vulkan.hpp>

// descriptor info
union DescriptorInfo {
    vk::DescriptorImageInfo image;
    vk::DescriptorBufferInfo buffer;
    vk::BufferView texelBuffer;
};

// Return true if the descriptor of the given type is an image descriptor. Return false otherwise.
inline bool isImageDescriptor(vk::DescriptorType descriptorType)
{
    using namespace vk;

    switch (descriptorType)
    {
    case DescriptorType::eSampler:
    case DescriptorType::eCombinedImageSampler:
    case DescriptorType::eSampledImage:
    case DescriptorType::eStorageImage:
    case DescriptorType::eInputAttachment:
        return true;
    default:
        return false;
    }
}

// Return true if the descriptor of the given type is a buffer descriptor. Return false otherwise.
inline bool isBufferDescriptor(vk::DescriptorType descriptorType)
{
    using namespace vk;

    switch (descriptorType)
    {
    case DescriptorType::eUniformBuffer:
    case DescriptorType::eStorageBuffer:
    case DescriptorType::eUniformBufferDynamic:
    case DescriptorType::eStorageBufferDynamic:
        return true;
    default:
        return false;
    }
}

// Return true if the descriptor of the given type is a texel buffer descriptor. Return false otherwise.
inline bool isTexelBufferDescriptor(vk::DescriptorType descriptorType)
{
    using namespace vk;

    switch (descriptorType)
    {
    case DescriptorType::eUniformTexelBuffer:
    case DescriptorType::eStorageTexelBuffer:
        return true;
    default:
        return false;
    }
}