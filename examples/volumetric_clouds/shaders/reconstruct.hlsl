Texture2D<float4> TraceColor : register(t0);
Texture2D<float> TraceDist : register(t1);
Texture2D<float4> HistoryColor : register(t2);
Texture2D<float> SceneDist : register(t3);

float4 ReconstructPS(float4 position : SV_Position) : SV_Target
{
    int2 hp = int2(position.xy);
    bool sky = SceneDist.Load(int3(hp, 0)) < 0;
    bool checker = DepthInfo.w > 0;
    int2 offset = int2(Temporal.zw);
    int2 tp = checker ? hp >> 1 : hp;
    bool fresh = !checker || all((hp & 1) == offset);
    int2 limit = int2(TraceSize.xy) - 1;

    float4 lo = 1e30;
    float4 hi = -1e30;
    float4 nearest = float4(0, 0, 0, 1);
    float nearestDistance = DepthInfo.z;
    float best = 1e30;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            int2 q = clamp(tp + int2(x, y), 0, limit);
            float4 c = TraceColor.Load(int3(q, 0));
            if (IsSky(c.a) != sky)
                continue;
            float4 v = float4(c.rgb, DecodeT(c.a));
            lo = min(lo, v);
            hi = max(hi, v);
            float2 center = checker ? float2(q * 2 + offset) : float2(q);
            float d2 = dot(center - hp, center - hp);
            if (d2 < best)
            {
                best = d2;
                nearest = v;
                nearestDistance = TraceDist.Load(int3(q, 0));
            }
        }
    }
    if (best >= 1e30)
    {
        int2 q = clamp(tp, 0, limit);
        float4 c = TraceColor.Load(int3(q, 0));
        nearest = float4(c.rgb, DecodeT(c.a));
        nearestDistance = TraceDist.Load(int3(q, 0));
        lo = nearest;
        hi = nearest;
    }

    float4 result = nearest;
    if (Temporal.x > 0)
    {
        float3 dir = ViewRay((hp + 0.5) * HistorySize.zw);
        float4 clip = mul(PrevViewProj, float4(dir * nearestDistance + CameraDelta.xyz, 1));
        if (clip.w > 0)
        {
            float2 uv = float2(clip.x, -clip.y) / clip.w * 0.5 + 0.5;
            float4 history;
            if (all(uv >= 0) && all(uv <= 1) && ClassBilinear(HistoryColor, uv, HistorySize.xy, sky, history))
            {
                float4 clamped = clamp(history, lo, hi);
                result = fresh ? lerp(clamped, nearest, Temporal.y) : clamped;
            }
        }
    }
    return float4(result.rgb, EncodeT(saturate(result.a), sky));
}
