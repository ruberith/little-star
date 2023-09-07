#include "Vulkan.h"

#include "Utils.h"
#include <set>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.hpp>

using namespace vk;
using namespace vma;

void Vulkan::activate(vk::CommandBuffer& commandBuffer)
{
    activeBuffer = commandBuffer;
}

void Vulkan::setup(vk::CommandPool& commandPool, vk::CommandBuffer& commandBuffer)
{
    commandBuffer = device.allocateCommandBuffers(CommandBufferAllocateInfo{
        .commandPool = commandPool,
        .level = CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    })[0];
    commandBuffer.begin(CommandBufferBeginInfo{
        .flags = CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    activate(commandBuffer);
}

void Vulkan::play(vk::Queue& queue, vk::CommandPool& commandPool, vk::CommandBuffer& commandBuffer)
{
    commandBuffer.end();
    queue.submit(SubmitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    });
    queue.waitIdle();
    device.freeCommandBuffers(commandPool, commandBuffer);
    for (AllocatedBuffer& buffer : stagingBuffers)
    {
        destroyBuffer(buffer);
    }
    stagingBuffers.clear();
}

void Vulkan::setupGraphics()
{
    setup(graphicsPool, graphicsBuffer);
}

void Vulkan::playGraphics()
{
    play(graphicsQueue, graphicsPool, graphicsBuffer);
}

void Vulkan::setupTransfer()
{
    setup(transferPool, transferBuffer);
}

void Vulkan::playTransfer()
{
    play(transferQueue, transferPool, transferBuffer);
}

AllocatedBuffer Vulkan::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags bufferUsage,
                                     vma::AllocationCreateFlags flags, vma::MemoryUsage memoryUsage)
{
    std::set queueFamilyIndexSet{gpu.computeQueueFamilyIndex, gpu.graphicsQueueFamilyIndex,
                                 gpu.transferQueueFamilyIndex};
    std::vector queueFamilyIndices(queueFamilyIndexSet.begin(), queueFamilyIndexSet.end());
    AllocatedBuffer buffer{.size = size};
    std::tie(buffer(), buffer.allocation) = allocator.createBuffer(
        BufferCreateInfo{
            .size = size,
            .usage = bufferUsage,
            .sharingMode = queueFamilyIndices.size() > 1 ? SharingMode::eConcurrent : SharingMode::eExclusive,
            .queueFamilyIndexCount = queueFamilyIndices.size() > 1 ? static_cast<uint32_t>(queueFamilyIndices.size())
                                                                   : static_cast<uint32_t>(0),
            .pQueueFamilyIndices = queueFamilyIndices.size() > 1 ? queueFamilyIndices.data() : nullptr,
        },
        AllocationCreateInfo{
            .flags = flags,
            .usage = memoryUsage,
        });
    return buffer;
}

AllocatedBuffer Vulkan::createStagingBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage)
{
    return createBuffer(size, usage, AllocationCreateFlagBits::eHostAccessSequentialWrite);
}

void Vulkan::mapBuffer(AllocatedBuffer& buffer)
{
    buffer.data = allocator.mapMemory(buffer.allocation);
}

void Vulkan::unmapBuffer(AllocatedBuffer& buffer)
{
    allocator.unmapMemory(buffer.allocation);
    buffer.data = {};
}

void Vulkan::fillStagingBuffer(AllocatedBuffer& buffer, Data data, vk::DeviceSize offset)
{
    if (!buffer.data)
    {
        mapBuffer(buffer);
        uint8_t* dst = static_cast<uint8_t*>(buffer.data) + offset;
        memcpy(dst, data(), data.size);
        unmapBuffer(buffer);
    }
    else
    {
        uint8_t* dst = static_cast<uint8_t*>(buffer.data) + offset;
        memcpy(dst, data(), data.size);
    }
}

