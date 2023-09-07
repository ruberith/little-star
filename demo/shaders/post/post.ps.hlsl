struct PushConstant
{
    // Enable cel shading?
    uint cel;
    // cinematic bar height
    float bar;
    // viewport height
    float height;
    // frame brightness
    float brightness;
};
[[vk::push_constant]] PushConstant _;

// color texture & sampler
[[vk::binding(0)]][[vk::combinedImageSampler]] Texture2D<float4> colorTexture;
[[vk::binding(0)]][[vk::combinedImageSampler]] SamplerState colorSampler;
// depth texture & sampler
[[vk::binding(1)]][[vk::combinedImageSampler]] Texture2D<float> depthTexture;
[[vk::binding(1)]][[vk::combinedImageSampler]] SamplerState depthSampler;
// normal texture & sampler
[[vk::binding(2)]][[vk::combinedImageSampler]] Texture2D<float4> normalTexture;
[[vk::binding(2)]][[vk::combinedImageSampler]] SamplerState normalSampler;

struct Input
{
    // screen-space position
    float4 position : SV_Position;
    // texcoord
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

struct Output
{
    // color
    [[vk::location(0)]] float4 color : SV_Target;
};

// Calculate the gradient magnitude as an indicator for the edge strength using the Sobel filter.
template<typename T>
float sobelEdge(Texture2D<T> texture, SamplerState S, float2 texcoord)
{
    T sampleN = texture.Sample(S, texcoord, int2(0, -1));
    T sampleNE = texture.Sample(S, texcoord, int2(1, -1));
    T sampleE = texture.Sample(S, texcoord, int2(1, 0));
    T sampleSE = texture.Sample(S, texcoord, int2(1, 1));
    T sampleS = texture.Sample(S, texcoord, int2(0, 1));
    T sampleSW = texture.Sample(S, texcoord, int2(-1, 1));
    T sampleW = texture.Sample(S, texcoord, int2(-1, 0));
    T sampleNW = texture.Sample(S, texcoord, int2(-1, -1));

    T gradientX = sampleNW + 2.0 * sampleW + sampleSW - sampleNE - 2.0 * sampleE - sampleSE;
    T gradientY = sampleNW + 2.0 * sampleN + sampleNE - sampleSW - 2.0 * sampleS - sampleSE;

    return sqrt(dot(gradientX, gradientX) + dot(gradientY, gradientY));
}

Output main(Input input)
{
    static const float edgeThreshold = 0.33;
    static const float3 outlineColor = 0.0;
    static const float levelCount = 256.0;

    Output output;

    const float y = input.position.y / _.height;
    if (y < _.bar || y > (1.0 - _.bar))
    {
        output.color = float4(0.0, 0.0, 0.0, 1.0);
        return output;
    }

    output.color = colorTexture.Sample(colorSampler, input.texcoord);
    if (_.cel == 1)
    {
        // Detect edges and draw the outline.
        float colorEdge = 0.1 * sobelEdge(colorTexture, colorSampler, input.texcoord);
        float depthEdge = 100.0 * depthTexture.Sample(depthSampler, input.texcoord) * sobelEdge(depthTexture, depthSampler, input.texcoord);
        float normalEdge = 0.1 * sobelEdge(normalTexture, normalSampler, input.texcoord);
        float edge = saturate(max(max(colorEdge, depthEdge), normalEdge));
        if (edge > edgeThreshold)
        {
            output.color.rgb = lerp(output.color.rgb, outlineColor, edge);
        }
        
        // Quantize the colors.
        const float gray = (output.color.r + output.color.g + output.color.b) / 3.0;
        output.color.rgb += floor(gray * levelCount) / levelCount - gray;
    }
    output.color.rgb *= _.brightness;
    
    return output;
}