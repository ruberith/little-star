#pragma once

#include <glm/gtx/compatibility.hpp>

// distance constraint
struct DistanceConstraint
{
    // particle indices
    alignas(4) glm::uint i;
    alignas(4) glm::uint j;
    // distance
    alignas(4) float d;
    // compliance
    alignas(4) float alpha;
};

// volume constraint
struct VolumeConstraint
{
    // particle indices
    alignas(4) glm::uint i;
    alignas(4) glm::uint j;
    alignas(4) glm::uint k;
    alignas(4) glm::uint l;
    // volume
    alignas(4) float V;
    // compliance
    alignas(4) float alpha;
};

// spatial index
struct Spatial
{
    // hash value
    alignas(4) glm::uint h;
    // particle index
    alignas(4) glm::uint i;
};

// particle state
enum struct State : glm::uint
{
    FREE = 0,
    PLAYER = 1,
    STAR = 2,
    STATIC = 3,
};