void Vulkan::fillStagingBuffer(AllocatedBuffer& buffer, std::vector<Data> data, bool align)
{
    if (!buffer.data)
    {
        mapBuffer(buffer);
        uint8_t* dst = static_cast<uint8_t*>(buffer.data);
        for (Data& d : data)
        {
            memcpy(dst, d(), d.size);
            dst += align ? gpu.alignedUniformSize(d.size) : d.size;
        }
        unmapBuffer(buffer);
    }
    else
    {
        uint8_t* dst = static_cast<uint8_t*>(buffer.data);
        for (Data& d : data)
        {
            memcpy(dst, d(), d.size);
            dst += align ? gpu.alignedUniformSize(d.size) : d.size;
        }
    }
}

AllocatedBuffer Vulkan::initStagingBuffer(vk::BufferUsageFlags usage, Data data)
{
    AllocatedBuffer buffer = createStagingBuffer(data.size, usage);
    fillStagingBuffer(buffer, data);
    return buffer;
}

AllocatedBuffer Vulkan::createReadbackBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage)
{
    return createBuffer(size, usage, AllocationCreateFlagBits::eHostAccessRandom);
}

void Vulkan::copyBuffer(AllocatedBuffer& srcBuffer, AllocatedBuffer& dstBuffer, vk::DeviceSize offset)
{
    activeBuffer.copyBuffer(srcBuffer(), dstBuffer(),
                            BufferCopy{
                                .dstOffset = offset,
                                .size = srcBuffer.size,
                            });
}

void Vulkan::fillBuffer(AllocatedBuffer& buffer, Data data, vk::DeviceSize offset)
{
    AllocatedBuffer stagingBuffer = initStagingBuffer(BufferUsageFlagBits::eTransferSrc, data);
    stagingBuffers.emplace_back(stagingBuffer);
    if (data.alloc)
        data.free();
    copyBuffer(stagingBuffer, buffer, offset);
}

AllocatedBuffer Vulkan::initBuffer(vk::BufferUsageFlags usage, Data data)
{
    AllocatedBuffer buffer = createBuffer(data.size, usage | BufferUsageFlagBits::eTransferDst);
    fillBuffer(buffer, data);
    return buffer;
}

AllocatedBuffer Vulkan::initBuffer(vk::BufferUsageFlags usage, std::vector<Data> data)
{
    DeviceSize size = 0;
    for (Data& d : data)
    {
        size += d.size;
    }
    AllocatedBuffer buffer = createBuffer(size, usage | BufferUsageFlagBits::eTransferDst);
    AllocatedBuffer stagingBuffer = createStagingBuffer(size, BufferUsageFlagBits::eTransferSrc);
    fillStagingBuffer(stagingBuffer, data);
    stagingBuffers.emplace_back(stagingBuffer);
    copyBuffer(stagingBuffer, buffer);
    return buffer;
}

void Vulkan::clearBuffer(AllocatedBuffer& buffer, uint32_t value, vk::DeviceSize offset, vk::DeviceSize size)
{
    activeBuffer.fillBuffer(buffer(), offset, size, value);
}

void Vulkan::syncBufferAccess(AllocatedBuffer& buffer, vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccess,
                              vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccess, vk::DeviceSize offset,
                              vk::DeviceSize size)
{
    activeBuffer.pipelineBarrier(srcStage, dstStage, {}, {},
                                 BufferMemoryBarrier{
                                     .srcAccessMask = srcAccess,
                                     .dstAccessMask = dstAccess,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .buffer = buffer(),
                                     .offset = offset,
                                     .size = size,
                                 },
                                 {});
}

void Vulkan::destroyBuffer(AllocatedBuffer& buffer)
{
    allocator.destroyBuffer(buffer(), buffer.allocation);
}

