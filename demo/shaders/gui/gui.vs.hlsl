struct Transform
{
    // scale vector
    float2 scale;
    // translation vector
    float2 translate;
};
[[vk::push_constant]] Transform transform;

struct Input
{
    // local position
    [[vk::location(0)]] float2 position : POSITION;
    // texcoord
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
    // color
    [[vk::location(2)]] float4 color : COLOR;
};

struct Output
{
    // clip-space position
    float4 position : SV_Position;
    // color
    [[vk::location(0)]] float4 color : COLOR;
    // texcoord
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
};

Output main(Input input)
{
    Output output;
    output.position = float4(input.position * transform.scale + transform.translate, 0.0, 1.0);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}
