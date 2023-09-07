#include <spatial.hlsl>

struct PushConstant
{
    // frame time step
    float dt;
    // moon gravity
    float g;
    // cell size
    float l;
    // particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

// particle positions
[[vk::binding(0)]] StructuredBuffer<float4> x;
// particle velocities
[[vk::binding(1)]] StructuredBuffer<float4> v;
// spatial indices
[[vk::binding(2)]] RWStructuredBuffer<Spatial> spat;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    static const float dt_sq_g = _.dt * _.dt * _.g;

    const uint i = thread.x;
    if (i >= _.n)
    {
        return;
    }

    // Calculate the cell index for the predicted position after a full time step.
    const float3 x_i = x[i].xyz + _.dt * v[i].xyz + dt_sq_g * normalize(x[i].xyz);
    const uint3 c_i = uint3(
        asuint(floor(x_i.x / _.l)),
        asuint(floor(x_i.y / _.l)),
        asuint(floor(x_i.z / _.l))
    );

    // Hash the cell index and store it together with the particle index.
    spat[i].h = ((73856093 * c_i.x) ^ (19349663 * c_i.y) ^ (83492791 * c_i.z)) % _.n;
    spat[i].i = i;
}