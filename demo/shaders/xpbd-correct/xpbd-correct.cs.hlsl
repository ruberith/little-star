#include <state.hlsl>

struct PushConstant
{
    // time step
    float dt;
    // particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

// predicted positions
[[vk::binding(0)]] StructuredBuffer<float4> x_;
// position deltas
[[vk::binding(1)]] StructuredBuffer<float4> dx;
// position deltas (* 10^7)
[[vk::binding(2)]] StructuredBuffer<int4> dxE7;
// particle states
[[vk::binding(3)]] StructuredBuffer<uint> state;
// particle positions
[[vk::binding(4)]] RWStructuredBuffer<float4> x;
// particle velocities
[[vk::binding(5)]] RWStructuredBuffer<float4> v;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    static const float dt_inv = 1.0 / _.dt;
    static const float v_max = 0.01 * dt_inv;

    const uint i = thread.x;
    if (i >= _.n || state[i] == STATIC)
    {
        return;
    }

    // Update the position using the Jacobi method and correct the velocity.
    const float3 _x_i = x[i].xyz;
    x[i].xyz = x_[i].xyz + 0.25 * (dx[i].xyz + 1.E-7 * float3(dxE7[i].xyz));
    v[i].xyz = clamp(dt_inv * (x[i].xyz - _x_i), -v_max, v_max);
}