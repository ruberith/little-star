// skybox texture & sampler
[[vk::binding(0)]][[vk::combinedImageSampler]] TextureCube<float4> skyboxTexture;
[[vk::binding(0)]][[vk::combinedImageSampler]] SamplerState skyboxSampler;

struct Input
{
    // texcoord
    [[vk::location(0)]] float3 texcoord : TEXCOORD;
};

struct Output
{
    // color
    [[vk::location(0)]] float4 color : SV_Target0;
    // global normal
    [[vk::location(1)]] float4 normal : SV_Target1;
};

Output main(Input input)
{
    Output output;
    output.color = skyboxTexture.Sample(skyboxSampler, input.texcoord);
    return output;
}