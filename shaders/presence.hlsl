// ============================================================================
// Vega — presence.hlsl
// Clarity (mid-frequency local contrast), Texture (high-frequency detail),
// and Dehaze (atmospheric haze removal) applied in a single pass.
// Operates in linear light space (after WB+Exposure, before ToneCurve).
// ============================================================================

#include "common.hlsli"

cbuffer PresenceParams : register(b0)
{
    float    cb_clarity;
    float    cb_texture;
    float    cb_dehaze;
    float    cb_pad0;
    uint     cb_width;
    uint     cb_height;
    uint     cb_pad1;
    uint     cb_pad2;
};

Texture2D<float4>   Input  : register(t0);
RWTexture2D<float4> Output : register(u0);

// Simple box blur for local mean estimation.
// Clamps sample coordinates to image bounds.
float3 boxBlur(int2 center, int radius)
{
    float3 sum   = float3(0, 0, 0);
    int    count = 0;
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int2 p = center + int2(dx, dy);
            p = clamp(p, int2(0, 0), int2((int)cb_width - 1, (int)cb_height - 1));
            sum += Input.Load(int3(p, 0)).rgb;
            ++count;
        }
    }
    return sum / (float)max(count, 1);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_width || dtid.y >= cb_height)
        return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));
    float3 color = pixel.rgb;

    // ── Clarity: mid-frequency local contrast (large-radius unsharp mask) ──
    if (abs(cb_clarity) > 0.5f)
    {
        float3 blur_large = boxBlur(int2(dtid.xy), 8);
        float3 detail     = color - blur_large;
        color += detail * (cb_clarity / 100.0f);
    }

    // ── Texture: high-frequency detail enhancement (small-radius unsharp mask) ──
    if (abs(cb_texture) > 0.5f)
    {
        float3 blur_small = boxBlur(int2(dtid.xy), 2);
        float3 fine       = color - blur_small;
        color += fine * (cb_texture / 100.0f);
    }

    // ── Dehaze: simplified dark-channel-prior haze removal / addition ──
    if (abs(cb_dehaze) > 0.5f)
    {
        float  strength      = cb_dehaze / 100.0f;
        float  dark_channel  = min(color.r, min(color.g, color.b));
        // Transmission estimate: closer to 0 means more haze, closer to 1 means clear.
        // A positive strength removes haze; negative adds a hazy look.
        float  transmission  = 1.0f - strength * saturate(dark_channel);
        transmission = max(transmission, 0.1f);
        // Dehaze: J = (I - A*(1-t)) / t  where A = white atmospheric light
        float3 A = float3(1.0f, 1.0f, 1.0f);
        color = (color - A * (1.0f - transmission)) / transmission;
        color = max(color, float3(0, 0, 0));
    }

    Output[dtid.xy] = float4(color, pixel.a);
}
