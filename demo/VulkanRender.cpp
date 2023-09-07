#include "Vulkan.h"

#include "Engine.h"

using namespace vk;

void Vulkan::initializeSwapchain()
{
    gpu.querySwapchainSupport(surface);

    // Create the swapchain.
    swapchainMinImageCount = gpu.selectSwapchainImageCount();
    SurfaceFormatKHR swapchainSurfaceFormat = gpu.selectSwapchainSurfaceFormat();
    swapchainFormat = swapchainSurfaceFormat.format;
    swapchainExtent = gpu.selectSwapchainExtent(engine.glfw);
    std::array queueFamilyIndices{gpu.graphicsQueueFamilyIndex, gpu.presentQueueFamilyIndex};
    PresentModeKHR swapchainPresentMode = gpu.selectSwapchainPresentMode();
    swapchain = device.createSwapchainKHR(SwapchainCreateInfoKHR{
        .surface = surface,
        .minImageCount = swapchainMinImageCount,
        .imageFormat = swapchainSurfaceFormat.format,
        .imageColorSpace = swapchainSurfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = presentQueueDistinct ? SharingMode::eConcurrent : SharingMode::eExclusive,
        .queueFamilyIndexCount = presentQueueDistinct ? static_cast<uint32_t>(2) : static_cast<uint32_t>(0),
        .pQueueFamilyIndices = presentQueueDistinct ? queueFamilyIndices.data() : nullptr,
        .preTransform = gpu.surfaceCapabilities.currentTransform,
        .compositeAlpha = CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = swapchainPresentMode,
        .clipped = true,
    });

    // Create views for the swapchain images.
    for (Image& image : device.getSwapchainImagesKHR(swapchain))
    {
        swapchainImages.emplace_back(AllocatedImage{
            .image = image,
            .format = swapchainFormat,
            .view = createImageView(image, ImageViewType::e2D, swapchainFormat),
        });
    }
    swapchainImageCount = swapchainImages.size();

    // Create the attached images.
    msaaSamples = gpu.selectMsaaSampleCount();
    depthFormat = gpu.selectDepthFormat();
    setupGraphics();
    colorImage = createImage(ImageViewType::e2D, swapchainExtent.width, swapchainExtent.height, 1, 1, msaaSamples,
                             swapchainFormat, ImageTiling::eOptimal, ImageUsageFlagBits::eColorAttachment);
    transitionImageLayout(colorImage, ImageLayout::eUndefined, ImageLayout::eColorAttachmentOptimal);

    colorResolveImage = createImage(ImageViewType::e2D, swapchainExtent.width, swapchainExtent.height, 1, 1,
                                    SampleCountFlagBits::e1, swapchainFormat, ImageTiling::eOptimal,
                                    ImageUsageFlagBits::eColorAttachment | ImageUsageFlagBits::eSampled);
    transitionImageLayout(colorResolveImage, ImageLayout::eUndefined, ImageLayout::eColorAttachmentOptimal);

    depthImage = createImage(ImageViewType::e2D, swapchainExtent.width, swapchainExtent.height, 1, 1, msaaSamples,
                             depthFormat, ImageTiling::eOptimal, ImageUsageFlagBits::eDepthStencilAttachment);
    transitionImageLayout(depthImage, ImageLayout::eUndefined, ImageLayout::eDepthStencilAttachmentOptimal);

    depthResolveImage = createImage(ImageViewType::e2D, swapchainExtent.width, swapchainExtent.height, 1, 1,
                                    SampleCountFlagBits::e1, depthFormat, ImageTiling::eOptimal,
                                    ImageUsageFlagBits::eDepthStencilAttachment | ImageUsageFlagBits::eSampled);
    transitionImageLayout(depthResolveImage, ImageLayout::eUndefined, ImageLayout::eDepthStencilAttachmentOptimal);

    normalImage =
        createImage(ImageViewType::e2D, swapchainExtent.width, swapchainExtent.height, 1, 1, msaaSamples,
                    textureFormat(ColorSpace::linear), ImageTiling::eOptimal, ImageUsageFlagBits::eColorAttachment);
    transitionImageLayout(normalImage, ImageLayout::eUndefined, ImageLayout::eColorAttachmentOptimal);

    normalResolveImage = createImage(ImageViewType::e2D, swapchainExtent.width, swapchainExtent.height, 1, 1,
                                     SampleCountFlagBits::e1, textureFormat(ColorSpace::linear), ImageTiling::eOptimal,
                                     ImageUsageFlagBits::eColorAttachment | ImageUsageFlagBits::eSampled);
    transitionImageLayout(normalResolveImage, ImageLayout::eUndefined, ImageLayout::eColorAttachmentOptimal);
    playGraphics();
}