vk::ImageView Vulkan::createImageView(vk::Image& image, vk::ImageViewType type, vk::Format format, uint32_t mipLevels,
                                      uint32_t layers)
{
    return device.createImageView(ImageViewCreateInfo{
        .image = image,
        .viewType = type,
        .format = format,
        .subresourceRange =
            ImageSubresourceRange{
                .aspectMask = isDepthFormat(format) ? ImageAspectFlagBits::eDepth : ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layers,
            },
    });
}

AllocatedImage Vulkan::createImage(vk::ImageViewType type, uint32_t width, uint32_t height, uint32_t mipLevels,
                                   uint32_t layers, vk::SampleCountFlagBits samples, vk::Format format,
                                   vk::ImageTiling tiling, vk::ImageUsageFlags imageUsage,
                                   vma::AllocationCreateFlags flags, vma::MemoryUsage memoryUsage)
{
    std::set queueFamilyIndexSet{gpu.computeQueueFamilyIndex, gpu.graphicsQueueFamilyIndex,
                                 gpu.transferQueueFamilyIndex};
    std::vector queueFamilyIndices(queueFamilyIndexSet.begin(), queueFamilyIndexSet.end());
    AllocatedImage image{
        .format = format,
        .mipLevels = mipLevels,
        .layers = layers,
    };
    std::tie(image(), image.allocation) = allocator.createImage(
        ImageCreateInfo{
            .flags = (type == ImageViewType::eCube) ? ImageCreateFlagBits::eCubeCompatible : ImageCreateFlags{},
            .imageType = ImageType::e2D,
            .format = format,
            .extent = {width, height, 1},
            .mipLevels = mipLevels,
            .arrayLayers = layers,
            .samples = samples,
            .tiling = tiling,
            .usage = imageUsage,
            .sharingMode = queueFamilyIndices.size() > 1 ? SharingMode::eConcurrent : SharingMode::eExclusive,
            .queueFamilyIndexCount = queueFamilyIndices.size() > 1 ? static_cast<uint32_t>(queueFamilyIndices.size())
                                                                   : static_cast<uint32_t>(0),
            .pQueueFamilyIndices = queueFamilyIndices.size() > 1 ? queueFamilyIndices.data() : nullptr,
            .initialLayout = ImageLayout::eUndefined,
        },
        AllocationCreateInfo{
            .flags = flags,
            .usage = memoryUsage,
        });
    image.view = createImageView(image(), type, format, mipLevels, layers);
    return image;
}

