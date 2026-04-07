// ============================================================================
// Vega — hsl_adjust.hlsl
// HSL per-channel adjustment + global vibrance + saturation.
// Matches CPU: HSLNode with 8-channel cosine blend.
// ============================================================================

#include "common.hlsli"

// Must match GPUPipeline::HSLCB at slot b1
cbuffer Params : register(b1)
{
    float4 cb_hsl_hue_0123;     // R, O, Y, G
    float4 cb_hsl_hue_4567;     // Aqua, B, Purple, Magenta

    float4 cb_hsl_sat_0123;
    float4 cb_hsl_sat_4567;

    float4 cb_hsl_lum_0123;
    float4 cb_hsl_lum_4567;

    float cb_vibrance;
    float cb_saturation;
    float _pad1;
    float _pad2;
};

// Dimensions from slot b2 (shared)
cbuffer Dimensions : register(b2)
{
    uint cb_src_width;
    uint cb_src_height;
    uint cb_dst_width;
    uint cb_dst_height;
};

Texture2D<float4>   Input  : register(t0);
RWTexture2D<float4> Output : register(u0);

// ---------------------------------------------------------------------------
// 8-channel cosine-blend weights (matches HSLNode::computeChannelWeights)
// ---------------------------------------------------------------------------

// Channel center hues in degrees
static const float kCenters[8] = {
    0.0f,    // Red
    30.0f,   // Orange
    60.0f,   // Yellow
    120.0f,  // Green
    180.0f,  // Aqua
    240.0f,  // Blue
    270.0f,  // Purple
    300.0f   // Magenta
};

// Half-widths: angular distance at which blending reaches zero
static const float kHalfWidths[8] = {
    30.0f,  // Red
    30.0f,  // Orange
    30.0f,  // Yellow
    60.0f,  // Green
    60.0f,  // Aqua
    30.0f,  // Blue
    30.0f,  // Purple
    30.0f   // Magenta
};

// Helper to index into packed float4 pairs
float GetHue(int i) { return (i < 4) ? cb_hsl_hue_0123[i] : cb_hsl_hue_4567[i - 4]; }
float GetSat(int i) { return (i < 4) ? cb_hsl_sat_0123[i] : cb_hsl_sat_4567[i - 4]; }
float GetLum(int i) { return (i < 4) ? cb_hsl_lum_0123[i] : cb_hsl_lum_4567[i - 4]; }

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_dst_width || dtid.y >= cb_dst_height)
        return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));

    float r = saturate(pixel.r);
    float g = saturate(pixel.g);
    float b = saturate(pixel.b);

    // Convert to HSL
    float3 hsl = RGBToHSL(float3(r, g, b));
    float h = hsl.x;
    float s = hsl.y;
    float l = hsl.z;

    // --- Per-channel HSL adjustments with cosine blend ---
    {
        float totalWeight = 0.0f;
        float weights[8];

        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            float diff = h - kCenters[i];
            // Wrap to [-180, 180]
            if (diff > 180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;
            diff = abs(diff);

            float hw = kHalfWidths[i];
            if (diff >= hw)
            {
                weights[i] = 0.0f;
            }
            else
            {
                weights[i] = 0.5f * (1.0f + cos(PI * diff / hw));
            }
            totalWeight += weights[i];
        }

        // Normalize
        if (totalWeight > 1e-6f)
        {
            float inv = 1.0f / totalWeight;
            [unroll]
            for (int i = 0; i < 8; ++i)
                weights[i] *= inv;
        }

        // Accumulate weighted shifts
        float hueShift = 0.0f;
        float satShift = 0.0f;
        float lumShift = 0.0f;

        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            if (weights[i] > 1e-6f)
            {
                hueShift += weights[i] * GetHue(i);
                satShift += weights[i] * GetSat(i);
                lumShift += weights[i] * GetLum(i);
            }
        }

        // Apply hue shift
        h += hueShift;
        if (h < 0.0f) h += 360.0f;
        if (h >= 360.0f) h -= 360.0f;

        // Apply saturation shift: slider [-100,100] maps to multiplier
        float satMul = 1.0f + satShift / 100.0f;
        s = saturate(s * satMul);

        // Apply luminance shift: slider [-100,100]
        l = saturate(l + lumShift / 200.0f);
    }

    // --- Global vibrance ---
    if (abs(cb_vibrance) > 0.01f)
    {
        float vibranceScale = cb_vibrance / 100.0f;
        float protection = 1.0f - s;
        protection = protection * protection;
        float vibMul = 1.0f + vibranceScale * protection;
        s = saturate(s * vibMul);
    }

    // --- Global saturation ---
    if (abs(cb_saturation) > 0.01f)
    {
        float satMul = 1.0f + cb_saturation / 100.0f;
        s = saturate(s * satMul);
    }

    // Convert back to RGB
    float3 rgb = HSLToRGB(float3(h, s, l));

    Output[dtid.xy] = float4(saturate(rgb), pixel.a);
}
