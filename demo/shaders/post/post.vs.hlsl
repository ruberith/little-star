struct Input
{
    // vertex index
    uint vertexIndex : SV_VertexID;
};

struct Output
{
    // clip-space position
    float4 position : SV_Position;
    // texcoord
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

Output main(Input input)
{
    Output output;
    output.texcoord = float2((input.vertexIndex << 1) & 2, input.vertexIndex & 2);
    output.position = float4(2.0 * output.texcoord - 1.0, 0.0, 1.0);
    return output;
}