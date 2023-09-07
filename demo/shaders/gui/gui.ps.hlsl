// font texture & sampler
[[vk::binding(0)]][[vk::combinedImageSampler]] Texture2D<float4> fontTexture;
[[vk::binding(0)]][[vk::combinedImageSampler]] SamplerState fontSampler;

struct Input
{
    // color
    [[vk::location(0)]] float4 color : COLOR;
    // texcoord
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
};

struct Output
{
    // color
    [[vk::location(0)]] float4 color : SV_Target;
};

Output main(Input input)
{
    Output output;
    output.color = input.color * fontTexture.Sample(fontSampler, input.texcoord);
    return output;
}