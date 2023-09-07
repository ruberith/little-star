#include "Vulkan.h"

#include "Demo.h"
#include "SurfaceMesh.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <set>

#define TINYOBJLOADER_IMPLEMENTATION

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace vk;
using namespace vma;

Vulkan::Vulkan(Engine& engine)
    : engine(engine),
      substepDeltaTime(engine.deltaTime / static_cast<float>(substepCount))
{
    // Initialize the dispatcher.
    DynamicLoader dynamicLoader;
    auto getInstanceProcAddr = dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstanceProcAddr);

    // Create the instance.
    const ApplicationInfo applicationInfo{
        .pApplicationName = engine.demo.name.data(),
        .applicationVersion = engine.demo.version.vulkan(),
        .pEngineName = engine.name.data(),
        .engineVersion = engine.version.vulkan(),
        .apiVersion = VK_API_VERSION_1_2,
    };
    const std::vector<const char*> instanceLayers{
#ifdef DEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
    };
    std::vector instanceExtensions{
#ifdef DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
#endif
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    };
    engine.glfw.addRequiredInstanceExtensions(instanceExtensions);
#ifdef DEBUG
    constexpr std::array validationFeatureEnables{
        ValidationFeatureEnableEXT::eBestPractices,
        ValidationFeatureEnableEXT::eSynchronizationValidation,
    };
    const ValidationFeaturesEXT validationFeatures{
        .enabledValidationFeatureCount = validationFeatureEnables.size(),
        .pEnabledValidationFeatures = validationFeatureEnables.data(),
    };
#endif
    instance = createInstance(InstanceCreateInfo{
#ifdef DEBUG
        .pNext = &validationFeatures,
#endif
        .flags = InstanceCreateFlagBits::eEnumeratePortabilityKHR,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    });
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

#ifdef DEBUG
    // Create the debug messenger.
    debugUtilsMessenger = instance.createDebugUtilsMessengerEXT(DebugUtilsMessengerCreateInfoEXT{
        .messageSeverity =
            DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
            DebugUtilsMessageSeverityFlagBitsEXT::eWarning | DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = DebugUtilsMessageTypeFlagBitsEXT::eGeneral | DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugCallback,
    });
