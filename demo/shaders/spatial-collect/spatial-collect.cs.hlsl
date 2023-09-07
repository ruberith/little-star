#include <spatial.hlsl>

struct PushConstant
{
    // particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

// spatial indices
[[vk::binding(0)]] StructuredBuffer<Spatial> spat;
// hash value => spatial index range of grid cell
[[vk::binding(1)]] RWStructuredBuffer<uint2> cell;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    const uint i = thread.x;
    if (i >= _.n)
    {
        return;
    }

    // Store the bounds for each grid cell.
    if (i == 0 || spat[i - 1].h != spat[i].h)
    {
        cell[spat[i].h].x = i;
    }
    if (i == _.n - 1 || spat[i].h != spat[i + 1].h)
    {
        cell[spat[i].h].y = i;
    }
}