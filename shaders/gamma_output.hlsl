// ============================================================================
// Vega — gamma_output.hlsl
// Final output pass: linear light -> sRGB gamma encode.
//
// This is intentionally a thin pass.  All tone operators and adjustments run
// in linear light upstream.  Separating gamma from the HSL shader lets the
// pipeline insert denoise/sharpen between HSL and gamma without re-encoding
// and re-linearising mid-pipeline, which would introduce rounding error.
//
// LinearToSRGB from common.hlsli implements the full IEC 61966-2-1 piece-wise
// function (not the 2.2 power approximation).
// ============================================================================

#include "common.hlsli"

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

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_dst_width || dtid.y >= cb_dst_height)
        return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));

    pixel.rgb = LinearToSRGB(pixel.rgb);

    Output[dtid.xy] = pixel;
}
