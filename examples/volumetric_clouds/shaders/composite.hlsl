Texture2D<float4> CloudColor : register(t0);

float3 Tonemap(float3 x)
{
    x *= SunColor.w;
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float4 CompositePS(float4 position : SV_Position) : SV_Target
{
    float2 uv = position.xy * ScreenSize.zw;
    float4 value;
#if RML_COMPOSITE_SKY
    if (!ClassBilinear(CloudColor, uv, HistorySize.xy, true, value))
    {
        float4 c = CloudColor.Load(int3(clamp(int2(uv * HistorySize.xy), 0, int2(HistorySize.xy) - 1), 0));
        value = float4(c.rgb, DecodeT(c.a));
    }
#else
    if (!ClassBilinear(CloudColor, uv, HistorySize.xy, false, value))
        discard;
#endif
    float alpha = saturate(1 - value.a);
    if (alpha < 0.002)
        discard;
    return float4(pow(Tonemap(value.rgb / alpha), 1 / 2.2), alpha);
}