#endif

    // Create the surface.
    surface = engine.glfw.createWindowSurface(instance);

    // Select the physical device.
    std::vector deviceExtensions{
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    PhysicalDeviceDynamicRenderingFeatures deviceDynamicRenderingFeatures{
        .dynamicRendering = true,
    };
    PhysicalDeviceVulkan12Features deviceVulkan12Features{
        .pNext = &deviceDynamicRenderingFeatures,
        .timelineSemaphore = true,
    };
    const PhysicalDeviceFeatures2 deviceFeatures2{
        .pNext = &deviceVulkan12Features,
        .features =
            PhysicalDeviceFeatures{
                .sampleRateShading = true,
                .samplerAnisotropy = true,
            },
    };
    gpu = GPU::select(instance, surface, deviceExtensions);
    gpu.checkPortabilitySubset(deviceExtensions);

    // Create the logical device.
    std::vector<DeviceQueueCreateInfo> deviceQueueCreateInfos;
    const std::set deviceQueueFamilyIndices{
        gpu.graphicsQueueFamilyIndex,
        gpu.computeQueueFamilyIndex,
        gpu.transferQueueFamilyIndex,
        gpu.presentQueueFamilyIndex,
    };
    const float deviceQueuePriority = 1.0f;
    deviceQueueCreateInfos.reserve(deviceQueueFamilyIndices.size());
    for (uint32_t queueFamilyIndex : deviceQueueFamilyIndices)
    {
        deviceQueueCreateInfos.emplace_back(DeviceQueueCreateInfo{
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &deviceQueuePriority,
        });
    }
    device = gpu.device.createDevice(DeviceCreateInfo{
        .pNext = &deviceFeatures2,
        .queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size()),
        .pQueueCreateInfos = deviceQueueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    });
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    // Obtain the queue handles.
    graphicsQueue = device.getQueue(gpu.graphicsQueueFamilyIndex, 0);
    computeQueue = device.getQueue(gpu.computeQueueFamilyIndex, 0);
    transferQueue = device.getQueue(gpu.transferQueueFamilyIndex, 0);
    presentQueue = device.getQueue(gpu.presentQueueFamilyIndex, 0);
    computeQueueDistinct = (gpu.graphicsQueueFamilyIndex != gpu.computeQueueFamilyIndex);
    transferQueueDistinct = (gpu.graphicsQueueFamilyIndex != gpu.transferQueueFamilyIndex);
    presentQueueDistinct = (gpu.graphicsQueueFamilyIndex != gpu.presentQueueFamilyIndex);

    // Create the memory allocator.
    allocator = createAllocator(AllocatorCreateInfo{
        .physicalDevice = gpu.device,
        .device = device,
        .instance = instance,
        .vulkanApiVersion = applicationInfo.apiVersion,
    });

    // Create the command pools and persistent buffers.
    graphicsPool = device.createCommandPool(CommandPoolCreateInfo{
        .flags = CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = gpu.graphicsQueueFamilyIndex,
    });
    transferPool = device.createCommandPool(CommandPoolCreateInfo{
        .flags = CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = gpu.transferQueueFamilyIndex,
    });
    for (uint32_t i = 0; i < frameCount; i++)
    {
        simPools[i] = device.createCommandPool(CommandPoolCreateInfo{
            .queueFamilyIndex = gpu.computeQueueFamilyIndex,
        });
        renderPools[i] = device.createCommandPool(CommandPoolCreateInfo{
            .queueFamilyIndex = gpu.graphicsQueueFamilyIndex,
        });
        simBuffers[i] = device.allocateCommandBuffers(CommandBufferAllocateInfo{
            .commandPool = simPools[i],
            .level = CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        })[0];
        renderBuffers[i] = device.allocateCommandBuffers(CommandBufferAllocateInfo{
            .commandPool = renderPools[i],
            .level = CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        })[0];
    }

    initializeSwapchain();

    // Calculate and reserve uniform sizes.
    alignedMaterialUniformSize = gpu.alignedUniformSize(sizeof(MaterialUniform));
    alignedSkinUniformSize = gpu.alignedUniformSize(sizeof(SkinUniform));
    constUniformBufferSize += alignedSkinUniformSize;
    playerCollisionUniformOffset = varUniformBufferSize;
    varUniformBufferSize += gpu.alignedUniformSize(sizeof(PlayerCollisionUniform));
    attachmentOffset = varUniformBufferSize;
    varUniformBufferSize += gpu.alignedUniformSize(sizeof(attachmentPositions));
    sceneUniformOffset = varUniformBufferSize;
    varUniformBufferSize += gpu.alignedUniformSize(sizeof(SceneUniform));
    cameraTransformUniformOffset = varUniformBufferSize;
    varUniformBufferSize += gpu.alignedUniformSize(sizeof(TransformUniform));
    lightTransformUniformOffset = varUniformBufferSize;
    varUniformBufferSize += gpu.alignedUniformSize(sizeof(TransformUniform));
    viewProjectionUniformOffset = varUniformBufferSize;
    varUniformBufferSize += gpu.alignedUniformSize(sizeof(ViewProjectionUniform));

    generateStarParticles();
    auto particleMesh = SurfaceMesh<uint16_t>::load("particle");

    // Load the models.
    loadModel("astronaut");
    loadModel("moon");
    loadModel("ball", "gltf", {0.0f, 21.0f, 2.0f});
    loadModel("flag", "gltf", {0.0f, 21.0f, -2.0f});
    loadModel("star", "gltf", starPosition);

    auto skyboxVertices = std::to_array<glm::float3>({
        {+1.0f, +1.0f, +1.0f}, // 0
        {+1.0f, -1.0f, +1.0f}, // 1
        {+1.0f, +1.0f, -1.0f}, // 2
        {+1.0f, -1.0f, -1.0f}, // 3
        {-1.0f, +1.0f, +1.0f}, // 4
        {-1.0f, -1.0f, +1.0f}, // 5
        {-1.0f, +1.0f, -1.0f}, // 6
        {-1.0f, -1.0f, -1.0f}, // 7
    });
    auto skyboxIndices = std::to_array<uint16_t>({
        0, 1, 2, /**/ 3, 2, 1, // +X
        6, 5, 4, /**/ 5, 6, 7, // -X
        0, 2, 4, /**/ 6, 4, 2, // +Y
        5, 3, 1, /**/ 3, 5, 7, // -Y
        0, 4, 5, /**/ 5, 1, 0, // +Z
        7, 6, 2, /**/ 2, 3, 7, // -Z
    });

    // Calculate and reserve vertex sizes.
    particleVertexOffset = vertexBufferSize;
    vertexBufferSize += Size::of(particleMesh.positions);
    skyboxVertexOffset = vertexBufferSize;
    vertexBufferSize += sizeof(skyboxVertices);
    whiteVertexOffset = vertexBufferSize;
    vertexBufferSize += maxVertexCount * sizeof(Vertex::color);

    // Calculate and reserve index sizes.
    particleIndexCount = particleMesh.indices.size();
    particleIndexOffset = indexBufferSize;
    indexBufferSize += Size::of(particleMesh.indices);
    skyboxIndexCount = skyboxIndices.size();
    skyboxIndexOffset = indexBufferSize;
    indexBufferSize += sizeof(skyboxIndices);

    // Create samplers.
    linearClampSampler = device.createSampler(gpu.sampler(Filter::eLinear, SamplerAddressMode::eClampToEdge, false));
    linearClampAniSampler = device.createSampler(gpu.sampler(Filter::eLinear, SamplerAddressMode::eClampToEdge, true));
    linearRepeatSampler = device.createSampler(gpu.sampler(Filter::eLinear, SamplerAddressMode::eRepeat, false));
    linearRepeatAniSampler = device.createSampler(gpu.sampler(Filter::eLinear, SamplerAddressMode::eRepeat, true));
    nearestClampSampler = device.createSampler(gpu.sampler(Filter::eNearest, SamplerAddressMode::eClampToEdge, false));
    shadowSampler = device.createSampler(SamplerCreateInfo{
        .magFilter = Filter::eNearest,
        .minFilter = Filter::eNearest,
        .mipmapMode = SamplerMipmapMode::eNearest,
        .addressModeU = SamplerAddressMode::eClampToBorder,
        .addressModeV = SamplerAddressMode::eClampToBorder,
        .addressModeW = SamplerAddressMode::eClampToBorder,
        .compareEnable = true,
        .compareOp = CompareOp::eLess,
        .borderColor = BorderColor::eFloatOpaqueWhite,
    });

    initializeDescriptorPool();

    // Initialize the vertex, index, and uniform buffers.
    setupTransfer();
    vertexBuffer =
        createBuffer(vertexBufferSize, BufferUsageFlagBits::eVertexBuffer | BufferUsageFlagBits::eTransferDst);
    fillBuffer(vertexBuffer, Data::of(particleMesh.positions), particleVertexOffset);
    fillBuffer(vertexBuffer, Data::of(skyboxVertices), skyboxVertexOffset);
    if (maxVertexCount > 0)
    {
        fillBuffer(vertexBuffer, Data::set(maxVertexCount, glm::float4{1.0f, 1.0f, 1.0f, 1.0f}), whiteVertexOffset);
    }

    indexBuffer = createBuffer(indexBufferSize, BufferUsageFlagBits::eIndexBuffer | BufferUsageFlagBits::eTransferDst);
    fillBuffer(indexBuffer, Data::of(particleMesh.indices), particleIndexOffset);
    fillBuffer(indexBuffer, Data::of(skyboxIndices), skyboxIndexOffset);

    constUniformBuffer =
        createBuffer(constUniformBufferSize, BufferUsageFlagBits::eUniformBuffer | BufferUsageFlagBits::eTransferDst);
    fillBuffer(constUniformBuffer, Data::zero(sizeof(SkinUniform)));
    for (AllocatedBuffer& varUniformBuffer : varUniformBuffers)
    {
        varUniformBuffer = createStagingBuffer(varUniformBufferSize,
                                               BufferUsageFlagBits::eUniformBuffer | BufferUsageFlagBits::eTransferSrc);
        mapBuffer(varUniformBuffer);
    }
    playTransfer();

    // Initialize placeholder images.
    setupGraphics();
    whiteImage = initTextureImage(ImageUsageFlagBits::eSampled, {255, 255, 255, 255});
    blueImage = initTextureImage(ImageUsageFlagBits::eSampled, {128, 128, 255, 255});
    playGraphics();

    initializeModels();

    initializeSimulation();

    // Initialize the pipelines.
    initializeStarUpdatePipeline();
    initializeSpatialHashPipeline();
    initializeSpatialSortPipeline();
    initializeSpatialGroupsortPipeline();
    initializeSpatialCollectPipeline();
    initializeXpbdPredictPipeline();
    initializeXpbdObjcollPipeline();
    initializeXpbdPcollPipeline();
    initializeXpbdDistPipeline();
    initializeXpbdVolPipeline();
    initializeXpbdCorrectPipeline();
    initializeDepthPipeline();
    initializeParticleDepthPipeline();
    initializeLightingPipeline();
    initializeParticlePipeline();
    initializeSkyboxPipeline();
    initializePostPipeline();

    // Create the semaphores and fences.
    constexpr SemaphoreTypeCreateInfo simCompleteType{
        .semaphoreType = SemaphoreType::eTimeline,
        .initialValue = 0,
    };
    simComplete = device.createSemaphore(SemaphoreCreateInfo{
        .pNext = &simCompleteType,
    });
    for (uint32_t i = 0; i < frameCount; i++)
    {
        imageAcquired[i] = device.createSemaphore(SemaphoreCreateInfo{});
        renderComplete[i] = device.createSemaphore(SemaphoreCreateInfo{});
        updateInFlight[i] = device.createFence(FenceCreateInfo{
            .flags = FenceCreateFlagBits::eSignaled,
        });
        frameInFlight[i] = device.createFence(FenceCreateInfo{
            .flags = FenceCreateFlagBits::eSignaled,
        });
    }
}

