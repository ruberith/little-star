#include "Vulkan.h"

#include "GUI.h"
#include "Vertex.h"

using namespace vk;

std::vector<vk::PipelineShaderStageCreateInfo> Vulkan::initializeShaders(std::vector<Shader>& shaders)
{
    std::vector<PipelineShaderStageCreateInfo> shaderStages;
    shaderModules.reserve(shaderModules.size() + shaders.size());
    shaderStages.reserve(shaders.size());
    for (Shader& shader : shaders)
    {
        shaderCompiler.compile(shader);
        shader.load();
        shaderModules.emplace_back(device.createShaderModule(ShaderModuleCreateInfo{
            .codeSize = shader.code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
        }));
        shaderStages.emplace_back(PipelineShaderStageCreateInfo{
            .stage = shader.stageBit(),
            .module = shaderModules.back(),
            .pName = "main",
        });
    }
    return shaderStages;
}

void Vulkan::initializeStarUpdatePipeline()
{
    std::vector shaders{
        Shader{
            .name = "star-update",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", starWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(glm::float4) + sizeof(glm::uint),
    };
    starUpdatePipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &starUpdateDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, starUpdatePipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                                .stage = shaderStage,
                                                                                .layout = starUpdatePipelineLayout,
                                                                            });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create star update pipeline");
    }

    starUpdateDescSets = initDescriptorSets(starUpdateDescLayout);
    for (DescriptorSet& set : starUpdateDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.x, starParticleCount * sizeof(glm::float4), set, 0);
        setStorageBuffer(storageBuffer, storage.offset.v, starParticleCount * sizeof(glm::float4), set, 1);
        setStorageBuffer(storageBuffer, storage.offset.state, starParticleCount * sizeof(glm::uint), set, 2);
        setStorageBuffer(counterBuffer, 0, counterBufferSize, set, 3);
    }
}

void Vulkan::initializeSpatialHashPipeline()
{
    std::vector shaders{
        Shader{
            .name = "spatial-hash",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", particleWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = 3 * sizeof(float) + sizeof(glm::uint),
    };
    spatialHashPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &spatialHashDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, spatialHashPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                                 .stage = shaderStage,
                                                                                 .layout = spatialHashPipelineLayout,
                                                                             });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create spatial hash pipeline");
    }

    spatialHashDescSets = initDescriptorSets(spatialHashDescLayout);
    for (DescriptorSet& set : spatialHashDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.x, storage.size.x, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.v, storage.size.v, set, 1);
        setStorageBuffer(storageBuffer, storage.offset.spat, storage.size.spat, set, 2);
    }
}

void Vulkan::initializeSpatialSortPipeline()
{
    std::vector shaders{
        Shader{
            .name = "spatial-sort",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", spatWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = 2 * sizeof(glm::uint),
    };
    spatialSortPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &spatialSortDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, spatialSortPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                                 .stage = shaderStage,
                                                                                 .layout = spatialSortPipelineLayout,
                                                                             });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create spatial sort pipeline");
    }

    spatialSortDescSets = initDescriptorSets(spatialSortDescLayout);
    for (DescriptorSet& set : spatialSortDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.spat, storage.size.spat, set, 0);
    }
}

void Vulkan::initializeSpatialGroupsortPipeline()
{
    std::vector shaders{
        Shader{
            .name = "spatial-groupsort",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", sharedSpatWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = 2 * sizeof(glm::uint),
    };
    spatialGroupsortPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &spatialGroupsortDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, spatialGroupsortPipeline) =
        device.createComputePipeline({}, ComputePipelineCreateInfo{
                                             .stage = shaderStage,
                                             .layout = spatialGroupsortPipelineLayout,
                                         });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create spatial groupsort pipeline");
    }

    spatialGroupsortDescSets = initDescriptorSets(spatialGroupsortDescLayout);
    for (DescriptorSet& set : spatialGroupsortDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.spat, storage.size.spat, set, 0);
    }
}