void Vulkan::transitionImageLayout(AllocatedImage& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                   uint32_t mipLevel, uint32_t levelCount)
{
    // Select the pipeline stage and access flags depending on the layout combination.
    PipelineStageFlags srcStage, dstStage;
    AccessFlags srcAccess, dstAccess;
    if (oldLayout == ImageLayout::eUndefined && newLayout == ImageLayout::eTransferDstOptimal)
    {
        srcStage = PipelineStageFlagBits::eTopOfPipe;
        srcAccess = {};
        dstStage = PipelineStageFlagBits::eTransfer;
        dstAccess = AccessFlagBits::eTransferWrite;
    }
    else if (oldLayout == ImageLayout::eTransferDstOptimal && newLayout == ImageLayout::eShaderReadOnlyOptimal)
    {
        srcStage = PipelineStageFlagBits::eTransfer;
        srcAccess = AccessFlagBits::eTransferWrite;
        dstStage = PipelineStageFlagBits::eFragmentShader;
        dstAccess = AccessFlagBits::eShaderRead;
    }
    else if (oldLayout == ImageLayout::eTransferDstOptimal && newLayout == ImageLayout::eTransferSrcOptimal)
    {
        srcStage = PipelineStageFlagBits::eTransfer;
        srcAccess = AccessFlagBits::eTransferWrite;
        dstStage = PipelineStageFlagBits::eTransfer;
        dstAccess = AccessFlagBits::eTransferRead;
    }
    else if (oldLayout == ImageLayout::eTransferSrcOptimal && newLayout == ImageLayout::eShaderReadOnlyOptimal)
    {
        srcStage = PipelineStageFlagBits::eTransfer;
        srcAccess = AccessFlagBits::eTransferRead;
        dstStage = PipelineStageFlagBits::eFragmentShader;
        dstAccess = AccessFlagBits::eShaderRead;
    }
    else if (oldLayout == ImageLayout::eUndefined && newLayout == ImageLayout::eColorAttachmentOptimal)
    {
        srcStage = PipelineStageFlagBits::eTopOfPipe;
        srcAccess = {};
        dstStage = PipelineStageFlagBits::eColorAttachmentOutput;
        dstAccess = AccessFlagBits::eColorAttachmentWrite;
    }
    else if (oldLayout == ImageLayout::eColorAttachmentOptimal && newLayout == ImageLayout::eShaderReadOnlyOptimal)
    {
        srcStage = PipelineStageFlagBits::eColorAttachmentOutput;
        srcAccess = AccessFlagBits::eColorAttachmentWrite;
        dstStage = PipelineStageFlagBits::eFragmentShader;
        dstAccess = AccessFlagBits::eShaderRead;
    }
    else if (oldLayout == ImageLayout::eShaderReadOnlyOptimal && newLayout == ImageLayout::eColorAttachmentOptimal)
    {
        srcStage = PipelineStageFlagBits::eFragmentShader;
        srcAccess = AccessFlagBits::eShaderRead;
        dstStage = PipelineStageFlagBits::eColorAttachmentOutput;
        dstAccess = AccessFlagBits::eColorAttachmentWrite;
    }
    else if (oldLayout == ImageLayout::eColorAttachmentOptimal && newLayout == ImageLayout::ePresentSrcKHR)
    {
        srcStage = PipelineStageFlagBits::eColorAttachmentOutput;
        srcAccess = AccessFlagBits::eColorAttachmentWrite;
        dstStage = PipelineStageFlagBits::eBottomOfPipe;
        dstAccess = {};
    }
    else if (oldLayout == ImageLayout::eUndefined && newLayout == ImageLayout::eDepthStencilAttachmentOptimal)
    {
        srcStage = PipelineStageFlagBits::eTopOfPipe;
        srcAccess = {};
        dstStage = PipelineStageFlagBits::eEarlyFragmentTests;
        dstAccess = AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
    }
    else if (oldLayout == ImageLayout::eDepthStencilAttachmentOptimal &&
             newLayout == ImageLayout::eDepthStencilReadOnlyOptimal)
    {
        srcStage = PipelineStageFlagBits::eLateFragmentTests;
        srcAccess = AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
        dstStage = PipelineStageFlagBits::eEarlyFragmentTests;
        dstAccess = AccessFlagBits::eDepthStencilAttachmentRead;
    }
    else if (oldLayout == ImageLayout::eDepthStencilReadOnlyOptimal &&
             newLayout == ImageLayout::eDepthStencilAttachmentOptimal)
    {
        srcStage = PipelineStageFlagBits::eLateFragmentTests;
        srcAccess = AccessFlagBits::eDepthStencilAttachmentRead;
        dstStage = PipelineStageFlagBits::eEarlyFragmentTests;
        dstAccess = AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
    }
    else if (oldLayout == ImageLayout::eDepthStencilAttachmentOptimal &&
             newLayout == ImageLayout::eShaderReadOnlyOptimal)
    {
        srcStage = PipelineStageFlagBits::eLateFragmentTests;
        srcAccess = AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
        dstStage = PipelineStageFlagBits::eFragmentShader;
        dstAccess = AccessFlagBits::eShaderRead;
    }
    else if (oldLayout == ImageLayout::eShaderReadOnlyOptimal &&
             newLayout == ImageLayout::eDepthStencilAttachmentOptimal)
    {
        srcStage = PipelineStageFlagBits::eFragmentShader;
        srcAccess = AccessFlagBits::eShaderRead;
        dstStage = PipelineStageFlagBits::eEarlyFragmentTests;
        dstAccess = AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
    }
    else
    {
        throw std::invalid_argument("Failed to transition image layout: unsupported layout combination");
    }

    // Select the image aspect flags depending on the image format.
    ImageAspectFlags imageAspects;
    if (isDepthFormat(image.format))
    {
        imageAspects = ImageAspectFlagBits::eDepth;
        if (isStencilFormat(image.format))
        {
            imageAspects |= ImageAspectFlagBits::eStencil;
        }
    }
    else
    {
        imageAspects = ImageAspectFlagBits::eColor;
    }

    activeBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {},
                                 ImageMemoryBarrier{
                                     .srcAccessMask = srcAccess,
                                     .dstAccessMask = dstAccess,
                                     .oldLayout = oldLayout,
                                     .newLayout = newLayout,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .image = image(),
                                     .subresourceRange =
                                         ImageSubresourceRange{
                                             .aspectMask = imageAspects,
                                             .baseMipLevel = mipLevel,
                                             .levelCount = (levelCount == 0) ? image.mipLevels : levelCount,
                                             .baseArrayLayer = 0,
                                             .layerCount = image.layers,
                                         },
                                 });
}

