struct Mesh
{
    // embedding index
    [[vk::offset(64)]] uint embedding;
};
[[vk::push_constant]] Mesh mesh;

struct Scene
{
    // view position
    float4 viewPosition;
    // light position
    float4 lightPosition;
    // light intensity [cd]
    float4 lightIntensity;
    // exposure value at ISO 100
    float EV100;
    // exposure [lx * s]
    float exposure;
};
[[vk::binding(0, 0)]] ConstantBuffer<Scene> scene;

// BRDF texture & sampler
[[vk::binding(1, 0)]][[vk::combinedImageSampler]] Texture2D<float4> brdfTexture;
[[vk::binding(1, 0)]][[vk::combinedImageSampler]] SamplerState brdfSampler;
// irradiance texture & sampler
[[vk::binding(2, 0)]][[vk::combinedImageSampler]] TextureCube<float4> irradianceTexture;
[[vk::binding(2, 0)]][[vk::combinedImageSampler]] SamplerState irradianceSampler;
// radiance texture & sampler
[[vk::binding(3, 0)]][[vk::combinedImageSampler]] TextureCube<float4> radianceTexture;
[[vk::binding(3, 0)]][[vk::combinedImageSampler]] SamplerState radianceSampler;
// shadow texture & sampler
[[vk::binding(5, 0)]] Texture2D<float> shadowTexture;
[[vk::binding(6, 0)]] SamplerComparisonState shadowSampler;

// color texture & sampler
[[vk::binding(0, 1)]][[vk::combinedImageSampler]] Texture2D<float4> colorTexture;
[[vk::binding(0, 1)]][[vk::combinedImageSampler]] SamplerState colorSampler;
// PBR texture & sampler
[[vk::binding(1, 1)]][[vk::combinedImageSampler]] Texture2D<float4> pbrTexture;
[[vk::binding(1, 1)]][[vk::combinedImageSampler]] SamplerState pbrSampler;
// normal texture & sampler
[[vk::binding(2, 1)]][[vk::combinedImageSampler]] Texture2D<float4> normalTexture;
[[vk::binding(2, 1)]][[vk::combinedImageSampler]] SamplerState normalSampler;
// occlusion texture & sampler
[[vk::binding(3, 1)]][[vk::combinedImageSampler]] Texture2D<float4> occlusionTexture;
[[vk::binding(3, 1)]][[vk::combinedImageSampler]] SamplerState occlusionSampler;
// emissive texture & sampler
[[vk::binding(4, 1)]][[vk::combinedImageSampler]] Texture2D<float4> emissiveTexture;
[[vk::binding(4, 1)]][[vk::combinedImageSampler]] SamplerState emissiveSampler;

struct Material
{
    // color factor
    float4 colorFactor;
    // PBR factor
    float4 pbrFactor;
    // emissive factor
    float4 emissiveFactor;
    // normal scale
    float normalScale;
    // occlusion strength
    float occlusionStrength;
    // color texcoord index
    uint colorTexcoord;
    // PBR texcoord index
    uint pbrTexcoord;
    // normal texcoord index
    uint normalTexcoord;
    // occlusion texcoord index
    uint occlusionTexcoord;
    // emissive texcoord index
    uint emissiveTexcoord;
    // PBR workflow index
    uint workflow;
};
[[vk::binding(5, 1)]] ConstantBuffer<Material> mat;

struct Input
{
    // global position
    [[vk::location(0)]] float3 position : POSITION0;
    // shadow-map position
    [[vk::location(1)]] float4 shadowPosition : POSITION1;
    // texcoords
    [[vk::location(2)]] float2 texcoord[5] : TEXCOORD;
    // color
    [[vk::location(7)]] float4 color : COLOR;
    // tangent-to-world transformation matrix
    [[vk::location(8)]] float3x3 tangentToWorld : MATRIX;
};

struct Output
{
    // color
    [[vk::location(0)]] float4 color : SV_Target0;
    // global normal
    [[vk::location(1)]] float4 normal : SV_Target1;
};