void Vulkan::initializeSpatialCollectPipeline()
{
    std::vector shaders{
        Shader{
            .name = "spatial-collect",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", particleWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(glm::uint),
    };
    spatialCollectPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &spatialCollectDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, spatialCollectPipeline) =
        device.createComputePipeline({}, ComputePipelineCreateInfo{
                                             .stage = shaderStage,
                                             .layout = spatialCollectPipelineLayout,
                                         });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create spatial collect pipeline");
    }

    spatialCollectDescSets = initDescriptorSets(spatialCollectDescLayout);
    for (DescriptorSet& set : spatialCollectDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.spat, storage.size.spat, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.cell, storage.size.cell, set, 1);
    }
}

void Vulkan::initializeXpbdPredictPipeline()
{
    std::vector shaders{
        Shader{
            .name = "xpbd-predict",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", particleWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = 2 * sizeof(float) + sizeof(glm::uint),
    };
    xpbdPredictPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &xpbdPredictDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, xpbdPredictPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                                 .stage = shaderStage,
                                                                                 .layout = xpbdPredictPipelineLayout,
                                                                             });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create XPBD predict pipeline");
    }

    xpbdPredictDescSets = initDescriptorSets(xpbdPredictDescLayout);
    for (DescriptorSet& set : xpbdPredictDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.x, storage.size.x, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.v, storage.size.v, set, 1);
        setStorageBuffer(storageBuffer, storage.offset.x_, storage.size.x_, set, 2);
    }
}

void Vulkan::initializeXpbdObjcollPipeline()
{
    std::vector shaders{
        Shader{
            .name = "xpbd-objcoll",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", particleWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(glm::uint),
    };
    xpbdObjcollPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &xpbdObjcollDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, xpbdObjcollPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                                 .stage = shaderStage,
                                                                                 .layout = xpbdObjcollPipelineLayout,
                                                                             });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create XPBD object collide pipeline");
    }

    xpbdObjcollDescSets = initDescriptorSets(xpbdObjcollDescLayout);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        setUniformBuffer(varUniformBuffers[i], playerCollisionUniformOffset, sizeof(PlayerCollisionUniform),
                         xpbdObjcollDescSets[i], 0);
        setStorageBuffer(storageBuffer, storage.offset.x_, storage.size.x_, xpbdObjcollDescSets[i], 1);
        setStorageBuffer(storageBuffer, storage.offset.r, storage.size.r, xpbdObjcollDescSets[i], 2);
        setStorageBuffer(storageBuffer, storage.offset.dx, storage.size.dx, xpbdObjcollDescSets[i], 3);
    }
}

void Vulkan::initializeXpbdPcollPipeline()
{
    std::vector shaders{
        Shader{
            .name = "xpbd-pcoll",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", particleWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(glm::uint),
    };
    xpbdPcollPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &xpbdPcollDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, xpbdPcollPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                               .stage = shaderStage,
                                                                               .layout = xpbdPcollPipelineLayout,
                                                                           });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create XPBD particle collide pipeline");
    }

    xpbdPcollDescSets = initDescriptorSets(xpbdPcollDescLayout);
    for (DescriptorSet& set : xpbdPcollDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.x_, storage.size.x_, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.spat, storage.size.spat, set, 1);
        setStorageBuffer(storageBuffer, storage.offset.cell, storage.size.cell, set, 2);
        setStorageBuffer(storageBuffer, storage.offset.r, storage.size.r, set, 3);
        setStorageBuffer(storageBuffer, storage.offset.w, storage.size.w, set, 4);
        setStorageBuffer(storageBuffer, storage.offset.state, storage.size.state, set, 5);
        setStorageBuffer(storageBuffer, storage.offset.dx, storage.size.dx, set, 6);
    }
}

void Vulkan::initializeXpbdDistPipeline()
{
    std::vector shaders{
        Shader{
            .name = "xpbd-dist",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", distWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(float) + sizeof(glm::uint),
    };
    xpbdDistPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &xpbdDistDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, xpbdDistPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                              .stage = shaderStage,
                                                                              .layout = xpbdDistPipelineLayout,
                                                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create XPBD distance constrain pipeline");
    }

    xpbdDistDescSets = initDescriptorSets(xpbdDistDescLayout);
    for (DescriptorSet& set : xpbdDistDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.distConstr, storage.size.distConstr, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.x_, storage.size.x_, set, 1);
        setStorageBuffer(storageBuffer, storage.offset.w, storage.size.w, set, 2);
        setStorageBuffer(storageBuffer, storage.offset.dxE7, storage.size.dxE7, set, 3);
    }
}