void Vulkan::copyBufferToImage(AllocatedBuffer& buffer, AllocatedImage& image, uint32_t width, uint32_t height,
                               uint32_t mipLevel, uint32_t layer)
{
    activeBuffer.copyBufferToImage(buffer(), image(), ImageLayout::eTransferDstOptimal,
                                   BufferImageCopy{
                                       .bufferOffset = 0,
                                       .bufferRowLength = 0,
                                       .bufferImageHeight = 0,
                                       .imageSubresource =
                                           ImageSubresourceLayers{
                                               .aspectMask = ImageAspectFlagBits::eColor,
                                               .mipLevel = mipLevel,
                                               .baseArrayLayer = layer,
                                               .layerCount = 1,
                                           },
                                       .imageOffset = {0, 0, 0},
                                       .imageExtent = {width, height, 1},
                                   });
}

void Vulkan::copyImageToBuffer(AllocatedImage& image, AllocatedBuffer& buffer, uint32_t width, uint32_t height,
                               uint32_t mipLevel, uint32_t layer)
{
    activeBuffer.copyImageToBuffer(image(), ImageLayout::eTransferSrcOptimal, buffer(),
                                   BufferImageCopy{
                                       .bufferOffset = 0,
                                       .bufferRowLength = 0,
                                       .bufferImageHeight = 0,
                                       .imageSubresource =
                                           ImageSubresourceLayers{
                                               .aspectMask = ImageAspectFlagBits::eColor,
                                               .mipLevel = mipLevel,
                                               .baseArrayLayer = layer,
                                               .layerCount = 1,
                                           },
                                       .imageOffset = {0, 0, 0},
                                       .imageExtent = {width, height, 1},
                                   });
}

void Vulkan::fillImage(AllocatedImage& image, ImageData data, uint32_t mipLevel, uint32_t layer)
{
    AllocatedBuffer stagingBuffer = initStagingBuffer(BufferUsageFlagBits::eTransferSrc, data);
    stagingBuffers.emplace_back(stagingBuffer);
    if (data.alloc)
        data.free();
    copyBufferToImage(stagingBuffer, image, data.width, data.height, mipLevel, layer);
}

AllocatedImage Vulkan::initTextureImage(vk::ImageUsageFlags usage, ImageData data, ColorSpace colorSpace)
{
    AllocatedImage image =
        createImage(ImageViewType::e2D, data.width, data.height, 1, 1, SampleCountFlagBits::e1,
                    textureFormat(colorSpace), ImageTiling::eOptimal, usage | ImageUsageFlagBits::eTransferDst);
    transitionImageLayout(image, ImageLayout::eUndefined, ImageLayout::eTransferDstOptimal);
    fillImage(image, data);
    transitionImageLayout(image, ImageLayout::eTransferDstOptimal, ImageLayout::eShaderReadOnlyOptimal);
    return image;
}

