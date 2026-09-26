Texture2D<float4> CloudColor : register(t0);

float3 Tonemap(float3 x)
{
    x *= SunColor.w;
    float3 shoulder = 0.6 + 0.4 * (1 - exp(-(x - 0.6) * 2.5));
    return x < 0.6 ? x : shoulder;
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
    return float4(sqrt(saturate(Tonemap(value.rgb / alpha))), alpha);
}