void Vulkan::initializeXpbdVolPipeline()
{
    std::vector shaders{
        Shader{
            .name = "xpbd-vol",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", volWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(float) + sizeof(glm::uint),
    };
    xpbdVolPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &xpbdVolDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, xpbdVolPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                             .stage = shaderStage,
                                                                             .layout = xpbdVolPipelineLayout,
                                                                         });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create XPBD volume constrain pipeline");
    }

    xpbdVolDescSets = initDescriptorSets(xpbdVolDescLayout);
    for (DescriptorSet& set : xpbdVolDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.volConstr, storage.size.volConstr, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.x_, storage.size.x_, set, 1);
        setStorageBuffer(storageBuffer, storage.offset.w, storage.size.w, set, 2);
        setStorageBuffer(storageBuffer, storage.offset.dxE7, storage.size.dxE7, set, 3);
    }
}

void Vulkan::initializeXpbdCorrectPipeline()
{
    std::vector shaders{
        Shader{
            .name = "xpbd-correct",
            .stage = ShaderStage::Compute,
            .macros = {Shader::macro("g_n", particleWorkgroup.size)},
        },
    };
    auto shaderStage = initializeShaders(shaders)[0];

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(float) + sizeof(glm::uint),
    };
    xpbdCorrectPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &xpbdCorrectDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    std::tie(result, xpbdCorrectPipeline) = device.createComputePipeline({}, ComputePipelineCreateInfo{
                                                                                 .stage = shaderStage,
                                                                                 .layout = xpbdCorrectPipelineLayout,
                                                                             });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create XPBD correct pipeline");
    }

    xpbdCorrectDescSets = initDescriptorSets(xpbdCorrectDescLayout);
    for (DescriptorSet& set : xpbdCorrectDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.x_, storage.size.x_, set, 0);
        setStorageBuffer(storageBuffer, storage.offset.dx, storage.size.dx, set, 1);
        setStorageBuffer(storageBuffer, storage.offset.dxE7, storage.size.dxE7, set, 2);
        setStorageBuffer(storageBuffer, storage.offset.state, storage.size.state, set, 3);
        setStorageBuffer(storageBuffer, storage.offset.x, storage.size.x, set, 4);
        setStorageBuffer(storageBuffer, storage.offset.v, storage.size.v, set, 5);
    }
}

