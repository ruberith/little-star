#include "Image.h"

#include "Utils.h"
#include <ktx.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace fs = std::filesystem;

ImageData ImageData::loadTexture(const std::string& name, const std::string& extension, uint32_t mipLevel,
                                 const std::string& layer)
{
    ImageData texture{};
    const std::string path = texturePath(name, extension, mipLevel, layer).string();
    int channelsInFile;
    texture.data = stbi_load(path.data(), &texture.width, &texture.height, &channelsInFile, channels);
    if (!texture.data)
    {
        throw std::runtime_error("Failed to load texture [" + name + "]");
    }
    texture.size = texture.width * texture.height * channels;
    texture.alloc = true;
    return texture;
}

std::vector<ImageData> ImageData::loadTextureMipmap(const std::string& name, const std::string& extension)
{
    std::vector<ImageData> texture;
    ImageData texture0 = loadTexture(name, extension);
    const uint32_t mipLevelCount = calculateMipLevels(texture0.width, texture0.height);
    texture.reserve(mipLevelCount);
    texture.emplace_back(texture0);
    for (uint32_t i = 1; i < mipLevelCount; i++)
    {
        texture.emplace_back(loadTexture(name, extension, i));
    }
    return texture;
}

std::vector<std::vector<ImageData>> ImageData::loadTextureCubemap(const std::string& name, const std::string& extension)
{
    std::vector<std::vector<ImageData>> texture;
    constexpr std::array faces{"px", "nx", "py", "ny", "pz", "nz"};
    uint32_t mipLevelCount;
    if (!isMipmapped(name, extension, "px", &mipLevelCount))
    {
        mipLevelCount = 1;
    }
    texture.reserve(faces.size());
    for (uint32_t i = 0; i < faces.size(); i++)
    {
        texture.emplace_back(std::vector<ImageData>{});
        texture[i].reserve(mipLevelCount);
        for (uint32_t j = 0; j < mipLevelCount; j++)
        {
            texture[i].emplace_back(loadTexture(name, extension, j, faces[i]));
        }
    }
    return texture;
}

std::vector<std::vector<ImageData>> ImageData::loadKhronosTexture(const std::string& name, const std::string& model)
{
    std::vector<std::vector<ImageData>> data;
    const std::string path =
        (model.empty() ? texturePath(name, "ktx2") : modelTexturePath(model, name, "ktx2")).string();
    ktxTexture* texture;
    if (ktxTexture_CreateFromNamedFile(path.data(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) != KTX_SUCCESS)
    {
        throw std::runtime_error("Failed to load texture [" + name + "]" +
                                 (model.empty() ? "" : (" of model [" + model + "]")));
    }
    size_t offset;
    uint8_t* src = ktxTexture_GetData(texture);
    const uint32_t elementSize = ktxTexture_GetElementSize(texture);
    data.reserve(texture->numFaces);
    for (uint32_t face = 0; face < texture->numFaces; face++)
    {
        int width = texture->baseWidth;
        int height = texture->baseHeight;
        data.emplace_back(std::vector<ImageData>{});
        data[face].reserve(texture->numLevels);
        for (uint32_t mipLevel = 0; mipLevel < texture->numLevels; mipLevel++)
        {
            data[face].emplace_back(ImageData{
                Data::allocate(width * height * channels),
                width,
                height,
            });
            ktxTexture_GetImageOffset(texture, mipLevel, 0, face, &offset);
            if (elementSize < channels)
            {
                uint8_t* dst = static_cast<uint8_t*>(data[face][mipLevel]());
                for (uint32_t element = 0; element < width * height; element++)
                {
                    memcpy(dst, src + offset, elementSize);
                    // Extend the element to four channels.
                    for (uint32_t channel = elementSize; channel < 3; channel++)
                    {
                        dst[channel] = 0;
                    }
                    dst[3] = 255;
                    dst += channels;
                    offset += elementSize;
                }
            }
            else
            {
                ImageData& dst = data[face][mipLevel];
                memcpy(dst(), src + offset, dst.size);
            }
            if (width >= 2)
                width /= 2;
            if (height >= 2)
                height /= 2;
        }
    }
    ktxTexture_Destroy(texture);
    return data;
}

bool ImageData::isMipmapped(const std::string& name, const std::string& extension, const std::string& layer,
                            uint32_t* count)
{
    const std::string path = texturePath(name, extension, 0, layer).string();
    int width, height, channelsInFile;
    if (!stbi_info(path.data(), &width, &height, &channelsInFile))
    {
        throw std::runtime_error("Failed to get info for texture [" + name + "]");
    }
    const uint32_t mipLevelCount = calculateMipLevels(width, height);
    if (count)
    {
        *count = mipLevelCount;
    }
    for (uint32_t i = 1; i < mipLevelCount; i++)
    {
        if (!fs::exists(texturePath(name, extension, i, layer)))
        {
            return false;
        }
    }
    return true;
}

void ImageData::storeTexture(const std::string& name, const std::string& extension, uint32_t mipLevel,
                             const std::string& layer)
{
    const std::string path = texturePath(name, extension, mipLevel, layer).string();
    int success;
    if (extension == "png")
    {
        success = stbi_write_png(path.data(), width, height, channels, data, width * channels);
    }
    else if (extension == "jpg" || extension == "jpeg")
    {
        success = stbi_write_jpg(path.data(), width, height, channels, data, 100);
    }
    else
    {
        throw std::invalid_argument("Failed to write texture [" + name + "]: unsupported extension");
    }
    if (!success)
    {
        throw std::runtime_error("Failed to write texture [" + name + "]");
    }
}