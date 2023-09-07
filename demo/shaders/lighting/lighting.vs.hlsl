struct Mesh
{
    // model matrix
    float4x4 model;
    // embedding index
    uint embedding;
};
[[vk::push_constant]] Mesh mesh;

struct ViewProjection
{
    // camera view-projection matrix
    float4x4 camera;
    // shadow view-projection matrix
    float4x4 shadow;
};
[[vk::binding(4, 0)]] ConstantBuffer<ViewProjection> viewProjection;

struct Skin
{
    // joint matrices
    float4x4 joint[128];
    // joint count
    uint jointCount;
};
[[vk::binding(0, 2)]] ConstantBuffer<Skin> skin;

// particle positions
[[vk::binding(7, 0)]] StructuredBuffer<float4> positions;

struct Input
{
    // local position
    [[vk::location(0)]] float3 position : POSITION;
    // local normal
    [[vk::location(1)]] float3 normal : NORMAL;
    // local tangent
    [[vk::location(2)]] float4 tangent : TANGENT;
    // texcoords
    [[vk::location(3)]] float2 texcoord[5] : TEXCOORD;
    // color
    [[vk::location(8)]] float4 color : COLOR;
    // joints
    [[vk::location(9)]] uint4 joints : BLENDINDICES;
    // weights
    [[vk::location(10)]] float4 weights : BLENDWEIGHT;
};

struct Output
{
    // clip-space position
    float4 clipPosition : SV_Position;
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

Output main(Input input)
{
    Output output;

    if (mesh.embedding == 1) // triangular barycentric skinning
    {
        output.clipPosition = float4(
            input.weights.x * positions[input.joints.x].xyz +
            input.weights.y * positions[input.joints.y].xyz +
            input.weights.z * positions[input.joints.z].xyz, 1.0);
    }
    else if (mesh.embedding == 2) // tetrahedral barycentric skinning
    {
        output.clipPosition = float4(
            input.weights.x * positions[input.joints.x].xyz +
            input.weights.y * positions[input.joints.y].xyz +
            input.weights.z * positions[input.joints.z].xyz +
            input.weights.w * positions[input.joints.w].xyz, 1.0);
    }
    else
    {
        float4x4 model;
        if (skin.jointCount > 0) // linear blend skinning
        {
            model =
                input.weights.x * skin.joint[input.joints.x] +
                input.weights.y * skin.joint[input.joints.y] +
                input.weights.z * skin.joint[input.joints.z] +
                input.weights.w * skin.joint[input.joints.w];
        }
        else // no skinning
        {
            model = mesh.model;
        }
        output.clipPosition = mul(model, float4(input.position, 1.0));
        float3 normal = normalize(mul((float3x3)model, input.normal));
        float3 tangent = normalize(mul((float3x3)model, input.tangent.xyz));
        float3 bitangent = normalize(mul((float3x3)model, cross(input.normal, input.tangent.xyz) * input.tangent.w));
        output.tangentToWorld = transpose(float3x3(tangent, bitangent, normal));
    }
    output.position = output.clipPosition.xyz / output.clipPosition.w;
    output.shadowPosition = mul(viewProjection.shadow, output.clipPosition);
    output.clipPosition = mul(viewProjection.camera, output.clipPosition);

    output.texcoord[0] = input.texcoord[0];
    output.texcoord[1] = input.texcoord[1];
    output.texcoord[2] = input.texcoord[2];
    output.texcoord[3] = input.texcoord[3];
    output.texcoord[4] = input.texcoord[4];

    output.color = input.color;

    return output;
}