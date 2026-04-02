// ============================================================================
// Vega — histogram_compute.hlsl
// GPU histogram computation using atomic InterlockedAdd.
// Produces 256-bin histograms for R, G, B, and Luminance channels.
// ============================================================================

#include "common.hlsli"

cbuffer Params : register(b0)
{
    uint cb_width;
    uint cb_height;
    float _pad0;
    float _pad1;
};

Texture2D<float4>  Input    : register(t0);

// Each buffer holds 256 uint entries
RWBuffer<uint>     HistR    : register(u0);
RWBuffer<uint>     HistG    : register(u1);
RWBuffer<uint>     HistB    : register(u2);
RWBuffer<uint>     HistLuma : register(u3);

// Use groupshared memory for local histogram accumulation to reduce
// global memory contention. Each group builds a local histogram, then
// flushes it to global buffers with InterlockedAdd.
groupshared uint gs_histR[256];
groupshared uint gs_histG[256];
groupshared uint gs_histB[256];
groupshared uint gs_histLuma[256];

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID,
            uint3 gtid : SV_GroupThreadID,
            uint  gi   : SV_GroupIndex)
{
    // --- Clear local histogram bins ---
    // Thread group is 16x16 = 256 threads, so each thread clears exactly one bin
    gs_histR[gi]    = 0;
    gs_histG[gi]    = 0;
    gs_histB[gi]    = 0;
    gs_histLuma[gi] = 0;

    GroupMemoryBarrierWithGroupSync();

    // --- Accumulate into groupshared histogram ---
    if (dtid.x < cb_width && dtid.y < cb_height)
    {
        float4 pixel = Input.Load(int3(dtid.xy, 0));

        // Clamp to [0, 1] and quantize to [0, 255]
        uint binR = min(uint(saturate(pixel.r) * 255.0f), 255u);
        uint binG = min(uint(saturate(pixel.g) * 255.0f), 255u);
        uint binB = min(uint(saturate(pixel.b) * 255.0f), 255u);

        float lum = Luminance(saturate(pixel.rgb));
        uint binL = min(uint(lum * 255.0f), 255u);

        InterlockedAdd(gs_histR[binR], 1u);
        InterlockedAdd(gs_histG[binG], 1u);
        InterlockedAdd(gs_histB[binB], 1u);
        InterlockedAdd(gs_histLuma[binL], 1u);
    }

    GroupMemoryBarrierWithGroupSync();

    // --- Flush local histogram to global buffers ---
    // Each of the 256 threads handles one bin
    if (gs_histR[gi] > 0)
        InterlockedAdd(HistR[gi], gs_histR[gi]);
    if (gs_histG[gi] > 0)
        InterlockedAdd(HistG[gi], gs_histG[gi]);
    if (gs_histB[gi] > 0)
        InterlockedAdd(HistB[gi], gs_histB[gi]);
    if (gs_histLuma[gi] > 0)
        InterlockedAdd(HistLuma[gi], gs_histLuma[gi]);
}
