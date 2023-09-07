#pragma once

#include "Data.h"
#include <stb_image.h>
#include <vk_mem_alloc.hpp>

#ifdef max
#undef max
#endif

// image data
struct ImageData : Data
{
    // image width
    int width{1};
    // image height
    int height{1};
    // image channel count
    static const int channels{STBI_rgb_alpha};

    // Calculate the mip level count for the given image width and height.
    static inline uint32_t calculateMipLevels(int width, int height)
    {
        return 1 + static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))));
    }
    // Load a texture image.
    static ImageData loadTexture(const std::string& name, const std::string& extension = "png", uint32_t mipLevel = 0,
                                 const std::string& layer = {});
    // Load a texture mipmap.
    static std::vector<ImageData> loadTextureMipmap(const std::string& name, const std::string& extension = "png");
    // Load a texture cubemap.
    static std::vector<std::vector<ImageData>> loadTextureCubemap(const std::string& name,
                                                                  const std::string& extension = "png");
    // Load a Khronos texture (KTX 2.0).
    static std::vector<std::vector<ImageData>> loadKhronosTexture(const std::string& name,
                                                                  const std::string& model = {});
    // Return true if the specified texture is mipmapped. Return false otherwise.
    // Optionally get the mip level count.
    static bool isMipmapped(const std::string& name, const std::string& extension = "png",
                            const std::string& layer = {}, uint32_t* count = {});
    // Store the texture image to the specified file.
    void storeTexture(const std::string& name, const std::string& extension = "png", uint32_t mipLevel = 0,
                      const std::string& layer = {});
};

// Vulkan image + allocation
struct AllocatedImage
{
    // image handle
    vk::Image image;
    // memory allocation
    vma::Allocation allocation;
    // image format
    vk::Format format;
    // image mip level count
    uint32_t mipLevels{1};
    // image layer count
    uint32_t layers{1};
    // image view
    vk::ImageView view;

    // Return the image handle.
    vk::Image& operator()()
    {
        return image;
    }
};