void Vulkan::updateScene()
{
    using namespace glm;

    // Update the cinematic bar height, frame brightness, star regeneration progress, and lightness blend value
    // depending on the game state.
    float lightness = 1.0f;
    if (engine.state == Engine::State::Intro)
    {
        brightness = fade(engine.stateTime, 0.0f, 3.0f, 12.0f, 13.5f);
        lightness = (engine.stateTime < 10.5f)
                        ? fadeOut(engine.stateTime, 0.0f, 10.5f)
                        : 0.5 * fade(std::fmod(engine.stateTime, 1.5f), 0.0f, 0.75f, 0.75f, 1.5f);
        lightness *= lightness;
    }
    else if (engine.state == Engine::State::Main)
    {
        bar = lerp(0.0f, 0.1f, fadeOut(engine.stateTime, 0.0f, 3.0f));
        brightness = fadeIn(engine.stateTime, 0.0f, 1.5f);
        starProgress = saturate(static_cast<float>(counterBuffer.as<glm::uint>()) / 700.0f);
        lightness = starProgress * starProgress;
    }
    else if (engine.state == Engine::State::Finale)
    {
        bar = lerp(0.0f, 0.1f, fadeIn(engine.stateTime, 0.0f, 3.0f));
    }
    else if (engine.state == Engine::State::Credits)
    {
        brightness = fadeOut(engine.stateTime, 157.0f, 160.0f);
    }

    // Update the scene uniform.
    scene.viewPosition = {engine.camera.position, 1.0f};
    scene.lightPosition = {starPosition, 1.0f};
    scene.lightIntensity = float4{lerp(3000.0f, 15000.0f, lightness)};
    scene.EV100 = engine.camera.EV100;
    scene.exposure = engine.camera.exposure;

    // Update the view projection matrices.
    const float4x4 view = engine.camera.view();
    const float4x4 projection = engine.camera.projection(swapchainExtent.width, swapchainExtent.height);
    cameraTransform.viewProjection = projection * view;
    skyboxModelViewProjection = projection * float4x4(float3x3(view));
    lightTransform.viewProjection =
        perspective(radians(100.0f), 1.0f, 5.0f, 50.0f) * lookAt(float3(scene.lightPosition), {}, {1.0f, 0.0f, 0.0f});
    viewProjection.camera = cameraTransform.viewProjection;
    static constexpr float4x4 clipToTexture{
        0.5f, 0.0f, 0.0f, 0.0f, //
        0.0f, 0.5f, 0.0f, 0.0f, //
        0.0f, 0.0f, 1.0f, 0.0f, //
        0.5f, 0.5f, 0.0f, 1.0f, //
    };
    viewProjection.shadow = clipToTexture * lightTransform.viewProjection;

    // Update the emissive color of the star.
    Model::Material& starMaterial = getModel("star").material("Material");
    starMaterial.uniform.emissiveFactor =
        lerp(float4{0.05f, 0.045f, 0.0f, 1.0f}, float4{1.0f, 0.9f, 0.0f, 1.0f}, lightness);

    // Update the uniform buffer in sequential order.
    varUniformBuffers[frameIndex]
        .set(scene, sceneUniformOffset)
        .set(cameraTransform, cameraTransformUniformOffset)
        .set(lightTransform, lightTransformUniformOffset)
        .set(viewProjection, viewProjectionUniformOffset);
    for (Model& model : models)
    {
        for (Model::Skin& skin : model.skins)
        {
            skin.update(varUniformBuffers[frameIndex]);
        }
    }
    starMaterial.update(varUniformBuffers[frameIndex]);
}

