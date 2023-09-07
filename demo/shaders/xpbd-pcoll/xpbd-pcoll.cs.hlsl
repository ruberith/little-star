#include <spatial.hlsl>
#include <state.hlsl>

struct PushConstant
{
    // particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

// predicted positions
[[vk::binding(0)]] StructuredBuffer<float4> x_;
// spatial indices
[[vk::binding(1)]] StructuredBuffer<Spatial> spat;
// hash value => spatial index range of grid cell
[[vk::binding(2)]] StructuredBuffer<uint2> cell;
// particle radii
[[vk::binding(3)]] StructuredBuffer<float> r;
// particle weights (= inverse masses)
[[vk::binding(4)]] StructuredBuffer<float> w;
// particle states
[[vk::binding(5)]] StructuredBuffer<uint> state;
// position deltas
[[vk::binding(6)]] RWStructuredBuffer<float4> dx;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= _.n || state[spat[thread.x].i] == STATIC)
    {
        return;
    }

    // Calculate the position corrections due to particle collisions via (X)PBD.
    const uint h = spat[thread.x].h;
    const uint i = spat[thread.x].i;
    for (uint idx = cell[h].x; idx <= cell[h].y; idx++)
    {
        const uint j = spat[idx].i;
        if (i != j)
        {
            // Calculate the penetration depth.
            const float3 x_ij = x_[i].xyz - x_[j].xyz;
            const float d = (r[i] + r[j]) - length(x_ij);
            if (d > 0.0)
            {
                // Resolve the penetration.
                dx[i].xyz += d * w[i] / (w[i] + w[j]) * normalize(x_ij);
            }
        }
    }
}