#include "Vulkan.h"

#include "Descriptor.h"

using namespace vk;

vk::DescriptorSetLayout Vulkan::initDescriptorSetLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                                                        uint32_t count)
{
    // Reserve descriptor sets and pool sizes for the specified descriptors.
    descSetCount += count * frameCount;
    for (const DescriptorSetLayoutBinding& binding : bindings)
    {
        auto poolSize = std::find_if(
            descPoolSizes.begin(), descPoolSizes.end(),
            [&binding](const DescriptorPoolSize& poolSize) -> bool { return binding.descriptorType == poolSize.type; });
        if (poolSize != descPoolSizes.end())
        {
            poolSize->descriptorCount += count * frameCount * binding.descriptorCount;
        }
        else
        {
            descPoolSizes.emplace_back(DescriptorPoolSize{
                .type = binding.descriptorType,
                .descriptorCount = count * frameCount * binding.descriptorCount,
            });
        }
    }
    // Create the descriptor set layout.
    return device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    });
}

void Vulkan::initializeDescriptorPool()
{
    descPoolSizes.reserve(11);
    // Initialize the descriptor set layouts.
    starUpdateDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    spatialHashDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    spatialSortDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    spatialGroupsortDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    spatialCollectDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    xpbdPredictDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    xpbdObjcollDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    xpbdPcollDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    xpbdDistDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    xpbdVolDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    xpbdCorrectDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
        DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eCompute,
        },
    });
    depthDescLayout = initDescriptorSetLayout(
        {
            DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eVertex,
            },
            DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eVertex,
            },
        },
        2);
    sceneDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearClampSampler,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearClampSampler,
        },
        DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearClampSampler,
        },
        DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eVertex,
        },
        DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = DescriptorType::eSampledImage,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
        },
        DescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = DescriptorType::eSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &shadowSampler,
        },
        DescriptorSetLayoutBinding{
            .binding = 7,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eVertex,
        },
    });
    materialDescLayout = initDescriptorSetLayout(
        {
            DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eFragment,
            },
            DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eFragment,
            },
            DescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eFragment,
            },
            DescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eFragment,
            },
            DescriptorSetLayoutBinding{
                .binding = 4,
                .descriptorType = DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eFragment,
            },
            DescriptorSetLayoutBinding{
                .binding = 5,
                .descriptorType = DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eFragment,
            },
        },
        materialCount);
    skinDescLayout = initDescriptorSetLayout(
        {
            DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = ShaderStageFlagBits::eVertex,
            },
        },
        skinCount + 1);
    particleDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eVertex,
        },
    });
    skyboxDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearClampSampler,
        },
    });
    postDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearClampSampler,
        },
        DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &nearestClampSampler,
        },
        DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearClampSampler,
        },
    });
    guiDescLayout = initDescriptorSetLayout({
        DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = &linearRepeatSampler,
        },
    });
    // Create the descriptor pool.
    descPool = device.createDescriptorPool(DescriptorPoolCreateInfo{
        .maxSets = descSetCount,
        .poolSizeCount = static_cast<uint32_t>(descPoolSizes.size()),
        .pPoolSizes = descPoolSizes.data(),
    });
    descPoolSizes.clear();
}

vk::DescriptorSet Vulkan::initDescriptorSet(vk::DescriptorSetLayout& layout)
{
    return device.allocateDescriptorSets(DescriptorSetAllocateInfo{
        .descriptorPool = descPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    })[0];
}

std::array<vk::DescriptorSet, Vulkan::frameCount> Vulkan::initDescriptorSets(vk::DescriptorSetLayout& layout)
{
    std::array<vk::DescriptorSet, Vulkan::frameCount> sets;
    for (uint32_t i = 0; i < frameCount; i++)
    {
        sets[i] = initDescriptorSet(layout);
    }
    return sets;
}

void Vulkan::setDescriptor(DescriptorInfo info, vk::DescriptorType type, vk::DescriptorSet& set, uint32_t binding,
                           uint32_t arrayElement)
{
    WriteDescriptorSet descriptorWrite{
        .dstSet = set,
        .dstBinding = binding,
        .dstArrayElement = arrayElement,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = isImageDescriptor(type) ? &info.image : nullptr,
        .pBufferInfo = isBufferDescriptor(type) ? &info.buffer : nullptr,
        .pTexelBufferView = isTexelBufferDescriptor(type) ? &info.texelBuffer : nullptr,
    };
    device.updateDescriptorSets(descriptorWrite, {});
}

void Vulkan::setCombinedImageSampler(const vk::Sampler& sampler, AllocatedImage& image, vk::DescriptorSet& set,
                                     uint32_t binding, uint32_t arrayElement)
{
    setDescriptor({.image = {sampler, image.view, ImageLayout::eShaderReadOnlyOptimal}},
                  DescriptorType::eCombinedImageSampler, set, binding, arrayElement);
}

void Vulkan::setSampledImage(AllocatedImage& image, vk::DescriptorSet& set, uint32_t binding, uint32_t arrayElement)
{
    setDescriptor({.image = {{}, image.view, ImageLayout::eShaderReadOnlyOptimal}}, DescriptorType::eSampledImage, set,
                  binding, arrayElement);
}

void Vulkan::setUniformBuffer(AllocatedBuffer& buffer, vk::DeviceSize offset, vk::DeviceSize range,
                              vk::DescriptorSet& set, uint32_t binding, uint32_t arrayElement)
{
    setDescriptor({.buffer = {buffer(), offset, range}}, DescriptorType::eUniformBuffer, set, binding, arrayElement);
}

void Vulkan::setStorageBuffer(AllocatedBuffer& buffer, vk::DeviceSize offset, vk::DeviceSize range,
                              vk::DescriptorSet& set, uint32_t binding, uint32_t arrayElement)
{
    setDescriptor({.buffer = {buffer(), offset, range}}, DescriptorType::eStorageBuffer, set, binding, arrayElement);
}

void Vulkan::setStorageBuffer(AllocatedBuffer& buffer, vk::DescriptorSet& set, uint32_t binding, uint32_t arrayElement)
{
    setDescriptor({.buffer = {buffer(), 0, buffer.size}}, DescriptorType::eStorageBuffer, set, binding, arrayElement);
}