// effects.hlsl — Post-Crop Vignetting and Film Grain
// Applied in sRGB space (after gamma conversion).

cbuffer EffectsParams : register(b0)
{
    float cb_vig_amount;      // -100 to 100 (negative = lighten edges)
    float cb_vig_midpoint;    // 0 to 100
    float cb_vig_roundness;   // -100 to 100
    float cb_vig_feather;     // 0 to 100
    float cb_grain_amount;    // 0 to 100
    float cb_grain_size;      // 1 to 100
    float cb_grain_roughness; // 0 to 100
    float cb_pad0;
    uint  cb_width;
    uint  cb_height;
    uint  cb_frame_seed;      // incremented each frame for animated grain
    uint  cb_pad1;
};

Texture2D<float4>   Input  : register(t0);
RWTexture2D<float4> Output : register(u0);

// ── Hash function for pseudo-random noise (no texture needed) ──
float hash(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_width || dtid.y >= cb_height) return;

    float4 pixel = Input.Load(int3(dtid.xy, 0));
    float3 color = pixel.rgb;

    // ── Vignette ─────────────────────────────────────────────────────────────
    if (abs(cb_vig_amount) > 0.5)
    {
        float2 uv = float2(dtid.x, dtid.y) / float2(cb_width, cb_height);
        float2 d  = uv - float2(0.5, 0.5);

        // Roundness adjusts the aspect correction: -100 = wide ellipse, +100 = tall ellipse.
        // At 0 (default) we apply a mild aspect ratio compensation so the vignette looks
        // circular regardless of image proportions.
        float aspect    = float(cb_width) / float(cb_height);
        float roundness = cb_vig_roundness / 100.0;
        d.x *= lerp(1.0, aspect, 0.5 + roundness * 0.5);

        float dist     = length(d) * 2.0;
        float midpoint = cb_vig_midpoint / 100.0;
        float feather  = max(cb_vig_feather / 100.0, 0.01);

        // Smooth ramp from fully-inside to fully-outside the vignette zone
        float vignette = smoothstep(midpoint - feather * 0.5, midpoint + feather * 0.5, dist);
        float strength = cb_vig_amount / 100.0;

        // Positive amount = darken edges; negative = lighten edges
        color *= 1.0 - vignette * strength;
    }

    // ── Film Grain ────────────────────────────────────────────────────────────
    if (cb_grain_amount > 0.5)
    {
        // Quantise pixel coords by grain size to produce grain "clumps"
        float size_scale  = max(1.0, cb_grain_size / 10.0);
        float2 grain_uv   = floor(float2(dtid.xy) / size_scale);
        grain_uv         += float(cb_frame_seed);   // animated per-frame

        float noise            = hash(grain_uv) - 0.5;  // centered in [-0.5, 0.5]
        float roughness_factor = cb_grain_roughness / 100.0;

        // Low roughness blends towards gentle noise; high roughness keeps full noise
        noise = lerp(noise * 0.5, noise, roughness_factor);

        float amount = cb_grain_amount / 100.0 * 0.15;
        color       += noise * amount;
        color        = saturate(color);
    }

    Output[dtid.xy] = float4(color, pixel.a);
}
