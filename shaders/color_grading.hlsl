// ============================================================================
// Vega — color_grading.hlsl
// Three-way color grading: Shadows / Midtones / Highlights.
// Each range has an independent hue+saturation color wheel applied via
// a luminance-based tonal mask.  Blending and Balance control the mask
// overlap and the midpoint between shadows and highlights.
// ============================================================================

#include "common.hlsli"

// Must match GPUPipeline::ColorGradingCB at slot b0
cbuffer ColorGradingParams : register(b0)
{
    float cb_shadow_hue;   // degrees [0, 360)
    float cb_shadow_sat;   // [0, 100]
    float cb_mid_hue;
    float cb_mid_sat;
    float cb_high_hue;
    float cb_high_sat;
    float cb_blending;     // [0, 100] — controls mask overlap width
    float cb_balance;      // [-100, +100] — shifts the shadows/highlights pivot
    uint  cb_width;
    uint  cb_height;
    float cb_pad0;
    float cb_pad1;
};

Texture2D<float4>   Input  : register(t0);
RWTexture2D<float4> Output : register(u0);

// ---------------------------------------------------------------------------
// applyColorShift
// Blend a hue+saturation tint into an existing RGB value.
// The tint is expressed as a cosine-based RGB direction in [0,1] space,
// lerped against the original colour proportionally to the saturation
// parameter.  A small gain factor (0.3) keeps the effect subtle and
// avoids washing out the original image detail.
// ---------------------------------------------------------------------------
float3 applyColorShift(float3 rgb, float hue, float sat)
{
    if (sat < 0.1f)
        return rgb;

    float s      = sat / 100.0f;
    float h_rad  = hue * PI / 180.0f;

    // Project hue onto R/G/B axes (120 degree offsets)
    float3 shift = float3(
        cos(h_rad),
        cos(h_rad - 2.09439510239f),   // 2*PI/3
        cos(h_rad + 2.09439510239f)
    );
    // Remap [-1, 1] -> [0, 1]
    shift = shift * 0.5f + 0.5f;

    return lerp(rgb, rgb * shift, s * 0.3f);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_width || dtid.y >= cb_height)
        return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));
    float3 color = pixel.rgb;

    // BT.709 perceptual luminance drives the tonal masks
    float lum = Luminance(color);

    // balance_pt moves the shadow/highlight division point:
    //   balance = 0   -> pivot at 0.5
    //   balance = +100 -> pivot shifted toward highlights (more shadow range)
    //   balance = -100 -> pivot shifted toward shadows (more highlight range)
    float balance_pt = 0.5f + cb_balance / 200.0f;

    // blend controls how wide the crossfade region is.
    // A value of 0 gives hard boundaries; 100 gives maximum overlap.
    float blend = max(cb_blending / 100.0f, 0.01f);

    // Half-width of the transition zone clamped so it can't exceed balance_pt
    float half_w = blend * 0.5f;

    float shadow_mask    = 1.0f - smoothstep(balance_pt - half_w, balance_pt + half_w, lum);
    float highlight_mask = smoothstep(balance_pt - half_w, balance_pt + half_w, lum);
    float midtone_mask   = max(1.0f - shadow_mask - highlight_mask, 0.0f);

    // Apply per-range colour shifts, blending by the corresponding mask weight
    float3 result = color;
    result = lerp(result, applyColorShift(result, cb_shadow_hue, cb_shadow_sat), shadow_mask);
    result = lerp(result, applyColorShift(result, cb_mid_hue,    cb_mid_sat),    midtone_mask);
    result = lerp(result, applyColorShift(result, cb_high_hue,   cb_high_sat),   highlight_mask);

    Output[dtid.xy] = float4(result, pixel.a);
}
