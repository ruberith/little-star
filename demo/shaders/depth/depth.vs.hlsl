struct Mesh
{
    // model matrix
    float4x4 model;
    // embedding index
    uint embedding;
};
[[vk::push_constant]] Mesh mesh;

struct Transform
{
    // view-projection matrix
    float4x4 viewProjection;
};
[[vk::binding(0, 0)]] ConstantBuffer<Transform> transform;

struct Skin
{
    // joint matrices
    float4x4 joint[128];
    // joint count
    uint jointCount;
};
[[vk::binding(0, 1)]] ConstantBuffer<Skin> skin;

// particle positions
[[vk::binding(1, 0)]] StructuredBuffer<float4> positions;

struct Input
{
    // local position
    [[vk::location(0)]] float3 position : POSITION;
    // joints
    [[vk::location(1)]] uint4 joints : BLENDINDICES;
    // weights
    [[vk::location(2)]] float4 weights : BLENDWEIGHT;
};

struct Output
{
    // clip-space position
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;

    if (mesh.embedding == 1) // triangular barycentric skinning
    {
        output.position = float4(
            input.weights.x * positions[input.joints.x].xyz +
            input.weights.y * positions[input.joints.y].xyz +
            input.weights.z * positions[input.joints.z].xyz, 1.0);
    }
    else if (mesh.embedding == 2) // tetrahedral barycentric skinning
    {
        output.position = float4(
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
        output.position = mul(model, float4(input.position, 1.0));
    }
    output.position = mul(transform.viewProjection, output.position);
    
    return output;
}