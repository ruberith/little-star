#pragma once

#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/hash.hpp>

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif
#include <vulkan/vulkan_hash.hpp>

// mesh vertex
struct Vertex
{
    // vertex position
    glm::float3 position{};
    // vertex normal
    glm::float3 normal{};
    // vertex tangent
    glm::float4 tangent{};
    // vertex texcoords
    glm::float2 texcoord[5]{};
    // vertex color
    glm::float4 color{1.0f};
    // vertex joints
    glm::uvec4 joints{};
    // vertex weights
    glm::float4 weights{};

    // vertex input binding descriptions
    static constexpr std::array bindingDescriptions{
        vk::VertexInputBindingDescription{0, sizeof(position), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{1, sizeof(normal), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{2, sizeof(tangent), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{3, sizeof(texcoord[0]), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{4, sizeof(texcoord[1]), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{5, sizeof(texcoord[2]), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{6, sizeof(texcoord[3]), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{7, sizeof(texcoord[4]), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{8, sizeof(color), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{9, sizeof(joints), vk::VertexInputRate::eVertex},
        vk::VertexInputBindingDescription{10, sizeof(weights), vk::VertexInputRate::eVertex},
    };

    // vertex input attribute descriptions
    static constexpr std::array attributeDescriptions{
        vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, 0},
        vk::VertexInputAttributeDescription{1, 1, vk::Format::eR32G32B32Sfloat, 0},
        vk::VertexInputAttributeDescription{2, 2, vk::Format::eR32G32B32A32Sfloat, 0},
        vk::VertexInputAttributeDescription{3, 3, vk::Format::eR32G32Sfloat, 0},
        vk::VertexInputAttributeDescription{4, 4, vk::Format::eR32G32Sfloat, 0},
        vk::VertexInputAttributeDescription{5, 5, vk::Format::eR32G32Sfloat, 0},
        vk::VertexInputAttributeDescription{6, 6, vk::Format::eR32G32Sfloat, 0},
        vk::VertexInputAttributeDescription{7, 7, vk::Format::eR32G32Sfloat, 0},
        vk::VertexInputAttributeDescription{8, 8, vk::Format::eR32G32B32A32Sfloat, 0},
        vk::VertexInputAttributeDescription{9, 9, vk::Format::eR32G32B32A32Uint, 0},
        vk::VertexInputAttributeDescription{10, 10, vk::Format::eR32G32B32A32Sfloat, 0},
    };

    // Return true if the positions, normals, and first texcoords of the vertices match. Return false otherwise.
    bool operator==(const Vertex& vertex) const
    {
        return position == vertex.position && normal == vertex.normal && texcoord[0] == vertex.texcoord[0];
    }
};

namespace std
{
// vertex hash
template <> struct hash<Vertex>
{
    // Return the hash value for the given vertex.
    size_t operator()(const Vertex& vertex) const
    {
        size_t h = 0;
        VULKAN_HPP_HASH_COMBINE(h, vertex.position);
        VULKAN_HPP_HASH_COMBINE(h, vertex.normal);
        VULKAN_HPP_HASH_COMBINE(h, vertex.texcoord[0]);
        return h;
    }
};
} // namespace std