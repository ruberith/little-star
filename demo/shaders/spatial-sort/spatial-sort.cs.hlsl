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

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    const uint i = thread.x;
    const uint j = i ^ _.dist;
    const bool increase = (i & _.peak) == 0;

    if (i < j) // Do not swap back.
    {
        // Compare the order of the elements with the order of the monotonic sequence.
        if ((increase && (spat[i].h > spat[j].h)) || (!increase && (spat[i].h < spat[j].h)))
        {
            // Swap the elements so that the orders match.
            const Spatial spat_i = spat[i];
            spat[i] = spat[j];
            spat[j] = spat_i;
        }
    }
}