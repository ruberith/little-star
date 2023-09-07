#pragma once

#include "Data.h"
#include <mikktspace.h>
#include <stdexcept>

namespace TangentSpace
{
// user data
struct UserData
{
    // face count
    uint32_t faceCount{};
    // index size
    uint32_t indexSize{};
    // index data
    Data indices;
    // position data
    Data positions;
    // normal data
    Data normals;
    // texcoord data
    Data texcoords;
    // tangent data
    Data tangents;

    // Return the index of the specified face vertex.
    uint32_t index(const int face, const int vertex)
    {
        switch (indexSize)
        {
        case 4:
            return *(static_cast<uint32_t*>(indices()) + face * 3 + vertex);
        case 2:
            return *(static_cast<uint16_t*>(indices()) + face * 3 + vertex);
        default:
            throw std::runtime_error("unsupported index size");
        }
    }
};

// Return the interface for the MikkTSpace generator.
inline SMikkTSpaceInterface Interface()
{
    return SMikkTSpaceInterface{
        .m_getNumFaces = [](const SMikkTSpaceContext* context) -> int {
            UserData* userData = static_cast<UserData*>(context->m_pUserData);
            return userData->faceCount;
        },
        .m_getNumVerticesOfFace = [](const SMikkTSpaceContext* context, const int face) -> int { return 3; },
        .m_getPosition = [](const SMikkTSpaceContext* context, float position[], const int face,
                            const int vertex) -> void {
            UserData* userData = static_cast<UserData*>(context->m_pUserData);
            position = static_cast<float*>(userData->positions()) + userData->index(face, vertex) * 3;
        },
        .m_getNormal = [](const SMikkTSpaceContext* context, float normal[], const int face, const int vertex) -> void {
            UserData* userData = static_cast<UserData*>(context->m_pUserData);
            normal = static_cast<float*>(userData->normals()) + userData->index(face, vertex) * 3;
        },
        .m_getTexCoord = [](const SMikkTSpaceContext* context, float texcoord[], const int face,
                            const int vertex) -> void {
            UserData* userData = static_cast<UserData*>(context->m_pUserData);
            texcoord = static_cast<float*>(userData->texcoords()) + userData->index(face, vertex) * 2;
        },
        .m_setTSpaceBasic = [](const SMikkTSpaceContext* context, const float tangent[], const float sign,
                               const int face, const int vertex) -> void {
            UserData* userData = static_cast<UserData*>(context->m_pUserData);
            float* dst = static_cast<float*>(userData->tangents()) + userData->index(face, vertex) * 4;
            dst[0] = tangent[0];
            dst[1] = tangent[1];
            dst[2] = tangent[2];
            dst[3] = sign;
        },
    };
};

// Generate the tangent space for the specified data.
// Fill the tangents member of userData for which memory must be allocated.
inline void generate(SMikkTSpaceInterface& Interface, UserData& userData)
{
    SMikkTSpaceContext context{
        .m_pInterface = &Interface,
        .m_pUserData = &userData,
    };
    if (!genTangSpaceDefault(&context))
    {
        throw std::runtime_error("Failed to generate tangent space");
    }
};
} // namespace TangentSpace