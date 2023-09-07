struct PushConstant
{
    // time step
    float dt;
    // constraint count
    uint n;
};
[[vk::push_constant]] PushConstant _;

struct VolumeConstraint
{
    // particle indices
    uint i;
    uint j;
    uint k;
    uint l;
    // volume
    float V;
    // compliance
    float alpha;
};
// volume constraints
[[vk::binding(0)]] StructuredBuffer<VolumeConstraint> constr;
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
    static const float sixth = 1.0 / 6.0;

    if (thread.x >= _.n)
    {
        return;
    }
    const uint i = constr[thread.x].i;
    const uint j = constr[thread.x].j;
    const uint k = constr[thread.x].k;
    const uint l = constr[thread.x].l;
    const float V = constr[thread.x].V;
    const float alpha = constr[thread.x].alpha * dt_sq_inv;

    // Calculate the position corrections due to the volume constraint via XPBD.
    const float3 x_i = x_[i].xyz;
    const float3 x_j = x_[j].xyz;
    const float3 x_k = x_[k].xyz;
    const float3 x_l = x_[l].xyz;
    const float3 x_ji = x_j - x_i;
    const float3 x_ki = x_k - x_i;
    const float3 x_li = x_l - x_i;
    const float3 x_kj = x_k - x_j;
    const float3 x_lj = x_l - x_j;
    const float C = sixth * dot(cross(x_ji, x_ki), x_li) - V;
    const float3 n_i = sixth * cross(x_lj, x_kj);
    const float3 n_j = sixth * cross(x_ki, x_li);
    const float3 n_k = sixth * cross(x_li, x_ji);
    const float3 n_l = sixth * cross(x_ji, x_ki);
    const float dlambda = -C / (
        w[i] * dot(n_i, n_i) +
        w[j] * dot(n_j, n_j) +
        w[k] * dot(n_k, n_k) +
        w[l] * dot(n_l, n_l) + alpha);
    const int3 dxE7_i = int3(1.E7 * dlambda * w[i] * n_i);
    const int3 dxE7_j = int3(1.E7 * dlambda * w[j] * n_j);
    const int3 dxE7_k = int3(1.E7 * dlambda * w[k] * n_k);
    const int3 dxE7_l = int3(1.E7 * dlambda * w[l] * n_l);
    InterlockedAdd(dxE7[4 * i], dxE7_i[0]);
    InterlockedAdd(dxE7[4 * i + 1], dxE7_i[1]);
    InterlockedAdd(dxE7[4 * i + 2], dxE7_i[2]);
    InterlockedAdd(dxE7[4 * j], dxE7_j[0]);
    InterlockedAdd(dxE7[4 * j + 1], dxE7_j[1]);
    InterlockedAdd(dxE7[4 * j + 2], dxE7_j[2]);
    InterlockedAdd(dxE7[4 * k], dxE7_k[0]);
    InterlockedAdd(dxE7[4 * k + 1], dxE7_k[1]);
    InterlockedAdd(dxE7[4 * k + 2], dxE7_k[2]);
    InterlockedAdd(dxE7[4 * l], dxE7_l[0]);
    InterlockedAdd(dxE7[4 * l + 1], dxE7_l[1]);
    InterlockedAdd(dxE7[4 * l + 2], dxE7_l[2]);
}