static const float pi = 3.14159265358979323846;

// Calculate the tangent-to-world transformation matrix using screen-space derivatives.
float3x3 screenSpaceTangentToWorld(float3 position, float2 texcoord)
{
    const float3 ddx_position = ddx(position);
    const float3 ddy_position = -ddy(position);
    const float2 ddx_texcoord = ddx(texcoord);
    const float2 ddy_texcoord = -ddy(texcoord);

    const float3 normal = normalize(cross(ddx_position, ddy_position));
    float3 tangent =
        (ddy_texcoord.y * ddx_position - ddx_texcoord.y * ddy_position) /
        (ddy_texcoord.y * ddx_texcoord.x - ddx_texcoord.y * ddy_texcoord.x);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    const float3 bitangent = cross(normal, tangent);

    return transpose(float3x3(tangent, bitangent, normal));
}

// Evaluate the Trowbridge-Reitz/GGX microfacet distribution.
float D_TrowbridgeReitzGGX(float alphaSquared, float NdotH)
{
    const float f = (NdotH * alphaSquared - NdotH) * NdotH + 1.0;

    return alphaSquared / (pi * f * f);
}

// Evaluate Schlick's approximation of the Fresnel reflectance function.
template<typename T>
T F_Schlick(T F0, float F90, float cosTheta)
{
    return F0 + (F90 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Evaluate the height-correlated Smith visibility function.
float V_SmithCorrelated(float alphaSquared, float NdotV, float NdotL)
{
    const float LambdaV = NdotL * sqrt((-NdotV * alphaSquared + NdotV) * NdotV + alphaSquared);
    const float LambdaL = NdotV * sqrt((-NdotL * alphaSquared + NdotL) * NdotL + alphaSquared);

    return 0.5 / (LambdaV + LambdaL);
}

// Calculate the diffuse reflectance using the Disney model with energy renormalization.
float diffuse_DisneyRenormalized(float roughness, float NdotV, float NdotL, float LdotH)
{
    const float energyBias = lerp(0, 0.5, roughness);
    const float energyFactor = lerp(1.0, 1.0 / 1.51, roughness);
    const float FD90 = energyBias + 2.0 * LdotH * LdotH * roughness;
    const float lightScatter = F_Schlick(1.0, FD90, NdotL);
    const float viewScatter = F_Schlick(1.0, FD90, NdotV);
    
    return lightScatter * viewScatter * energyFactor / pi;
}

// Calculate the shadow occlusion via Percentage Closer Filtering (PCF) with Poisson disk sampling.
float shadow(float4 shadowPosition)
{
    static const float spread_shadowRes = 4.0 / 8192.0;
    static const float2 poissonDisk[8] = {
        spread_shadowRes * float2(-0.32, 0.13),
        spread_shadowRes * float2(0.83, 0.15),
        spread_shadowRes * float2(-0.27, 0.88),
        spread_shadowRes * float2(-0.21, -0.78),
        spread_shadowRes * float2(0.49, -0.61),
        spread_shadowRes * float2(-0.98, 0.17),
        spread_shadowRes * float2(0.39, 0.66),
        spread_shadowRes * float2(-0.84, -0.51)
    };

    const float2 texcoord = shadowPosition.xy / shadowPosition.w;
    const float depth = shadowPosition.z / shadowPosition.w;

    return 0.2 + 0.1 * (
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[0], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[1], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[2], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[3], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[4], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[5], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[6], depth) +
        shadowTexture.SampleCmp(shadowSampler, texcoord + poissonDisk[7], depth)
    );
}

Output main(Input input)
{
    Output output;

    const float2 normalTexcoord = input.texcoord[mat.normalTexcoord];

    const float3 color = colorTexture.Sample(colorSampler, input.texcoord[mat.colorTexcoord]).rgb * mat.colorFactor.rgb * input.color.rgb;
    const float4 pbr = pbrTexture.Sample(pbrSampler, input.texcoord[mat.pbrTexcoord]) * mat.pbrFactor;
    const float3 normal = (normalTexture.Sample(normalSampler, normalTexcoord).rgb * 2.0 - 1.0) * float3(mat.normalScale, mat.normalScale, 1.0);
    const float occlusion = 1.0 + mat.occlusionStrength * (occlusionTexture.Sample(occlusionSampler, input.texcoord[mat.occlusionTexcoord]).r - 1.0);
    const float3 emissive = emissiveTexture.Sample(emissiveSampler, input.texcoord[mat.emissiveTexcoord]).rgb * mat.emissiveFactor.rgb;

    float3 diffuseColor;
    float3 F0;
    float roughness;
    if (mat.workflow == 1)
    {
        // Specular (pbr.rgb) | Glossiness (pbr.a)
        diffuseColor = color * (1.0 - max(max(pbr.r, pbr.g), pbr.b));
        F0 = pbr.rgb;
        roughness = 1.0 - pbr.a;
    }
    else
    {
        // Metallic (pbr.b) | Roughness (pbr.g)
        diffuseColor = lerp(color, 0.0, pbr.b);
        F0 = lerp(0.04, color, pbr.b);
        roughness = pbr.g;
    }
    const float F90 = saturate(50.0 * dot(F0, 0.33));
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;

    const float3x3 tangentToWorld = (mesh.embedding > 0) ?
                                    screenSpaceTangentToWorld(input.position, normalTexcoord) :
                                    input.tangentToWorld;
    const float3 N = normalize(mul(tangentToWorld, normal));
    const float3 V = normalize(scene.viewPosition.xyz - input.position);
    const float NdotV = abs(dot(N, V)) + 1e-5;
    const float3 light = scene.lightPosition.xyz - input.position;
    const float lightDistance = length(light);
    const float3 L = normalize(light);
    const float3 H = normalize(V + L);
    const float NdotL = saturate(dot(N, L));
    const float NdotH = saturate(dot(N, H));
    const float LdotH = saturate(dot(L, H));
    const float3 R = reflect(-V, N);

    const float specularOcclusion = saturate(pow(NdotV + occlusion, exp2(-16.0 * alpha - 1.0)) - 1.0 + occlusion);
    const float2 DFG = brdfTexture.Sample(brdfSampler, float2(NdotV, roughness)).rg;
    static const float maxRadianceLOD = 7.0;
    static const float indirectIlluminance = 100.0;
    const float shadowOcclusion = shadow(input.shadowPosition);
    static const float bloomEC = 4.0;

    // Calculate the indirect reflectance from the star map.
    const float3 indirectDiffuse = occlusion * diffuseColor * irradianceTexture.Sample(irradianceSampler, R).rgb;
    const float3 indirectSpecular = specularOcclusion * (F0 * DFG.r + F90 * DFG.g) * radianceTexture.SampleLevel(radianceSampler, R, roughness * maxRadianceLOD).rgb;

    // Calculate the direct reflectance from the starlight.
    const float3 directDiffuse = diffuseColor * diffuse_DisneyRenormalized(roughness, NdotV, NdotL, LdotH);
    const float3 directSpecular = F_Schlick(F0, F90, LdotH) * D_TrowbridgeReitzGGX(alphaSquared, NdotH) * V_SmithCorrelated(alphaSquared, NdotV, NdotL);
    
    // Compose the luminance from the indirect, direct, and emissive parts.
    float3 luminance = (indirectDiffuse + indirectSpecular) * indirectIlluminance;
    luminance += shadowOcclusion * (directDiffuse + directSpecular) * NdotL * scene.lightIntensity.rgb / (lightDistance * lightDistance);
    luminance += emissive * pow(2.0, scene.EV100 + bloomEC - 3.0);

    output.color.rgb = luminance * scene.exposure;
    output.color.a = 1.0;

    output.normal.xyz = N;

    return output;
}