void Vulkan::recordRendering(vk::CommandBuffer& renderBuffer, AllocatedImage& swapchainImage)
{
    renderBuffer.begin(CommandBufferBeginInfo{});
    activate(renderBuffer);

    transitionImageLayout(swapchainImage, ImageLayout::eUndefined, ImageLayout::eColorAttachmentOptimal);

    // Record the shadow pass.
    RenderingAttachmentInfo depthAttachment{
        .imageView = shadowImage.view,
        .imageLayout = ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = AttachmentLoadOp::eClear,
        .storeOp = AttachmentStoreOp::eStore,
        .clearValue = {.depthStencil = {1.0f, 0}},
    };
    renderBuffer.beginRendering(RenderingInfo{
        .renderArea =
            Rect2D{
                .offset = {0, 0},
                .extent = {shadowRes, shadowRes},
            },
        .layerCount = 1,
        .pDepthAttachment = &depthAttachment,
    });
    renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, shadowPipeline);
    renderBuffer.setViewport(0, Viewport{
                                    .x = 0.0f,
                                    .y = 0.0f,
                                    .width = static_cast<float>(shadowRes),
                                    .height = static_cast<float>(shadowRes),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f,
                                });
    renderBuffer.setScissor(0, Rect2D{
                                   .offset = {0, 0},
                                   .extent = {shadowRes, shadowRes},
                               });
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 0, shadowDescSets[frameIndex],
                                    {});
    Model::Skin* activeSkin{};
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 1,
                                    inactiveSkinDescSets[frameIndex], {});
    for (const Model& model : models)
    {
        for (const Model::Material& material : model.materials)
        {
            for (const Model::Node* node : material.nodes)
            {
                if (node->skin)
                {
                    if (activeSkin != node->skin)
                    {
                        activeSkin = node->skin;
                        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 1,
                                                        node->skin->descSets[frameIndex], {});
                    }
                }
                else
                {
                    renderBuffer.pushConstants<glm::float4x4>(depthPipelineLayout, ShaderStageFlagBits::eVertex, 0,
                                                              node->model);
                    if (activeSkin)
                    {
                        activeSkin = {};
                        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 1,
                                                        inactiveSkinDescSets[frameIndex], {});
                    }
                }
                const Model::Mesh& mesh = *node->mesh;
                renderBuffer.pushConstants<glm::uint>(depthPipelineLayout, ShaderStageFlagBits::eVertex,
                                                      sizeof(glm::float4x4), static_cast<glm::uint>(mesh.embedding));
                renderBuffer.bindVertexBuffers(0, {vertexBuffer(), vertexBuffer(), vertexBuffer()},
                                               {mesh.positionOffset, mesh.jointsOffset, mesh.weightsOffset});
                renderBuffer.bindIndexBuffer(indexBuffer(), mesh.indexOffset, mesh.indexType);
                renderBuffer.drawIndexed(mesh.indexCount, 1, 0, 0, 0);
            }
        }
    }
    renderBuffer.endRendering();

    transitionImageLayout(shadowImage, ImageLayout::eDepthStencilAttachmentOptimal,
                          ImageLayout::eShaderReadOnlyOptimal);

    // Record the depth pass.
    depthAttachment = RenderingAttachmentInfo{
        .imageView = depthImage.view,
        .imageLayout = ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = AttachmentLoadOp::eClear,
        .storeOp = AttachmentStoreOp::eStore,
        .clearValue = {.depthStencil = {1.0f, 0}},
    };
    renderBuffer.beginRendering(RenderingInfo{
        .renderArea =
            Rect2D{
                .offset = {0, 0},
                .extent = swapchainExtent,
            },
        .layerCount = 1,
        .pDepthAttachment = &depthAttachment,
    });
    renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, depthPipeline);
    renderBuffer.setViewport(0, Viewport{
                                    .x = 0.0f,
                                    .y = static_cast<float>(swapchainExtent.height),
                                    .width = static_cast<float>(swapchainExtent.width),
                                    .height = -static_cast<float>(swapchainExtent.height),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f,
                                });
    renderBuffer.setScissor(0, Rect2D{
                                   .offset = {0, 0},
                                   .extent = swapchainExtent,
                               });
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 0, depthDescSets[frameIndex],
                                    {});
    activeSkin = {};
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 1,
                                    inactiveSkinDescSets[frameIndex], {});
    for (const Model& model : models)
    {
        for (const Model::Material& material : model.materials)
        {
            for (const Model::Node* node : material.nodes)
            {
                if (node->skin)
                {
                    if (activeSkin != node->skin)
                    {
                        activeSkin = node->skin;
                        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 1,
                                                        node->skin->descSets[frameIndex], {});
                    }
                }
                else
                {
                    renderBuffer.pushConstants<glm::float4x4>(depthPipelineLayout, ShaderStageFlagBits::eVertex, 0,
                                                              node->model);
                    if (activeSkin)
                    {
                        activeSkin = {};
                        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, depthPipelineLayout, 1,
                                                        inactiveSkinDescSets[frameIndex], {});
                    }
                }
                const Model::Mesh& mesh = *node->mesh;
                renderBuffer.pushConstants<glm::uint>(depthPipelineLayout, ShaderStageFlagBits::eVertex,
                                                      sizeof(glm::float4x4), static_cast<glm::uint>(mesh.embedding));
                renderBuffer.bindVertexBuffers(0, {vertexBuffer(), vertexBuffer(), vertexBuffer()},
                                               {mesh.positionOffset, mesh.jointsOffset, mesh.weightsOffset});
                renderBuffer.bindIndexBuffer(indexBuffer(), mesh.indexOffset, mesh.indexType);
                renderBuffer.drawIndexed(mesh.indexCount, 1, 0, 0, 0);
            }
        }
    }
    if (starParticlesActive)
    {
        renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, particleDepthPipeline);
        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, particleDepthPipelineLayout, 0,
                                        particleDescSets[frameIndex], {});
        renderBuffer.pushConstants<glm::float4x4>(particleDepthPipelineLayout, ShaderStageFlagBits::eVertex, 0,
                                                  viewProjection.camera);
        renderBuffer.pushConstants<float>(particleDepthPipelineLayout, ShaderStageFlagBits::eVertex,
                                          sizeof(glm::float4x4), starParticleRadius);
        renderBuffer.bindVertexBuffers(0, vertexBuffer(), particleVertexOffset);
        renderBuffer.bindIndexBuffer(indexBuffer(), particleIndexOffset, IndexType::eUint16);
        renderBuffer.drawIndexed(particleIndexCount, starParticleCount, 0, 0, 0);
    }
    renderBuffer.endRendering();

    transitionImageLayout(depthImage, ImageLayout::eDepthStencilAttachmentOptimal,
                          ImageLayout::eDepthStencilReadOnlyOptimal);

    // Record the lighting pass.
    const std::array colorAttachments{
        RenderingAttachmentInfo{
            .imageView = colorImage.view,
            .imageLayout = ImageLayout::eColorAttachmentOptimal,
            .resolveMode = ResolveModeFlagBits::eAverage,
            .resolveImageView = colorResolveImage.view,
            .resolveImageLayout = ImageLayout::eColorAttachmentOptimal,
            .loadOp = AttachmentLoadOp::eClear,
            .storeOp = AttachmentStoreOp::eDontCare,
            .clearValue = {.color = {{{0.0f, 0.0f, 0.0f, 1.0f}}}},
        },
        RenderingAttachmentInfo{
            .imageView = normalImage.view,
            .imageLayout = ImageLayout::eColorAttachmentOptimal,
            .resolveMode = ResolveModeFlagBits::eAverage,
            .resolveImageView = normalResolveImage.view,
            .resolveImageLayout = ImageLayout::eColorAttachmentOptimal,
            .loadOp = AttachmentLoadOp::eClear,
            .storeOp = AttachmentStoreOp::eDontCare,
            .clearValue = {.color = {{{0.0f, 0.0f, 0.0f, 0.0f}}}},
        },
    };
    depthAttachment = RenderingAttachmentInfo{
        .imageView = depthImage.view,
        .imageLayout = ImageLayout::eDepthStencilReadOnlyOptimal,
        .resolveMode = ResolveModeFlagBits::eMin,
        .resolveImageView = depthResolveImage.view,
        .resolveImageLayout = ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = AttachmentLoadOp::eLoad,
        .storeOp = AttachmentStoreOp::eDontCare,
    };
    renderBuffer.beginRendering(RenderingInfo{
        .renderArea =
            Rect2D{
                .offset = {0, 0},
                .extent = swapchainExtent,
            },
        .layerCount = 1,
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = &depthAttachment,
    });
    renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, lightingPipeline);
    renderBuffer.setViewport(0, Viewport{
                                    .x = 0.0f,
                                    .y = static_cast<float>(swapchainExtent.height),
                                    .width = static_cast<float>(swapchainExtent.width),
                                    .height = -static_cast<float>(swapchainExtent.height),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f,
                                });
    renderBuffer.setScissor(0, Rect2D{
                                   .offset = {0, 0},
                                   .extent = swapchainExtent,
                               });
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, lightingPipelineLayout, 0, sceneDescSets[frameIndex],
                                    {});
    activeSkin = {};
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, lightingPipelineLayout, 2,
                                    inactiveSkinDescSets[frameIndex], {});
    for (const Model& model : models)
    {
        for (const Model::Material& material : model.materials)
        {
            renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, lightingPipelineLayout, 1,
                                            material.descSets[frameIndex], {});
            for (const Model::Node* node : material.nodes)
            {
                if (node->skin)
                {
                    if (activeSkin != node->skin)
                    {
                        activeSkin = node->skin;
                        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, lightingPipelineLayout, 2,
                                                        node->skin->descSets[frameIndex], {});
                    }
                }
                else
                {
                    renderBuffer.pushConstants<glm::float4x4>(lightingPipelineLayout, ShaderStageFlagBits::eVertex, 0,
                                                              node->model);
                    if (activeSkin)
                    {
                        activeSkin = {};
                        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, lightingPipelineLayout, 2,
                                                        inactiveSkinDescSets[frameIndex], {});
                    }
                }
                const Model::Mesh& mesh = *node->mesh;
                renderBuffer.pushConstants<glm::uint>(lightingPipelineLayout,
                                                      ShaderStageFlagBits::eVertex | ShaderStageFlagBits::eFragment,
                                                      sizeof(glm::float4x4), static_cast<glm::uint>(mesh.embedding));
                renderBuffer.bindVertexBuffers(
                    0,
                    {vertexBuffer(), vertexBuffer(), vertexBuffer(), vertexBuffer(), vertexBuffer(), vertexBuffer(),
                     vertexBuffer(), vertexBuffer(), vertexBuffer(), vertexBuffer(), vertexBuffer()},
                    {mesh.positionOffset, mesh.normalOffset, mesh.tangentOffset, mesh.texcoordOffset[0],
                     mesh.texcoordOffset[1], mesh.texcoordOffset[2], mesh.texcoordOffset[3], mesh.texcoordOffset[4],
                     mesh.colorOffset, mesh.jointsOffset, mesh.weightsOffset});
                renderBuffer.bindIndexBuffer(indexBuffer(), mesh.indexOffset, mesh.indexType);
                renderBuffer.drawIndexed(mesh.indexCount, 1, 0, 0, 0);
            }
        }
    }
    if (starParticlesActive)
    {
        renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, particlePipeline);
        renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, particlePipelineLayout, 0,
                                        particleDescSets[frameIndex], {});
        renderBuffer.pushConstants<glm::float4x4>(particlePipelineLayout, ShaderStageFlagBits::eVertex, 0,
                                                  viewProjection.camera);
        renderBuffer.pushConstants<float>(particlePipelineLayout, ShaderStageFlagBits::eVertex, sizeof(glm::float4x4),
                                          starParticleRadius);
        renderBuffer.pushConstants<float>(particlePipelineLayout, ShaderStageFlagBits::eFragment,
                                          sizeof(glm::float4x4) + sizeof(float), scene.EV100);
        renderBuffer.pushConstants<float>(particlePipelineLayout, ShaderStageFlagBits::eFragment,
                                          sizeof(glm::float4x4) + 2 * sizeof(float), scene.exposure);
        renderBuffer.bindVertexBuffers(0, vertexBuffer(), particleVertexOffset);
        renderBuffer.bindIndexBuffer(indexBuffer(), particleIndexOffset, IndexType::eUint16);
        renderBuffer.drawIndexed(particleIndexCount, starParticleCount, 0, 0, 0);
    }
    renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, skyboxPipeline);
    renderBuffer.pushConstants<glm::float4x4>(skyboxPipelineLayout, ShaderStageFlagBits::eVertex, 0,
                                              skyboxModelViewProjection);
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, skyboxPipelineLayout, 0, skyboxDescSets[frameIndex],
                                    {});
    renderBuffer.bindVertexBuffers(0, vertexBuffer(), skyboxVertexOffset);
    renderBuffer.bindIndexBuffer(indexBuffer(), skyboxIndexOffset, IndexType::eUint16);
    renderBuffer.drawIndexed(skyboxIndexCount, 1, 0, 0, 0);
    renderBuffer.endRendering();

    transitionImageLayout(colorResolveImage, ImageLayout::eColorAttachmentOptimal, ImageLayout::eShaderReadOnlyOptimal);
    transitionImageLayout(depthImage, ImageLayout::eDepthStencilReadOnlyOptimal,
                          ImageLayout::eDepthStencilAttachmentOptimal);
    transitionImageLayout(depthResolveImage, ImageLayout::eDepthStencilAttachmentOptimal,
                          ImageLayout::eShaderReadOnlyOptimal);
    transitionImageLayout(normalResolveImage, ImageLayout::eColorAttachmentOptimal,
                          ImageLayout::eShaderReadOnlyOptimal);
    transitionImageLayout(shadowImage, ImageLayout::eShaderReadOnlyOptimal,
                          ImageLayout::eDepthStencilAttachmentOptimal);

    // Record the post pass.
    RenderingAttachmentInfo colorAttachment{
        .imageView = swapchainImage.view,
        .imageLayout = ImageLayout::eColorAttachmentOptimal,
        .loadOp = AttachmentLoadOp::eDontCare,
        .storeOp = AttachmentStoreOp::eStore,
    };
    renderBuffer.beginRendering(RenderingInfo{
        .renderArea =
            Rect2D{
                .offset = {0, 0},
                .extent = swapchainExtent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    });
    renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, postPipeline);
    renderBuffer.pushConstants<glm::uint>(postPipelineLayout, ShaderStageFlagBits::eFragment, 0,
                                          static_cast<glm::uint>(cel));
    renderBuffer.pushConstants<float>(postPipelineLayout, ShaderStageFlagBits::eFragment, sizeof(glm::uint), bar);
    renderBuffer.pushConstants<float>(postPipelineLayout, ShaderStageFlagBits::eFragment,
                                      sizeof(glm::uint) + sizeof(float), static_cast<float>(swapchainExtent.height));
    renderBuffer.pushConstants<float>(postPipelineLayout, ShaderStageFlagBits::eFragment,
                                      sizeof(glm::uint) + 2 * sizeof(float), brightness);
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, postPipelineLayout, 0, postDescSets[frameIndex], {});
    renderBuffer.setViewport(0, Viewport{
                                    .x = 0.0f,
                                    .y = 0.0f,
                                    .width = static_cast<float>(swapchainExtent.width),
                                    .height = static_cast<float>(swapchainExtent.height),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f,
                                });
    renderBuffer.setScissor(0, Rect2D{
                                   .offset = {0, 0},
                                   .extent = swapchainExtent,
                               });
    renderBuffer.draw(3, 1, 0, 0);
    renderGui(renderBuffer);
    renderBuffer.endRendering();

    transitionImageLayout(colorResolveImage, ImageLayout::eShaderReadOnlyOptimal, ImageLayout::eColorAttachmentOptimal);
    transitionImageLayout(depthResolveImage, ImageLayout::eShaderReadOnlyOptimal,
                          ImageLayout::eDepthStencilAttachmentOptimal);
    transitionImageLayout(normalResolveImage, ImageLayout::eShaderReadOnlyOptimal,
                          ImageLayout::eColorAttachmentOptimal);
    transitionImageLayout(swapchainImage, ImageLayout::eColorAttachmentOptimal, ImageLayout::ePresentSrcKHR);

    renderBuffer.end();
}

