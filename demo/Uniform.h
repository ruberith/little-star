#pragma once

#include <array>
#include <glm/gtx/compatibility.hpp>

// scene uniform
struct SceneUniform
{
    // view position
    alignas(16) glm::float4 viewPosition{};
    // light position
    alignas(16) glm::float4 lightPosition{};
    // light intensity [cd]
    alignas(16) glm::float4 lightIntensity{};
    // exposure value at ISO 100
    alignas(4) float EV100{};
    // exposure [lx * s]
    alignas(4) float exposure{};
};

// transform uniform
struct TransformUniform
{
    // view-projection matrix
    alignas(16) glm::float4x4 viewProjection{1.0f};
};

// view-projection uniform
struct ViewProjectionUniform
{
    // camera view-projection matrix
    alignas(16) glm::float4x4 camera{1.0f};
    // shadow view-projection matrix
    alignas(16) glm::float4x4 shadow{1.0f};
};

// skin uniform
struct SkinUniform
{
    // joint matrices
    alignas(16) std::array<glm::float4x4, 128> joint{};
    // joint count
    alignas(4) glm::uint jointCount{};
};

// material uniform
struct MaterialUniform
{
    // PBR workflow
    enum struct PbrWorkflow : glm::uint
    {
        MetallicRoughness = 0,
        SpecularGlossiness = 1,
    };
    // color factor
    alignas(16) glm::float4 colorFactor{1.0f};
    // PBR factor
    alignas(16) glm::float4 pbrFactor{1.0f};
    // emissive factor
    alignas(16) glm::float4 emissiveFactor{};
    // normal scale
    alignas(4) float normalScale{1.0f};
    // occlusion strength
    alignas(4) float occlusionStrength{1.0f};
    // color texcoord index
    alignas(4) glm::uint colorTexcoord{};
    // PBR texcoord index
    alignas(4) glm::uint pbrTexcoord{};
    // normal texcoord index
    alignas(4) glm::uint normalTexcoord{};
    // occlusion texcoord index
    alignas(4) glm::uint occlusionTexcoord{};
    // emissive texcoord index
    alignas(4) glm::uint emissiveTexcoord{};
    // PBR workflow index
    alignas(4) glm::uint workflow{};
};

// player collision uniform
struct PlayerCollisionUniform
{
    // AABB minimum position
    alignas(16) glm::float4 x_min{};
    // AABB maximum position
    alignas(16) glm::float4 x_max{};
    // bounding sphere positions (xyz) and radii (w)
    alignas(16) std::array<glm::float4, 18> sphere{};
    // capsule AABB minimum positions
    alignas(16) std::array<glm::float4, 13> c_min{};
    // capsule AABB maximum positions
    alignas(16) std::array<glm::float4, 13> c_max{};
    // bounding sphere index pair (xy) forming a capsule
    alignas(16) std::array<glm::uvec4, 13> caps{};
};