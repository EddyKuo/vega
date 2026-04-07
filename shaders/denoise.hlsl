// ============================================================================
// Vega — denoise.hlsl
// Single-pass NxN neighborhood denoise in YCbCr space.
//
// Luma channel: bilateral-style weighted average.
//   weight = edge_threshold_blend(range_diff, edge_mask)
//   Falls back to pure box average when strength is high and no edges present.
//
// Chroma channels (Cb, Cr): unconditional box average.
//   Human vision is far less sensitive to chroma noise, so simple averaging
//   is perceptually indistinguishable from a more expensive filter.
//
// Output Y is blended with original Y based on cb_luma_strength and an edge
// mask, so fine structure is preserved at detail_keep > 0.
//
// BT.601 YCbCr coefficients are used (standard for digital imagery).
// ============================================================================

#include "common.hlsli"

// Must match GPUPipeline::DenoiseCB layout exactly (slot b0)
cbuffer DenoiseParams : register(b0)
{
    float cb_luma_strength;    // 0-1: how much luma smoothing to apply
    float cb_chroma_strength;  // 0-1: how much chroma smoothing to apply
    float cb_detail_keep;      // 0-1: protect edges from luma smoothing
    int   cb_luma_radius;      // 1-3: luma kernel half-width

    int   cb_chroma_radius;    // 1-4: chroma kernel half-width
    float _pad0;
    float _pad1;
    float _pad2;
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
// BT.601 RGB <-> YCbCr (offset form, Y in [0,1], Cb/Cr in [-0.5, 0.5])
// ---------------------------------------------------------------------------

float3 RGBToYCbCr(float3 rgb)
{
    float y  =  0.299f    * rgb.r + 0.587f    * rgb.g + 0.114f    * rgb.b;
    float cb = -0.168736f * rgb.r - 0.331264f * rgb.g + 0.5f      * rgb.b;
    float cr =  0.5f      * rgb.r - 0.418688f * rgb.g - 0.081312f * rgb.b;
    return float3(y, cb, cr);
}

float3 YCbCrToRGB(float3 ycbcr)
{
    float y  = ycbcr.x;
    float cb = ycbcr.y;
    float cr = ycbcr.z;
    float r = y                  + 1.402f    * cr;
    float g = y - 0.344136f * cb - 0.714136f * cr;
    float b = y + 1.772f    * cb;
    return float3(r, g, b);
}

// ---------------------------------------------------------------------------
// Safe clamped integer texel fetch (returns float3 RGB)
// ---------------------------------------------------------------------------

float3 LoadRGB(int2 coord)
{
    int2 c = clamp(coord, int2(0, 0), int2((int)cb_src_width - 1, (int)cb_src_height - 1));
    return Input.Load(int3(c, 0)).rgb;
}

// ---------------------------------------------------------------------------
// Simple gradient-based edge magnitude at center pixel (Sobel approximation)
// Returns value in [0, inf); typical range 0-1 for natural images.
// ---------------------------------------------------------------------------

float EdgeMagnitude(int2 coord)
{
    float3 l  = LoadRGB(coord + int2(-1,  0));
    float3 r  = LoadRGB(coord + int2( 1,  0));
    float3 u  = LoadRGB(coord + int2( 0, -1));
    float3 d  = LoadRGB(coord + int2( 0,  1));

    float3 dx = r - l;
    float3 dy = d - u;

    // Use luma component of gradient for a single scalar edge value
    float edgeX = 0.299f * dx.r + 0.587f * dx.g + 0.114f * dx.b;
    float edgeY = 0.299f * dy.r + 0.587f * dy.g + 0.114f * dy.b;

    return sqrt(edgeX * edgeX + edgeY * edgeY);
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

    // Load and convert center pixel
    float3 centerRGB  = LoadRGB(center);
    float3 centerYCC  = RGBToYCbCr(centerRGB);
    float  centerY    = centerYCC.x;

    // ------------------------------------------------------------------
    // Edge mask: high edge magnitude -> suppress luma smoothing.
    // edge_mask in [0, 1] where 0 = strong edge, 1 = flat region.
    // The threshold is tuned so that 8-bit step edges (~0.04 luma diff
    // per pixel) fully engage the protection at cb_detail_keep = 1.
    // ------------------------------------------------------------------
    float edgeMag  = EdgeMagnitude(center);
    float edgeMask = saturate(1.0f - edgeMag * 8.0f * cb_detail_keep);

    // ------------------------------------------------------------------
    // Luma bilateral-style accumulation
    // weight = 1 if |range_diff| < threshold, fades off beyond it.
    // Range threshold is driven by luma_strength so at higher strength
    // we accept larger range differences (more aggressive smoothing).
    // ------------------------------------------------------------------
    float lumaAcc    = 0.0f;
    float lumaWeight = 0.0f;

    // Range sigma: wider at higher strength (0.02 to 0.20 in Y space)
    float sigmaRange = 0.02f + cb_luma_strength * 0.18f;
    float invSigma2  = 1.0f / (2.0f * sigmaRange * sigmaRange);

    int lr = clamp(cb_luma_radius, 1, 3);

    for (int jy = -lr; jy <= lr; ++jy)
    {
        for (int jx = -lr; jx <= lr; ++jx)
        {
            float3 sampleRGB = LoadRGB(center + int2(jx, jy));
            float  sampleY   = 0.299f * sampleRGB.r + 0.587f * sampleRGB.g + 0.114f * sampleRGB.b;

            float rangeDiff = sampleY - centerY;
            float w = exp(-rangeDiff * rangeDiff * invSigma2);

            lumaAcc    += w * sampleY;
            lumaWeight += w;
        }
    }

    float filteredY = (lumaWeight > 1e-6f) ? (lumaAcc / lumaWeight) : centerY;

    // Blend filtered Y with original Y:
    //   edgeMask=0 (edge present) -> lean toward original
    //   edgeMask=1 (flat region)  -> full filtering
    float blendFactor = cb_luma_strength * edgeMask;
    float finalY      = lerp(centerY, filteredY, blendFactor);

    // ------------------------------------------------------------------
    // Chroma box average (unconditional)
    // ------------------------------------------------------------------
    float cbAcc = 0.0f;
    float crAcc = 0.0f;
    int   chromaCount = 0;

    int cr2 = clamp(cb_chroma_radius, 1, 4);

    for (int jy = -cr2; jy <= cr2; ++jy)
    {
        for (int jx = -cr2; jx <= cr2; ++jx)
        {
            float3 sampleYCC = RGBToYCbCr(LoadRGB(center + int2(jx, jy)));
            cbAcc += sampleYCC.y;
            crAcc += sampleYCC.z;
            ++chromaCount;
        }
    }

    float invCount = 1.0f / (float)chromaCount;
    float filteredCb = cbAcc * invCount;
    float filteredCr = crAcc * invCount;

    // Blend chroma with original based on chroma_strength
    float finalCb = lerp(centerYCC.y, filteredCb, cb_chroma_strength);
    float finalCr = lerp(centerYCC.z, filteredCr, cb_chroma_strength);

    // ------------------------------------------------------------------
    // Reconstruct and write
    // ------------------------------------------------------------------
    float3 outRGB = YCbCrToRGB(float3(finalY, finalCb, finalCr));

    Output[dtid.xy] = float4(outRGB, Input.Load(int3(center, 0)).a);
}