void Vulkan::renderGui(vk::CommandBuffer& renderBuffer)
{
    ImDrawData* drawData = GUI::drawData();
    if (!drawData || drawData->TotalIdxCount == 0)
    {
        return;
    }

    // If necessary, create or resize the GUI vertex buffer.
    DeviceSize vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    if (vertexSize > guiVertexBuffers[frameIndex].size)
    {
        if (guiVertexBuffers[frameIndex]())
        {
            unmapBuffer(guiVertexBuffers[frameIndex]);
            destroyBuffer(guiVertexBuffers[frameIndex]);
        }
        guiVertexBuffers[frameIndex] = createStagingBuffer(vertexSize, BufferUsageFlagBits::eVertexBuffer);
        mapBuffer(guiVertexBuffers[frameIndex]);
    }

    // If necessary, create or resize the GUI index buffer.
    DeviceSize indexSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
    if (indexSize > guiIndexBuffers[frameIndex].size)
    {
        if (guiIndexBuffers[frameIndex]())
        {
            unmapBuffer(guiIndexBuffers[frameIndex]);
            destroyBuffer(guiIndexBuffers[frameIndex]);
        }
        guiIndexBuffers[frameIndex] = createStagingBuffer(indexSize, BufferUsageFlagBits::eIndexBuffer);
        mapBuffer(guiIndexBuffers[frameIndex]);
    }

    // Fill the vertex buffer and the index buffer.
    ImDrawVert* vertexDst = static_cast<ImDrawVert*>(guiVertexBuffers[frameIndex].data);
    ImDrawIdx* indexDst = static_cast<ImDrawIdx*>(guiIndexBuffers[frameIndex].data);
    for (uint32_t i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* drawList = drawData->CmdLists[i];
        memcpy(vertexDst, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(indexDst, drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertexDst += drawList->VtxBuffer.Size;
        indexDst += drawList->IdxBuffer.Size;
    }

    // Bind the resources.
    renderBuffer.bindPipeline(PipelineBindPoint::eGraphics, guiPipeline);
    renderBuffer.bindVertexBuffers(0, guiVertexBuffers[frameIndex](), {0});
    renderBuffer.bindIndexBuffer(guiIndexBuffers[frameIndex](), 0,
                                 sizeof(ImDrawIdx) == 2 ? IndexType::eUint16 : IndexType::eUint32);
    renderBuffer.bindDescriptorSets(PipelineBindPoint::eGraphics, guiPipelineLayout, 0, guiDescSets[frameIndex], {});

    // Set the viewport.
    float viewportWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
    float viewportHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;
    renderBuffer.setViewport(0, Viewport{
                                    .x = 0.0f,
                                    .y = 0.0f,
                                    .width = viewportWidth,
                                    .height = viewportHeight,
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f,
                                });

    // Set the push constants.
    const std::array scale{
        2.0f / drawData->DisplaySize.x,
        2.0f / drawData->DisplaySize.y,
    };
    const std::array translate{
        -1.0f - drawData->DisplayPos.x * scale[0],
        -1.0f - drawData->DisplayPos.y * scale[1],
    };
    renderBuffer.pushConstants<float>(guiPipelineLayout, ShaderStageFlagBits::eVertex, 0, scale);
    renderBuffer.pushConstants<float>(guiPipelineLayout, ShaderStageFlagBits::eVertex, 8, translate);

    // Record the draw commands.
    ImVec2 clipOffset = drawData->DisplayPos;
    ImVec2 clipScale = drawData->FramebufferScale;
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    for (uint32_t i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* drawList = drawData->CmdLists[i];
        for (uint32_t j = 0; j < drawList->CmdBuffer.Size; j++)
        {
            const ImDrawCmd* drawCommand = &drawList->CmdBuffer[j];

            ImVec2 clipMin{
                (drawCommand->ClipRect.x - clipOffset.x) * clipScale.x,
                (drawCommand->ClipRect.y - clipOffset.y) * clipScale.y,
            };
            ImVec2 clipMax{
                (drawCommand->ClipRect.z - clipOffset.x) * clipScale.x,
                (drawCommand->ClipRect.w - clipOffset.y) * clipScale.y,
            };
            if (clipMin.x < 0.0f)
                clipMin.x = 0.0f;
            if (clipMin.y < 0.0f)
                clipMin.y = 0.0f;
            if (clipMax.x > viewportWidth)
                clipMax.x = viewportWidth;
            if (clipMax.y > viewportHeight)
                clipMax.y = viewportHeight;
            if (clipMin.x >= clipMax.x || clipMin.y >= clipMax.y)
                continue;
            renderBuffer.setScissor(0, Rect2D{
                                           .offset = {static_cast<int32_t>(clipMin.x), static_cast<int32_t>(clipMin.y)},
                                           .extent = {static_cast<uint32_t>(clipMax.x - clipMin.x),
                                                      static_cast<uint32_t>(clipMax.y - clipMin.y)},
                                       });

            renderBuffer.drawIndexed(drawCommand->ElemCount, 1, indexOffset + drawCommand->IdxOffset,
                                     vertexOffset + drawCommand->VtxOffset, 0);
        }
        vertexOffset += drawList->VtxBuffer.Size;
        indexOffset += drawList->IdxBuffer.Size;
    }
}

void Vulkan::render()
{
    result = device.waitForFences(frameInFlight[frameIndex], true, UINT64_MAX);

    // Acquire the next available presentable image.
    uint32_t imageIndex;
    std::tie(result, imageIndex) = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAcquired[frameIndex], {});
    switch (result)
    {
    case Result::eSuccess:
    case Result::eSuboptimalKHR:
        break;
    case Result::eErrorOutOfDateKHR:
        recreateSwapchain();
        return;
    default:
        throw std::runtime_error("Failed to acquire next available presentable image");
    }

    updateScene();

    device.resetFences(frameInFlight[frameIndex]);
    device.resetCommandPool(renderPools[frameIndex]);

    recordRendering(renderBuffers[frameIndex], swapchainImages[imageIndex]);

    const std::array waitSemaphoreValues{updateCount, static_cast<uint64_t>(0)};
    constexpr uint64_t signalSemaphoreValue{0};
    const TimelineSemaphoreSubmitInfo semaphoreValues{
        .waitSemaphoreValueCount = waitSemaphoreValues.size(),
        .pWaitSemaphoreValues = waitSemaphoreValues.data(),
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &signalSemaphoreValue,
    };
    const std::array waitSemaphores{
        simComplete,
        imageAcquired[frameIndex],
    };
    constexpr std::array waitDstStages{
        PipelineStageFlags{PipelineStageFlagBits::eVertexShader},
        PipelineStageFlags{PipelineStageFlagBits::eColorAttachmentOutput},
    };
    graphicsQueue.submit(
        SubmitInfo{
            .pNext = &semaphoreValues,
            .waitSemaphoreCount = waitSemaphores.size(),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitDstStages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &renderBuffers[frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderComplete[frameIndex],
        },
        frameInFlight[frameIndex]);

    result = presentQueue.presentKHR(PresentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderComplete[frameIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    });

    // If necessary, recreate the swapchain.
    if (resizedFramebuffer)
    {
        resizedFramebuffer = false;
        if (result == Result::eSuccess)
        {
            result = Result::eErrorOutOfDateKHR;
        }
    }
    switch (result)
    {
    case Result::eSuccess:
        break;
    case Result::eSuboptimalKHR:
    case Result::eErrorOutOfDateKHR:
        recreateSwapchain();
        break;
    default:
        throw std::runtime_error("Failed to queue image for presentation");
    }

    frameIndex = (frameIndex + 1) % frameCount;
}

void Vulkan::recreateSwapchain()
{
    int width, height;
    engine.glfw.getFramebufferSize(width, height);
    while (width == 0 || height == 0)
    {
        engine.glfw.getFramebufferSize(width, height);
        engine.glfw.waitEvents();
    }
    device.waitIdle();
    terminateSwapchain();
    initializeSwapchain();
    for (DescriptorSet& set : postDescSets)
    {
        setCombinedImageSampler({}, colorResolveImage, set, 0);
        setCombinedImageSampler({}, depthResolveImage, set, 1);
        setCombinedImageSampler({}, normalResolveImage, set, 2);
    }
}

void Vulkan::terminateSwapchain()
{
    destroyImage(colorImage);
    destroyImage(colorResolveImage);
    destroyImage(depthImage);
    destroyImage(depthResolveImage);
    destroyImage(normalImage);
    destroyImage(normalResolveImage);
    for (AllocatedImage& swapchainImage : swapchainImages)
    {
        destroyImage(swapchainImage);
    }
    swapchainImages.clear();
    device.destroySwapchainKHR(swapchain);
}