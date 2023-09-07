#pragma once

#include "Utils.h"
#include "Vertex.h"
#include <tiny_obj_loader.h>

// surface mesh
template <typename Index = uint32_t> struct SurfaceMesh
{
    // vertex positions
    std::vector<glm::float3> positions;
    // vertex texcoords
    std::vector<glm::float2> texcoords;
    // vertex normals
    std::vector<glm::float3> normals;
    // indices
    std::vector<Index> indices;

    // Load a surface mesh.
    static SurfaceMesh load(const std::string& name, const std::string& extension = "obj")
    {
        using namespace tinyobj;

        if (extension != "obj")
        {
            throw std::invalid_argument("Failed to load surface mesh [" + name + "]: unsupported extension " +
                                        extension);
        }

        attrib_t attributes;
        std::vector<shape_t> shapes;
        std::vector<material_t> materials;
        std::string error;
        const std::string path = meshPath(name, extension).string();
        if (!LoadObj(&attributes, &shapes, &materials, &error, path.data()))
        {
            throw std::runtime_error("Failed to load surface mesh [" + name + "]" +
                                     (error.empty() ? "" : (": " + error)));
        }
        const auto& positions = attributes.vertices;
        const auto& texcoords = attributes.texcoords;
        const auto& normals = attributes.normals;
        const uint32_t vertexCount = static_cast<uint32_t>(positions.size()) / 3;

        std::unordered_map<Vertex, Index> verticesToIndices;
        SurfaceMesh data;
        verticesToIndices.reserve(vertexCount);
        data.positions.reserve(vertexCount);
        data.texcoords.reserve(vertexCount);
        data.normals.reserve(vertexCount);
        for (const shape_t& shape : shapes)
        {
            const auto& indices = shape.mesh.indices;
            data.indices.reserve(data.indices.size() + indices.size());
            for (const index_t& index : indices)
            {
                Vertex vertex{
                    .position =
                        {
                            positions[3 * index.vertex_index],
                            positions[3 * index.vertex_index + 1],
                            positions[3 * index.vertex_index + 2],
                        },
                    .normal =
                        {
                            normals[3 * index.normal_index],
                            normals[3 * index.normal_index + 1],
                            normals[3 * index.normal_index + 2],
                        },
                    .texcoord =
                        {
                            {
                                texcoords[2 * index.texcoord_index],
                                1.0f - texcoords[2 * index.texcoord_index + 1],
                            },
                            {},
                            {},
                            {},
                            {},
                        },
                };
                // Only store unique vertices and adapt the indices.
                if (!verticesToIndices.contains(vertex))
                {
                    verticesToIndices[vertex] = static_cast<Index>(data.positions.size());
                    data.positions.emplace_back(vertex.position);
                    data.texcoords.emplace_back(vertex.texcoord[0]);
                    data.normals.emplace_back(vertex.normal);
                }
                data.indices.emplace_back(verticesToIndices[vertex]);
            }
        }
        return data;
    }
};