void Vulkan::initializeDepthPipeline()
{
    std::vector shaders{
        Shader{
            .name = "depth",
            .stage = ShaderStage::Vertex,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr std::array vertexBindingDescriptions{
        VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex::position),
            .inputRate = VertexInputRate::eVertex,
        },
        VertexInputBindingDescription{
            .binding = 1,
            .stride = sizeof(Vertex::joints),
            .inputRate = VertexInputRate::eVertex,
        },
        VertexInputBindingDescription{
            .binding = 2,
            .stride = sizeof(Vertex::weights),
            .inputRate = VertexInputRate::eVertex,
        },
    };
    constexpr std::array vertexAttributeDescriptions{
        VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = Format::eR32G32B32Sfloat,
            .offset = 0,
        },
        VertexInputAttributeDescription{
            .location = 1,
            .binding = 1,
            .format = Format::eR32G32B32A32Uint,
            .offset = 0,
        },
        VertexInputAttributeDescription{
            .location = 2,
            .binding = 2,
            .format = Format::eR32G32B32A32Sfloat,
            .offset = 0,
        },
    };
    const PipelineVertexInputStateCreateInfo vertexInputState{
        .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eBack,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    PipelineMultisampleStateCreateInfo multisampleState{
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = true,
        .minSampleShading = 0.25f,
    };

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = CompareOp::eLess,
    };

    const PipelineColorBlendStateCreateInfo colorBlendState{};

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    const std::array setLayouts{
        depthDescLayout,
        skinDescLayout,
    };
    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(glm::float4x4) + sizeof(glm::uint),
    };
    depthPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = setLayouts.size(),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    const PipelineRenderingCreateInfo rendering{
        .depthAttachmentFormat = depthFormat,
    };
    std::tie(result, depthPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = depthPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create depth pipeline");
    }

    multisampleState = {};
    std::tie(result, shadowPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = depthPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create shadow pipeline");
    }

    const DeviceSize positionsOffset = storage.offset.x + starParticleCount * sizeof(glm::float4);
    const DeviceSize positionsSize = (particleCount - starParticleCount) * sizeof(glm::float4);

    depthDescSets = initDescriptorSets(depthDescLayout);
    shadowDescSets = initDescriptorSets(depthDescLayout);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        setUniformBuffer(varUniformBuffers[i], cameraTransformUniformOffset, sizeof(TransformUniform), depthDescSets[i],
                         0);
        setUniformBuffer(varUniformBuffers[i], lightTransformUniformOffset, sizeof(TransformUniform), shadowDescSets[i],
                         0);
        setStorageBuffer(storageBuffer, positionsOffset, positionsSize, depthDescSets[i], 1);
        setStorageBuffer(storageBuffer, positionsOffset, positionsSize, shadowDescSets[i], 1);
    }
}

void Vulkan::initializeParticleDepthPipeline()
{
    std::vector shaders{
        Shader{
            .name = "particle",
            .stage = ShaderStage::Vertex,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr std::array vertexBindingDescriptions{
        VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex::position),
            .inputRate = VertexInputRate::eVertex,
        },
    };
    constexpr std::array vertexAttributeDescriptions{
        VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = Format::eR32G32B32Sfloat,
            .offset = 0,
        },
    };
    const PipelineVertexInputStateCreateInfo vertexInputState{
        .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eBack,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    PipelineMultisampleStateCreateInfo multisampleState{
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = true,
        .minSampleShading = 0.25f,
    };

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = CompareOp::eLess,
    };

    const PipelineColorBlendStateCreateInfo colorBlendState{};

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(glm::float4x4) + sizeof(float),
    };
    particleDepthPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &particleDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    const PipelineRenderingCreateInfo rendering{
        .depthAttachmentFormat = depthFormat,
    };
    std::tie(result, particleDepthPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = particleDepthPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create particle depth pipeline");
    }
}

