#include <state.hlsl>

struct PushConstant
{
    // player position
    float4 x_player;
    // star particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

// particle positions
[[vk::binding(0)]] RWStructuredBuffer<float4> x;
// particle velocities
[[vk::binding(1)]] RWStructuredBuffer<float4> v;
// particle states
[[vk::binding(2)]] RWStructuredBuffer<uint> state;
// particle counter
[[vk::binding(3)]] RWStructuredBuffer<uint> counter;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    static const float3 up = normalize(_.x_player.xyz);
    static const float3 x_player = _.x_player.xyz + 1.6 * up;
    static const float3 x_over_player = _.x_player.xyz + 3.5 * up;
    static const float3 x_star = {0.0, 30.0, 0.0};

    const uint i = thread.x;
    if (i >= _.n || state[i] == STATIC)
    {
        return;
    }
    else if (state[i] == FREE)
    {
        // The particle is free.
        if (distance(x[i].xyz, x_player) < 2.0)
        {
            state[i] = PLAYER;
        }
    }
    else if (state[i] == PLAYER)
    {
        // The particle is attracted to the player.
        v[i].xyz = 2.5 * normalize(x_over_player - x[i].xyz);
        if (distance(x[i].xyz, x_star) < 11.0)
        {
            state[i] = STAR;
        }
    }
    else if (state[i] == STAR)
    {
        // The particle is attracted to the star.
        v[i].xyz = 5.0 * normalize(x_star - x[i].xyz);
        if (distance(x[i].xyz, x_star) < 0.9)
        {
            x[i].xyz = x_star;
            state[i] = STATIC;
            // Count the particles at the star.
            InterlockedAdd(counter[0], 1);
        }
    }
}