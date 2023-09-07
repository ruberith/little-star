#include <spatial.hlsl>

struct PushConstant
{
    // mask defining peaks of bitonic sequences (before peak if i & peak = 0)
    uint peak;
    // distance between elements to compare (i ^ dist = j and j ^ dist = i)
    uint dist;
};
[[vk::push_constant]] PushConstant _;

// spatial indices
[[vk::binding(0)]] RWStructuredBuffer<Spatial> spat;

// group-shared spatial indices
groupshared Spatial g_spat[g_n];

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID, uint3 g_thread : SV_GroupThreadID)
{
    const uint i = thread.x;
    const uint g_i = g_thread.x;
    const bool increase = (i & _.peak) == 0;

    g_spat[g_i] = spat[i];

    GroupMemoryBarrierWithGroupSync();

    for (uint dist = _.dist; dist > 0; dist >>= 1)
    {
        const uint g_j = g_i ^ dist;

        if (g_i < g_j) // Do not swap back.
        {
            // Compare the order of the elements with the order of the monotonic sequence.
            if ((increase && (g_spat[g_i].h > g_spat[g_j].h)) || (!increase && (g_spat[g_i].h < g_spat[g_j].h)))
            {
                // Swap the elements so that the orders match.
                const Spatial g_spat_g_i = g_spat[g_i];
                g_spat[g_i] = g_spat[g_j];
                g_spat[g_j] = g_spat_g_i;
            }
        }

        GroupMemoryBarrierWithGroupSync();
    }

    spat[i] = g_spat[g_i];
}