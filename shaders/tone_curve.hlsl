// ============================================================================
// Vega — tone_curve.hlsl
// Tone curve adjustment via 1D LUT sampling.
// Matches CPU: ToneCurveNode (monotone cubic Hermite LUTs baked on CPU side)
//
// LUTs are 4096-entry Texture1D resources built on the CPU using the same
// Fritsch-Carlson monotone cubic Hermite spline as ToneCurveNode::buildLUT,
// then uploaded as SRVs.
// ============================================================================

#include "common.hlsli"

cbuffer Params : register(b0)
{
    uint  cb_width;
    uint  cb_height;
    uint  cb_has_rgb;   // 1 if master RGB curve is active
    uint  cb_has_r;     // 1 if per-channel R curve is active

    uint  cb_has_g;     // 1 if per-channel G curve is active
    uint  cb_has_b;     // 1 if per-channel B curve is active
    float _pad0;
    float _pad1;
};

Texture2D<float4>   Input   : register(t0);
Texture1D<float>    LUT_RGB : register(t1);  // 4096 entries, master curve
Texture1D<float>    LUT_R   : register(t2);  // 4096 entries, red channel
Texture1D<float>    LUT_G   : register(t3);  // 4096 entries, green channel
Texture1D<float>    LUT_B   : register(t4);  // 4096 entries, blue channel

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> Output : register(u0);

// Sample a 1D LUT with linear interpolation.
// Input x is in [0, 1], LUT is a 1D texture with 4096 texels.
float SampleLUT(Texture1D<float> lut, float x)
{
    // SampleLevel with a linear sampler gives hardware-interpolated lookup
    return lut.SampleLevel(LinearSampler, saturate(x), 0.0f);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_width || dtid.y >= cb_height)
        return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));

    // Apply master RGB curve first (same curve to all three channels)
    if (cb_has_rgb)
    {
        pixel.r = SampleLUT(LUT_RGB, pixel.r);
        pixel.g = SampleLUT(LUT_RGB, pixel.g);
        pixel.b = SampleLUT(LUT_RGB, pixel.b);
    }

    // Then per-channel curves
    if (cb_has_r) pixel.r = SampleLUT(LUT_R, pixel.r);
    if (cb_has_g) pixel.g = SampleLUT(LUT_G, pixel.g);
    if (cb_has_b) pixel.b = SampleLUT(LUT_B, pixel.b);

    Output[dtid.xy] = pixel;
}
