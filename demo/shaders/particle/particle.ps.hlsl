struct Camera
{
    // exposure value at ISO 100
    [[vk::offset(68)]] float EV100;
    // exposure [lx * s]
    float exposure;
};
[[vk::push_constant]] Camera cam;

struct Output
{
    // color
    [[vk::location(0)]] float4 color : SV_Target0;
    // global normal
    [[vk::location(1)]] float4 normal : SV_Target1;
};

Output main()
{
    Output output;

    static const float3 emissive = {1.0, 0.9, 0.0};
    static const float bloomEC = 4.0;

    output.color.rgb = emissive * pow(2.0, cam.EV100 + bloomEC - 3.0) * cam.exposure;
    output.color.a = 1.0;
    return output;
}