void Vulkan::initializeLightingPipeline()
{
    std::vector shaders{
        Shader{
            .name = "lighting",
            .stage = ShaderStage::Vertex,
        },
        Shader{
            .name = "lighting",
            .stage = ShaderStage::Pixel,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr std::array vertexBindingDescriptions = Vertex::bindingDescriptions;
    constexpr std::array vertexAttributeDescriptions = Vertex::attributeDescriptions;
    const PipelineVertexInputStateCreateInfo vertexInputState{
        .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    constexpr PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eBack,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    const PipelineMultisampleStateCreateInfo multisampleState{
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = true,
        .minSampleShading = 0.25f,
    };

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{
        .depthTestEnable = true,
        .depthCompareOp = CompareOp::eLessOrEqual,
    };

    constexpr std::array colorBlendAttachments{
        PipelineColorBlendAttachmentState{
            .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                              ColorComponentFlagBits::eA,
        },
        PipelineColorBlendAttachmentState{
            .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                              ColorComponentFlagBits::eA,
        },
    };
    const PipelineColorBlendStateCreateInfo colorBlendState{
        .attachmentCount = colorBlendAttachments.size(),
        .pAttachments = colorBlendAttachments.data(),
    };

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    const std::array setLayouts{
        sceneDescLayout,
        materialDescLayout,
        skinDescLayout,
    };
    constexpr std::array pushConstantRanges{
        PushConstantRange{
            .stageFlags = ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(glm::float4x4) + sizeof(glm::uint),
        },
        PushConstantRange{
            .stageFlags = ShaderStageFlagBits::eFragment,
            .offset = sizeof(glm::float4x4),
            .size = sizeof(glm::uint),
        },
    };
    lightingPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = setLayouts.size(),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = pushConstantRanges.size(),
        .pPushConstantRanges = pushConstantRanges.data(),
    });

    const std::array colorAttachmentFormats{
        colorImage.format,
        normalImage.format,
    };
    const PipelineRenderingCreateInfo rendering{
        .colorAttachmentCount = colorAttachmentFormats.size(),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),
        .depthAttachmentFormat = depthImage.format,
    };
    std::tie(result, lightingPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = lightingPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create lighting pipeline");
    }

    setupGraphics();
    brdfImage =
        initTextureImage(ImageUsageFlagBits::eSampled, ImageData::loadKhronosTexture("brdf")[0][0], ColorSpace::linear);
    irradianceImage = initTextureCubemap(ImageUsageFlagBits::eSampled, ImageData::loadKhronosTexture("irradiance"));
    radianceImage = initTextureCubemap(ImageUsageFlagBits::eSampled, ImageData::loadKhronosTexture("radiance"));
    shadowImage =
        createImage(ImageViewType::e2D, shadowRes, shadowRes, 1, 1, SampleCountFlagBits::e1, depthFormat,
                    ImageTiling::eOptimal, ImageUsageFlagBits::eDepthStencilAttachment | ImageUsageFlagBits::eSampled);
    transitionImageLayout(shadowImage, ImageLayout::eUndefined, ImageLayout::eDepthStencilAttachmentOptimal);
    playGraphics();

    sceneDescSets = initDescriptorSets(sceneDescLayout);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        setUniformBuffer(varUniformBuffers[i], sceneUniformOffset, sizeof(SceneUniform), sceneDescSets[i], 0);
        setCombinedImageSampler({}, brdfImage, sceneDescSets[i], 1);
        setCombinedImageSampler({}, irradianceImage, sceneDescSets[i], 2);
        setCombinedImageSampler({}, radianceImage, sceneDescSets[i], 3);
        setUniformBuffer(varUniformBuffers[i], viewProjectionUniformOffset, sizeof(ViewProjectionUniform),
                         sceneDescSets[i], 4);
        setSampledImage(shadowImage, sceneDescSets[i], 5);
        setStorageBuffer(storageBuffer, storage.offset.x + starParticleCount * sizeof(glm::float4),
                         (particleCount - starParticleCount) * sizeof(glm::float4), sceneDescSets[i], 7);
    }

    inactiveSkinDescSets = initDescriptorSets(skinDescLayout);
    for (DescriptorSet& set : inactiveSkinDescSets)
    {
        setUniformBuffer(constUniformBuffer, 0, sizeof(SkinUniform), set, 0);
    }
}

