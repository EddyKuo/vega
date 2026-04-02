// ============================================================================
// Vega — white_balance_exposure.hlsl
// Combined White Balance + Exposure + Contrast + Highlights/Shadows
// Matches CPU: WhiteBalanceNode + ExposureNode
// ============================================================================

#include "common.hlsli"

cbuffer Params : register(b0)
{
    float cb_wb_r;          // white balance red multiplier
    float cb_wb_g;          // white balance green multiplier
    float cb_wb_b;          // white balance blue multiplier
    float cb_exposure_ev;   // exposure in EV stops

    float cb_contrast;      // contrast [-100, 100]
    float cb_highlights;    // highlights [-100, 100]
    float cb_shadows;       // shadows [-100, 100]
    uint  cb_width;

    uint  cb_height;
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D<float4>   Input  : register(t0);
RWTexture2D<float4> Output : register(u0);

// ---------------------------------------------------------------------------
// Contrast S-curve (matches ExposureNode::contrastCurve)
// ---------------------------------------------------------------------------
float ContrastCurve(float x, float strength)
{
    if (abs(strength) < 0.01f)
        return x;

    float s = strength / 100.0f;
    float centered = x - 0.5f;
    float t = saturate(x);

    if (s > 0.0f)
    {
        // Sigmoidal contrast increase
        float k = 1.0f + s * 4.0f;
        float sig  = 1.0f / (1.0f + exp(-k * (t - 0.5f) * 6.0f));
        float sig0 = 1.0f / (1.0f + exp(-k * (-0.5f) * 6.0f));
        float sig1 = 1.0f / (1.0f + exp(-k * (0.5f) * 6.0f));
        return (sig - sig0) / (sig1 - sig0);
    }
    else
    {
        // Decrease contrast: lerp toward midpoint
        float factor = 1.0f + s; // s is negative, so factor in [0, 1]
        return 0.5f + centered * factor;
    }
}

// ---------------------------------------------------------------------------
// Highlights recovery curve (matches ExposureNode::highlightsCurve)
// ---------------------------------------------------------------------------
float HighlightsCurve(float lum, float amount)
{
    if (abs(amount) < 0.01f)
        return 1.0f;

    float mask = saturate((lum - 0.25f) / 0.75f);
    mask = mask * mask;

    float shift = -(amount / 100.0f) * mask * 0.5f;
    return 1.0f + shift;
}

// ---------------------------------------------------------------------------
// Shadows lift curve (matches ExposureNode::shadowsCurve)
// ---------------------------------------------------------------------------
float ShadowsCurve(float lum, float amount)
{
    if (abs(amount) < 0.01f)
        return 1.0f;

    float mask = saturate(1.0f - lum / 0.5f);
    mask = mask * mask;

    float shift = (amount / 100.0f) * mask * 0.5f;
    return 1.0f + shift;
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_width || dtid.y >= cb_height)
        return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));

    // --- White Balance ---
    pixel.r *= cb_wb_r;
    pixel.g *= cb_wb_g;
    pixel.b *= cb_wb_b;

    // --- Exposure ---
    float exp_mul = exp2(cb_exposure_ev);
    pixel.rgb *= exp_mul;

    // --- Highlights & Shadows ---
    bool hasHighlights = abs(cb_highlights) > 0.01f;
    bool hasShadows    = abs(cb_shadows) > 0.01f;

    if (hasHighlights || hasShadows)
    {
        float lum = saturate(Luminance(pixel.rgb));
        float mul = 1.0f;

        if (hasHighlights)
            mul *= HighlightsCurve(lum, cb_highlights);
        if (hasShadows)
            mul *= ShadowsCurve(lum, cb_shadows);

        pixel.rgb *= mul;
    }

    // --- Contrast ---
    if (abs(cb_contrast) > 0.01f)
    {
        pixel.r = ContrastCurve(saturate(pixel.r), cb_contrast);
        pixel.g = ContrastCurve(saturate(pixel.g), cb_contrast);
        pixel.b = ContrastCurve(saturate(pixel.b), cb_contrast);
    }

    Output[dtid.xy] = pixel;
}
