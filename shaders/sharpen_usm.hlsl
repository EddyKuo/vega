// ============================================================================
// Vega — sharpen_usm.hlsl
// Unsharp Mask (USM) sharpening, single-pass compute shader.
//
// Algorithm:
//   1. Compute luma of center pixel.
//   2. Box-blur luma over a (2*radius+1)^2 neighborhood (Load-based, no
//      sampler bias).
//   3. detail = luma_center - luma_blurred  (high-frequency residual)
//   4. Optionally scale detail by a flat-region mask:
//        mask = 1 - saturate(gradient_magnitude * masking_sensitivity)
//      This suppresses amplification of noise in already-noisy edges.
//   5. Apply: pixel.rgb += amount * detail * mask
//
// The detail signal is additive in linear-light space, which is correct for
// perceptual sharpening before the gamma output pass.
// ============================================================================

#include "common.hlsli"

// Must match GPUPipeline::SharpenCB layout exactly (slot b0)
cbuffer SharpenParams : register(b0)
{
    float cb_amount;    // 0-3: sharpening gain
    int   cb_radius;    // 1-3: blur kernel half-width
    float cb_masking;   // 0-1: flat-region protection (0 = none, 1 = max)
    float _pad0;
};

// Dimensions from slot b2 (shared across all shaders)
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
// Safe clamped texel fetch
// ---------------------------------------------------------------------------

float4 LoadClamped(int2 coord)
{
    int2 c = clamp(coord, int2(0, 0), int2((int)cb_src_width - 1, (int)cb_src_height - 1));
    return Input.Load(int3(c, 0));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_dst_width || dtid.y >= cb_dst_height)
        return;

    int2 center = int2(dtid.xy);

    float4 centerPixel = LoadClamped(center);
    float  lumCenter   = Luminance(centerPixel.rgb);

    int r = clamp(cb_radius, 1, 3);

    // ------------------------------------------------------------------
    // Box-blur luma over (2*r+1)^2 neighborhood
    // ------------------------------------------------------------------
    float lumaSum = 0.0f;
    int   tapCount = 0;

    for (int jy = -r; jy <= r; ++jy)
    {
        for (int jx = -r; jx <= r; ++jx)
        {
            float4 s = LoadClamped(center + int2(jx, jy));
            lumaSum += Luminance(s.rgb);
            ++tapCount;
        }
    }

    float lumaBlurred = lumaSum / (float)tapCount;
    float detail      = lumCenter - lumaBlurred;

    // ------------------------------------------------------------------
    // Optional masking: compute gradient magnitude from axis-aligned
    // neighbors and suppress sharpening in high-frequency areas (haloes).
    //
    // mask = 1 in flat regions (sharpen freely)
    // mask -> 0 as gradient magnitude increases (suppress haloes)
    // ------------------------------------------------------------------
    float mask = 1.0f;
    if (cb_masking > 0.01f)
    {
        float lumL = Luminance(LoadClamped(center + int2(-1,  0)).rgb);
        float lumR = Luminance(LoadClamped(center + int2( 1,  0)).rgb);
        float lumU = Luminance(LoadClamped(center + int2( 0, -1)).rgb);
        float lumD = Luminance(LoadClamped(center + int2( 0,  1)).rgb);

        float gx  = lumR - lumL;
        float gy  = lumD - lumU;
        float grad = sqrt(gx * gx + gy * gy);

        // Sensitivity: at cb_masking=1, gradient of 0.05 (about 12/255)
        // fully suppresses sharpening.  Scales proportionally with masking.
        float sensitivity = cb_masking * 20.0f;
        mask = saturate(1.0f - grad * sensitivity);
    }

    // ------------------------------------------------------------------
    // Apply sharpening
    // ------------------------------------------------------------------
    float3 outRGB = centerPixel.rgb + cb_amount * detail * mask;

    Output[dtid.xy] = float4(outRGB, centerPixel.a);
}
