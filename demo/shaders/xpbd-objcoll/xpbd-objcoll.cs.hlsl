struct PushConstant
{
    // particle count
    uint n;
};
[[vk::push_constant]] PushConstant _;

struct PlayerCollision
{
    // AABB minimum position
    float4 x_min;
    // AABB maximum position
    float4 x_max;
    // bounding sphere positions (xyz) and radii (w)
    float4 sphere[18];
    // capsule AABB minimum positions
    float4 c_min[13];
    // capsule AABB maximum positions
    float4 c_max[13];
    // bounding sphere index pair (xy) forming a capsule
    uint4 caps[13];
};
[[vk::binding(0)]] ConstantBuffer<PlayerCollision> player;

// predicted positions
[[vk::binding(1)]] StructuredBuffer<float4> x_;
// particle radii
[[vk::binding(2)]] StructuredBuffer<float> r;
// position deltas
[[vk::binding(3)]] RWStructuredBuffer<float4> dx;

void collideMoon(uint i)
{
    // moon radius
    static const float r_j = 20.0;

    // Calculate the penetration depth.
    const float d = (r[i] + r_j) - length(x_[i].xyz);
    if (d > 0.0)
    {
        // Resolve the penetration.
        const float3 n = normalize(x_[i].xyz);
        dx[i].xyz += d * n;
    }
}

// Return true if the axis-aligned bounding boxes overlap. Return false otherwise.
bool overlap(float3 a_min, float3 a_max, float3 b_min, float3 b_max)
{
    return (a_min.x < b_max.x &&
            a_min.y < b_max.y &&
            a_min.z < b_max.z &&
            a_max.x > b_min.x &&
            a_max.y > b_min.y &&
            a_max.z > b_min.z);
}

void collidePlayer(uint i)
{
    const float3 x_i_min = x_[i].xyz - r[i];
    const float3 x_i_max = x_[i].xyz + r[i];

    // Check for an overlap with the player AABB.
    if (!overlap(x_i_min, x_i_max, player.x_min.xyz, player.x_max.xyz))
    {
        return;
    }

    for (uint j = 0; j < 13; j++)
    {
        // Check for an overlap with each capsule AABB.
        if (overlap(x_i_min, x_i_max, player.c_min[j].xyz, player.c_max[j].xyz))
        {
            // Find the nearest point of the capsule.
            const float4 A = player.sphere[player.caps[j].x];
            const float4 B = player.sphere[player.caps[j].y];
            const float3 AB = B.xyz - A.xyz;
            const float t = saturate(dot(x_[i].xyz - A.xyz, AB.xyz) / dot(AB.xyz, AB.xyz));
            const float3 x_j = A.xyz + t * AB;
            const float r_j = lerp(A.w, B.w, t);
            
            // Calculate the penetration depth.
            const float3 x_ij = x_[i].xyz - x_j;
            const float d = (r[i] + r_j) - length(x_ij);
            if (d > 0.0)
            {
                // Resolve the penetration.
                const float3 n = normalize(x_ij);
                dx[i].xyz += d * n;
            }
        }
    }
}

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    const uint i = thread.x;
    if (i >= _.n)
    {
        return;
    }

    // Calculate the position corrections due to object collisions via (X)PBD.
    collideMoon(i);
    collidePlayer(i);
}