AllocatedImage Vulkan::initTextureImage(vk::ImageUsageFlags usage, const glm::u8vec4& color)
{
    ImageData data{
        Data::allocate(4),
    };
    uint8_t* dst = static_cast<uint8_t*>(data());
    dst[0] = color.r;
    dst[1] = color.g;
    dst[2] = color.b;
    dst[3] = color.a;
    return initTextureImage(usage, data, ColorSpace::linear);
}

AllocatedImage Vulkan::initTextureMipmap(vk::ImageUsageFlags usage, std::vector<ImageData> data, ColorSpace colorSpace)
{
    const uint32_t mipLevelCount = data.size();
    AllocatedImage image =
        createImage(ImageViewType::e2D, data[0].width, data[0].height, mipLevelCount, 1, SampleCountFlagBits::e1,
                    textureFormat(colorSpace), ImageTiling::eOptimal, usage | ImageUsageFlagBits::eTransferDst);
    transitionImageLayout(image, ImageLayout::eUndefined, ImageLayout::eTransferDstOptimal);
    for (uint32_t i = 0; i < mipLevelCount; i++)
    {
        fillImage(image, data[i], i);
    }
    transitionImageLayout(image, ImageLayout::eTransferDstOptimal, ImageLayout::eShaderReadOnlyOptimal);
    return image;
}

AllocatedImage Vulkan::initTextureCubemap(vk::ImageUsageFlags usage, std::vector<std::vector<ImageData>> data,
                                          ColorSpace colorSpace)
{
    const uint32_t mipLevelCount = data[0].size();
    AllocatedImage image = createImage(ImageViewType::eCube, data[0][0].width, data[0][0].height, mipLevelCount, 6,
                                       SampleCountFlagBits::e1, textureFormat(colorSpace), ImageTiling::eOptimal,
                                       usage | ImageUsageFlagBits::eTransferDst);
    transitionImageLayout(image, ImageLayout::eUndefined, ImageLayout::eTransferDstOptimal);
    for (uint32_t i = 0; i < 6; i++)
    {
        for (uint32_t j = 0; j < mipLevelCount; j++)
        {
            fillImage(image, data[i][j], j, i);
        }
    }
    transitionImageLayout(image, ImageLayout::eTransferDstOptimal, ImageLayout::eShaderReadOnlyOptimal);
    return image;
}

void Vulkan::destroyImage(AllocatedImage& image)
{
    device.destroyImageView(image.view);
    if (image.allocation)
    {
        allocator.destroyImage(image(), image.allocation);
    }
}

void Vulkan::blitImage(AllocatedImage& image, uint32_t srcMipLevel, int32_t srcWidth, int32_t srcHeight,
                       uint32_t dstMipLevel, int32_t dstWidth, int32_t dstHeight)
{
    activeBuffer.blitImage(image(), ImageLayout::eTransferSrcOptimal, image(), ImageLayout::eTransferDstOptimal,
                           ImageBlit{
                               .srcSubresource =
                                   ImageSubresourceLayers{
                                       .aspectMask = ImageAspectFlagBits::eColor,
                                       .mipLevel = srcMipLevel,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1,
                                   },
                               .srcOffsets =
                                   std::array{
                                       Offset3D{0, 0, 0},
                                       Offset3D{srcWidth, srcHeight, 1},
                                   },
                               .dstSubresource =
                                   ImageSubresourceLayers{
                                       .aspectMask = ImageAspectFlagBits::eColor,
                                       .mipLevel = dstMipLevel,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1,
                                   },
                               .dstOffsets =
                                   std::array{
                                       Offset3D{0, 0, 0},
                                       Offset3D{dstWidth, dstHeight, 1},
                                   },
                           },
                           Filter::eLinear);
}