void Vulkan::initializeParticlePipeline()
{
    std::vector shaders{
        Shader{
            .name = "particle",
            .stage = ShaderStage::Vertex,
        },
        Shader{
            .name = "particle",
            .stage = ShaderStage::Pixel,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr std::array vertexBindingDescriptions{
        VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex::position),
            .inputRate = VertexInputRate::eVertex,
        },
    };
    constexpr std::array vertexAttributeDescriptions{
        VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = Format::eR32G32B32Sfloat,
            .offset = 0,
        },
    };
    const PipelineVertexInputStateCreateInfo vertexInputState{
        .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    constexpr PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eBack,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    const PipelineMultisampleStateCreateInfo multisampleState{
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = true,
        .minSampleShading = 0.25f,
    };

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{
        .depthTestEnable = true,
        .depthCompareOp = CompareOp::eLessOrEqual,
    };

    constexpr std::array colorBlendAttachments{
        PipelineColorBlendAttachmentState{
            .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                              ColorComponentFlagBits::eA,
        },
        PipelineColorBlendAttachmentState{
            .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                              ColorComponentFlagBits::eA,
        },
    };
    const PipelineColorBlendStateCreateInfo colorBlendState{
        .attachmentCount = colorBlendAttachments.size(),
        .pAttachments = colorBlendAttachments.data(),
    };

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    constexpr std::array pushConstantRanges{
        PushConstantRange{
            .stageFlags = ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(glm::float4x4) + sizeof(float),
        },
        PushConstantRange{
            .stageFlags = ShaderStageFlagBits::eFragment,
            .offset = sizeof(glm::float4x4) + sizeof(float),
            .size = 2 * sizeof(float),
        },
    };
    particlePipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &particleDescLayout,
        .pushConstantRangeCount = pushConstantRanges.size(),
        .pPushConstantRanges = pushConstantRanges.data(),
    });

    const std::array colorAttachmentFormats{
        colorImage.format,
        normalImage.format,
    };
    const PipelineRenderingCreateInfo rendering{
        .colorAttachmentCount = colorAttachmentFormats.size(),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),
        .depthAttachmentFormat = depthImage.format,
    };
    std::tie(result, particlePipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = particlePipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create particle pipeline");
    }

    particleDescSets = initDescriptorSets(particleDescLayout);
    for (DescriptorSet& set : particleDescSets)
    {
        setStorageBuffer(storageBuffer, storage.offset.x, starParticleCount * sizeof(glm::float4), set, 0);
    }
}

void Vulkan::initializeSkyboxPipeline()
{
    std::vector shaders{
        Shader{
            .name = "skybox",
            .stage = ShaderStage::Vertex,
        },
        Shader{
            .name = "skybox",
            .stage = ShaderStage::Pixel,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr std::array vertexBindingDescriptions{
        VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex::position),
            .inputRate = VertexInputRate::eVertex,
        },
    };
    constexpr std::array vertexAttributeDescriptions{
        VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = Format::eR32G32B32Sfloat,
            .offset = 0,
        },
    };
    const PipelineVertexInputStateCreateInfo vertexInputState{
        .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    constexpr PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eFront,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    const PipelineMultisampleStateCreateInfo multisampleState{
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = true,
        .minSampleShading = 0.25f,
    };

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{
        .depthTestEnable = true,
        .depthCompareOp = CompareOp::eLessOrEqual,
    };

    constexpr std::array colorBlendAttachments{
        PipelineColorBlendAttachmentState{
            .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                              ColorComponentFlagBits::eA,
        },
        PipelineColorBlendAttachmentState{
            .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                              ColorComponentFlagBits::eA,
        },
    };
    const PipelineColorBlendStateCreateInfo colorBlendState{
        .attachmentCount = colorBlendAttachments.size(),
        .pAttachments = colorBlendAttachments.data(),
    };

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(glm::float4x4),
    };
    skyboxPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &skyboxDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    const std::array colorAttachmentFormats{
        colorImage.format,
        normalImage.format,
    };
    const PipelineRenderingCreateInfo rendering{
        .colorAttachmentCount = colorAttachmentFormats.size(),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),
        .depthAttachmentFormat = depthImage.format,
    };
    std::tie(result, skyboxPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = skyboxPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create skybox pipeline");
    }

    setupGraphics();
    skyboxImage = initTextureCubemap(ImageUsageFlagBits::eSampled, ImageData::loadKhronosTexture("starmap"));
    playGraphics();

    skyboxDescSets = initDescriptorSets(skyboxDescLayout);
    for (DescriptorSet& set : skyboxDescSets)
    {
        setCombinedImageSampler({}, skyboxImage, set, 0);
    }
}

