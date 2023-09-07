struct Transform
{
    // model-view-projection matrix
    float4x4 modelViewProjection;
};
[[vk::push_constant]] Transform transform;

struct Input
{
    // local position
    [[vk::location(0)]] float3 position : POSITION;
};

struct Output
{
    // clip-space position
    float4 position : SV_Position;
    // texcoord
    [[vk::location(0)]] float3 texcoord : TEXCOORD;
};

Output main(Input input)
{
    Output output;
    output.position = mul(transform.modelViewProjection, float4(input.position, 1.0)).xyww;
    output.texcoord = input.position;
    return output;
}