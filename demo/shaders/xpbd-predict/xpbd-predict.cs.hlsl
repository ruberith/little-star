struct PushConstant
{
    // time step
    float dt;
    // moon gravity
    float g;
    // particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

// particle positions
[[vk::binding(0)]] StructuredBuffer<float4> x;
// particle velocities
[[vk::binding(1)]] StructuredBuffer<float4> v;
// predicted positions
[[vk::binding(2)]] RWStructuredBuffer<float4> x_;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    static const float dt_sq_g = _.dt * _.dt * _.g;

    const uint i = thread.x;
    if (i >= _.n)
    {
        return;
    }

    // Predict the position after the substep.
    x_[i].xyz = x[i].xyz + _.dt * v[i].xyz + dt_sq_g * normalize(x[i].xyz);
}