void Vulkan::initializePostPipeline()
{
    std::vector shaders{
        Shader{
            .name = "post",
            .stage = ShaderStage::Vertex,
        },
        Shader{
            .name = "post",
            .stage = ShaderStage::Pixel,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr PipelineVertexInputStateCreateInfo vertexInputState{};

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    constexpr PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eNone,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    constexpr PipelineMultisampleStateCreateInfo multisampleState{};

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{};

    constexpr PipelineColorBlendAttachmentState colorBlendAttachment{
        .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                          ColorComponentFlagBits::eA,
    };
    const PipelineColorBlendStateCreateInfo colorBlendState{
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(glm::uint) + 3 * sizeof(float),
    };
    postPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &postDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    const PipelineRenderingCreateInfo rendering{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainFormat,
    };
    std::tie(result, postPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = postPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create post pipeline");
    }

    postDescSets = initDescriptorSets(postDescLayout);
    for (DescriptorSet& set : postDescSets)
    {
        setCombinedImageSampler({}, colorResolveImage, set, 0);
        setCombinedImageSampler({}, depthResolveImage, set, 1);
        setCombinedImageSampler({}, normalResolveImage, set, 2);
    }
}

void Vulkan::initializeGuiPipeline()
{
    std::vector shaders{
        Shader{
            .name = "gui",
            .stage = ShaderStage::Vertex,
        },
        Shader{
            .name = "gui",
            .stage = ShaderStage::Pixel,
        },
    };
    auto shaderStages = initializeShaders(shaders);

    constexpr VertexInputBindingDescription vertexBindingDescription{
        .binding = 0,
        .stride = sizeof(ImDrawVert),
        .inputRate = VertexInputRate::eVertex,
    };
    constexpr std::array vertexAttributeDescriptions{
        VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = Format::eR32G32Sfloat,
            .offset = IM_OFFSETOF(ImDrawVert, pos),
        },
        VertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = Format::eR32G32Sfloat,
            .offset = IM_OFFSETOF(ImDrawVert, uv),
        },
        VertexInputAttributeDescription{
            .location = 2,
            .binding = 0,
            .format = Format::eR8G8B8A8Unorm,
            .offset = IM_OFFSETOF(ImDrawVert, col),
        },
    };
    const PipelineVertexInputStateCreateInfo vertexInputState{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBindingDescription,
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    constexpr PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = PrimitiveTopology::eTriangleList,
    };

    constexpr PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    constexpr PipelineRasterizationStateCreateInfo rasterizationState{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eNone,
        .frontFace = FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };

    const PipelineMultisampleStateCreateInfo multisampleState{};

    constexpr PipelineDepthStencilStateCreateInfo depthStencilState{};

    constexpr PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = true,
        .srcColorBlendFactor = BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = BlendOp::eAdd,
        .srcAlphaBlendFactor = BlendFactor::eOne,
        .dstAlphaBlendFactor = BlendFactor::eOneMinusSrcAlpha,
        .alphaBlendOp = BlendOp::eAdd,
        .colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB |
                          ColorComponentFlagBits::eA,
    };
    const PipelineColorBlendStateCreateInfo colorBlendState{
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    constexpr std::array dynamicStates{
        DynamicState::eViewport,
        DynamicState::eScissor,
    };
    const PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    constexpr PushConstantRange pushConstantRange{
        .stageFlags = ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = 4 * sizeof(float),
    };
    guiPipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &guiDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    });

    const PipelineRenderingCreateInfo rendering{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainFormat,
    };
    std::tie(result, guiPipeline) =
        device.createGraphicsPipeline({}, GraphicsPipelineCreateInfo{
                                              .pNext = &rendering,
                                              .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                              .pStages = shaderStages.data(),
                                              .pVertexInputState = &vertexInputState,
                                              .pInputAssemblyState = &inputAssemblyState,
                                              .pViewportState = &viewportState,
                                              .pRasterizationState = &rasterizationState,
                                              .pMultisampleState = &multisampleState,
                                              .pDepthStencilState = &depthStencilState,
                                              .pColorBlendState = &colorBlendState,
                                              .pDynamicState = &dynamicState,
                                              .layout = guiPipelineLayout,
                                          });
    if (result != Result::eSuccess)
    {
        throw std::runtime_error("Failed to create GUI pipeline");
    }

    setupGraphics();
    fontImage = initTextureImage(ImageUsageFlagBits::eSampled, GUI::fontTexture());
    playGraphics();

    guiDescSets = initDescriptorSets(guiDescLayout);
    for (DescriptorSet& set : guiDescSets)
    {
        setCombinedImageSampler({}, fontImage, set, 0);
    }
}