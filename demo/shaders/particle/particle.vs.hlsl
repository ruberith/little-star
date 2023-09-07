struct Transform
{
    // view-projection matrix
    float4x4 viewProjection;
    // particle radius
    float radius;
};
[[vk::push_constant]] Transform transform;

// particle positions
[[vk::binding(0)]] StructuredBuffer<float4> positions;

struct Input
{
    // particle index
    uint i : SV_InstanceID;
    // local position
    [[vk::location(0)]] float3 position : POSITION;
};

struct Output
{
    // clip-space position
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.position = float4(positions[input.i].xyz + transform.radius * input.position, 1.0);
    output.position = mul(transform.viewProjection, output.position);
    return output;
}