void Vulkan::deviceWaitIdle()
{
    device.waitIdle();
}

Vulkan::~Vulkan()
{
    terminateSwapchain();
    for (Model& model : models)
    {
        destroyModel(model);
    }
    for (Sampler& sampler : {
             std::ref(linearClampSampler),
             std::ref(linearClampAniSampler),
             std::ref(linearRepeatSampler),
             std::ref(linearRepeatAniSampler),
             std::ref(nearestClampSampler),
             std::ref(shadowSampler),
         })
    {
        device.destroySampler(sampler);
    }
    for (AllocatedImage& image : {
             std::ref(brdfImage),
             std::ref(irradianceImage),
             std::ref(radianceImage),
             std::ref(whiteImage),
             std::ref(blueImage),
             std::ref(skyboxImage),
             std::ref(fontImage),
             std::ref(shadowImage),
         })
    {
        destroyImage(image);
    }
    destroyBuffer(storageBuffer);
    unmapBuffer(counterBuffer);
    destroyBuffer(counterBuffer);
    for (AllocatedBuffer& buffer : {
             std::ref(vertexBuffer),
             std::ref(indexBuffer),
         })
    {
        destroyBuffer(buffer);
    }
    destroyBuffer(constUniformBuffer);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        unmapBuffer(varUniformBuffers[i]);
        destroyBuffer(varUniformBuffers[i]);
        if (guiVertexBuffers[i]())
        {
            unmapBuffer(guiVertexBuffers[i]);
            destroyBuffer(guiVertexBuffers[i]);
        }
        if (guiIndexBuffers[i]())
        {
            unmapBuffer(guiIndexBuffers[i]);
            destroyBuffer(guiIndexBuffers[i]);
        }
        device.destroySemaphore(imageAcquired[i]);
        device.destroySemaphore(renderComplete[i]);
        device.destroyFence(updateInFlight[i]);
        device.destroyFence(frameInFlight[i]);
        device.destroyCommandPool(simPools[i]);
        device.destroyCommandPool(renderPools[i]);
    }
    device.destroySemaphore(simComplete);
    for (CommandPool& commandPool : {
             std::ref(graphicsPool),
             std::ref(transferPool),
         })
    {
        device.destroyCommandPool(commandPool);
    }
    device.destroyDescriptorPool(descPool);
    for (Pipeline& pipeline : {
             std::ref(starUpdatePipeline),       std::ref(spatialHashPipeline),    std::ref(spatialSortPipeline),
             std::ref(spatialGroupsortPipeline), std::ref(spatialCollectPipeline), std::ref(xpbdPredictPipeline),
             std::ref(xpbdObjcollPipeline),      std::ref(xpbdPcollPipeline),      std::ref(xpbdDistPipeline),
             std::ref(xpbdVolPipeline),          std::ref(xpbdCorrectPipeline),    std::ref(depthPipeline),
             std::ref(particleDepthPipeline),    std::ref(lightingPipeline),       std::ref(particlePipeline),
             std::ref(skyboxPipeline),           std::ref(postPipeline),           std::ref(guiPipeline),
             std::ref(shadowPipeline),
         })
    {
        device.destroyPipeline(pipeline);
    }
    for (PipelineLayout& pipelineLayout : {
             std::ref(starUpdatePipelineLayout),
             std::ref(spatialHashPipelineLayout),
             std::ref(spatialSortPipelineLayout),
             std::ref(spatialGroupsortPipelineLayout),
             std::ref(spatialCollectPipelineLayout),
             std::ref(xpbdPredictPipelineLayout),
             std::ref(xpbdObjcollPipelineLayout),
             std::ref(xpbdPcollPipelineLayout),
             std::ref(xpbdDistPipelineLayout),
             std::ref(xpbdVolPipelineLayout),
             std::ref(xpbdCorrectPipelineLayout),
             std::ref(depthPipelineLayout),
             std::ref(particleDepthPipelineLayout),
             std::ref(lightingPipelineLayout),
             std::ref(particlePipelineLayout),
             std::ref(skyboxPipelineLayout),
             std::ref(postPipelineLayout),
             std::ref(guiPipelineLayout),
         })
    {
        device.destroyPipelineLayout(pipelineLayout);
    }
    for (DescriptorSetLayout& descLayout : {
             std::ref(starUpdateDescLayout),
             std::ref(spatialHashDescLayout),
             std::ref(spatialSortDescLayout),
             std::ref(spatialGroupsortDescLayout),
             std::ref(spatialCollectDescLayout),
             std::ref(xpbdPredictDescLayout),
             std::ref(xpbdObjcollDescLayout),
             std::ref(xpbdPcollDescLayout),
             std::ref(xpbdDistDescLayout),
             std::ref(xpbdVolDescLayout),
             std::ref(xpbdCorrectDescLayout),
             std::ref(depthDescLayout),
             std::ref(sceneDescLayout),
             std::ref(materialDescLayout),
             std::ref(skinDescLayout),
             std::ref(particleDescLayout),
             std::ref(skyboxDescLayout),
             std::ref(postDescLayout),
             std::ref(guiDescLayout),
         })
    {
        device.destroyDescriptorSetLayout(descLayout);
    }
    for (ShaderModule& shaderModule : shaderModules)
    {
        device.destroyShaderModule(shaderModule);
    }
    allocator.destroy();
    device.destroy();
    instance.destroySurfaceKHR(surface);
#ifdef DEBUG
    instance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger);
#endif
    instance.destroy();
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Vulkan::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                       void* userData)
{
    switch (callbackData->messageIdNumber)
    {
    case static_cast<int32_t>(0xdc18ad6b): // UNASSIGNED-BestPractices-vkAllocateMemory-small-allocation
    case static_cast<int32_t>(0xb3d4346b): // UNASSIGNED-BestPractices-vkBindMemory-small-dedicated-allocation
        return false;
    default:
        std::cout << callbackData->pMessage << std::endl;
        std::cout << std::endl;
        return false;
    }
}