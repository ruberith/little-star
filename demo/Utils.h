#pragma once

#include <filesystem>
#include <glm/gtx/compatibility.hpp>
#include <vulkan/vulkan.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#include <windows.h>
#undef max
#undef min
#undef STRICT
#endif

// demo executable path
inline const std::filesystem::path demoPath = []() -> std::filesystem::path {
#if defined(__APPLE__) || defined(_WIN32)
    char buffer[1024];
    uint32_t bufferSize = static_cast<uint32_t>(sizeof(buffer));
#if defined(__APPLE__)
    _NSGetExecutablePath(buffer, &bufferSize);
#elif defined(_WIN32)
    GetModuleFileName(NULL, buffer, bufferSize);
#endif
    std::string bufferString(buffer);
    return std::filesystem::path(bufferString.substr(0, bufferString.find('\0')));
#else
    return std::filesystem::canonical("/proc/self/exe");
#endif
}();

// Return the path to the specified font file.
inline std::filesystem::path fontPath(const std::string& name, const std::string& style = "Regular",
                                      const std::string& extension = "ttf")
{
    return demoPath.parent_path().parent_path() / "demo" / "fonts" / name / (name + "." + style + "." + extension);
}

// Return the path to the specified mesh file.
inline std::filesystem::path meshPath(const std::string& name, const std::string& extension = "obj")
{
    return demoPath.parent_path().parent_path() / "demo" / "meshes" / name / (name + "." + extension);
}

// Return the path to the specified model file.
inline std::filesystem::path modelPath(const std::string& name, const std::string& extension = "gltf")
{
    return demoPath.parent_path().parent_path() / "demo" / "models" / name / (name + "." + extension);
}

// Return the path to the specified model mesh file.
inline std::filesystem::path modelMeshPath(const std::string& model, const std::string& mesh,
                                           const std::string& extension = "msh")
{
    return demoPath.parent_path().parent_path() / "demo" / "models" / model / "meshes" / (mesh + "." + extension);
}

// Return the path to the specified model texture file.
inline std::filesystem::path modelTexturePath(const std::string& model, const std::string& texture,
                                              const std::string& extension = "ktx2")
{
    return demoPath.parent_path().parent_path() / "demo" / "models" / model / "textures" / (texture + "." + extension);
}

// Return the path to the specified sound file.
inline std::filesystem::path soundPath(const std::string& name, const std::string& extension = "wav")
{
    return demoPath.parent_path().parent_path() / "demo" / "sounds" / name / (name + "." + extension);
}

// Return the path to the specified texture file.
inline std::filesystem::path texturePath(const std::string& name, const std::string& extension = "png",
                                         uint32_t mipLevel = 0, const std::string& layer = {})
{
    return demoPath.parent_path().parent_path() / "demo" / "textures" / name /
           (name + (mipLevel == 0 ? "" : ("." + std::to_string(mipLevel))) + (layer.empty() ? "" : ("." + layer)) +
            "." + extension);
}

// Return true if the given format is a depth format. Return false otherwise.
inline bool isDepthFormat(vk::Format format)
{
    using namespace vk;

    switch (format)
    {
    case Format::eD24UnormS8Uint:
    case Format::eD32Sfloat:
    case Format::eD32SfloatS8Uint:
        return true;
    default:
        return false;
    }
}

// Return true if the given format is a stencil format. Return false otherwise.
inline bool isStencilFormat(vk::Format format)
{
    using namespace vk;

    switch (format)
    {
    case Format::eD24UnormS8Uint:
    case Format::eD32SfloatS8Uint:
        return true;
    default:
        return false;
    }
}

// color space (transfer function)
enum struct ColorSpace
{
    linear,
    sRGB,
};

// Return the 8-bit RGBA texture format corresponding to the given color space.
inline vk::Format textureFormat(ColorSpace colorSpace)
{
    using namespace vk;

    switch (colorSpace)
    {
    case ColorSpace::linear:
        return Format::eR8G8B8A8Unorm;
    case ColorSpace::sRGB:
        return Format::eR8G8B8A8Srgb;
    }
}

// Compose the transformation matrix from translation, rotation, and scale.
inline glm::float4x4 compose(const glm::float3& translation, const glm::quat& rotation, const glm::float3& scale)
{
    return glm::translate(glm::float4x4(1.0f), translation) * glm::mat4_cast(rotation) *
           glm::scale(glm::float4x4(1.0f), scale);
}

// Decompose the transformation matrix into translation, rotation, and scale.
inline void decompose(const glm::float4x4& matrix, glm::float3& translation, glm::quat& rotation, glm::float3& scale)
{
    using namespace glm;

    translation = float3(matrix[3]);

    scale.x = length(float3(matrix[0]));
    scale.y = length(float3(matrix[1]));
    scale.z = length(float3(matrix[2]));
    if (determinant(matrix) < 0.0f)
    {
        scale.x = -scale.x;
    }

    float3 invScale = 1.0f / scale;
    float3x3 rot = float3x3(matrix);
    rot[0] *= invScale.x;
    rot[1] *= invScale.y;
    rot[2] *= invScale.z;
    rotation = normalize(quat_cast(rot));
}

// Transform the point with the transformation matrix using homogeneous coordinates.
inline glm::float4 transformPoint(const glm::float3& x, const glm::float4x4& transformation)
{
    glm::float4 x_ = {x, 1.0f};
    x_ = transformation * x_;
    x_ /= x_.w;
    return x_;
}

// Transform the vector with the transformation matrix using homogeneous coordinates.
inline glm::float4 transformVector(const glm::float3& v, const glm::float4x4& transformation)
{
    return transformation * glm::float4{v, 0.0f};
}

// mesh embedding
enum struct MeshEmbedding : glm::uint
{
    none = 0,
    triangular = 1,
    tetrahedral = 2,
};

// Align the size with the alignment s.t. the aligned size is the next higher multiple of the alignment.
inline constexpr vk::DeviceSize alignedSize(vk::DeviceSize size, vk::DeviceSize alignment)
{
    if (alignment > 0)
    {
        return (size + alignment - 1) & -alignment;
    }
    return size;
}

// workgroup dimensions
struct WorkgroupDimensions
{
    // number of workgroups to dispatch
    uint32_t count{};
    // number of threads in a workgroup
    uint32_t size{};
};

// interval
template <typename T> struct Interval
{
    // minimum value
    T min;
    // maximum value
    T max;

    // Return true if the interval contains the value. Return false otherwise.
    constexpr bool contains(T value)
    {
        return (min <= value) && (value <= max);
    }
};

// Construct the specified interval.
template <typename T> Interval<T> interval(T min, T max)
{
    return Interval<T>{min, max};
}

// Clamp the value to the range [0,1].
inline constexpr float saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

// Return the blend/fade-in value i.e. the saturated interpolation weight for the time relative to [start,end].
inline constexpr float fadeIn(float time, float start, float end)
{
    return saturate((time - start) / (end - start));
}

// Return the blend/fade-out value i.e. the inverse of the blend/fade-in value.
inline constexpr float fadeOut(float time, float start, float end)
{
    return 1.0f - fadeIn(time, start, end);
}

// Return the combined blend/fade value for both in and out transition.
inline constexpr float fade(float time, float inStart, float inEnd, float outStart, float outEnd)
{
    return std::min(fadeIn(time, inStart, inEnd), fadeOut(time, outStart, outEnd));
}