struct PushConstant
{
    // time step
    float dt;
    // constraint count
    uint n;
};
[[vk::push_constant]] PushConstant _;

struct DistanceConstraint
{
    // particle indices
    uint i;
    uint j;
    // distance
    float d;
    // compliance
    float alpha;
};
// distance constraints
[[vk::binding(0)]] StructuredBuffer<DistanceConstraint> constr;
// predicted positions
[[vk::binding(1)]] StructuredBuffer<float4> x_;
// particle weights (= inverse masses)
[[vk::binding(2)]] StructuredBuffer<float> w;
// position deltas (* 10^7)
[[vk::binding(3)]] RWStructuredBuffer<int> dxE7;

[numthreads(g_n, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    static const float dt_sq_inv = 1.0 / (_.dt * _.dt); 

    if (thread.x >= _.n)
    {
        return;
    }
    const uint i = constr[thread.x].i;
    const uint j = constr[thread.x].j;
    const float d = constr[thread.x].d;
    const float alpha = constr[thread.x].alpha * dt_sq_inv;

    // Calculate the position corrections due to the distance constraint via XPBD.
    const float3 x_ij = x_[i].xyz - x_[j].xyz;
    const float C = length(x_ij) - d;
    const float3 n = normalize(x_ij);
    const float dlambda = -C / (w[i] + w[j] + alpha);
    const int3 dxE7_i = int3(1.E7 * dlambda * w[i] * n);
    const int3 dxE7_j = int3(-1.E7 * dlambda * w[j] * n);
    InterlockedAdd(dxE7[4 * i], dxE7_i[0]);
    InterlockedAdd(dxE7[4 * i + 1], dxE7_i[1]);
    InterlockedAdd(dxE7[4 * i + 2], dxE7_i[2]);
    InterlockedAdd(dxE7[4 * j], dxE7_j[0]);
    InterlockedAdd(dxE7[4 * j + 1], dxE7_j[1]);
    InterlockedAdd(dxE7[4 * j + 2], dxE7_j[2]);
}