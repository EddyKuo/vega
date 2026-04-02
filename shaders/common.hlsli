// ============================================================================
// Vega — common.hlsli
// Shared utility functions for all compute shaders.
// ============================================================================

#ifndef VEGA_COMMON_HLSLI
#define VEGA_COMMON_HLSLI

static const float PI = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// sRGB <-> Linear conversion (IEC 61966-2-1)
// ---------------------------------------------------------------------------

float LinearToSRGB_Channel(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    if (x < 0.0031308f)
        return 12.92f * x;
    return 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
}

float SRGBToLinear_Channel(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    if (x < 0.04045f)
        return x / 12.92f;
    return pow((x + 0.055f) / 1.055f, 2.4f);
}

float3 LinearToSRGB(float3 c)
{
    return float3(
        LinearToSRGB_Channel(c.x),
        LinearToSRGB_Channel(c.y),
        LinearToSRGB_Channel(c.z));
}

float3 SRGBToLinear(float3 c)
{
    return float3(
        SRGBToLinear_Channel(c.x),
        SRGBToLinear_Channel(c.y),
        SRGBToLinear_Channel(c.z));
}

// ---------------------------------------------------------------------------
// BT.709 Luminance
// ---------------------------------------------------------------------------

float Luminance(float3 c)
{
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

// ---------------------------------------------------------------------------
// RGB <-> HSL conversion
// ---------------------------------------------------------------------------

float HueToChannel(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

// Returns float3(H, S, L) where H is in [0, 360) degrees
float3 RGBToHSL(float3 rgb)
{
    float r = rgb.x;
    float g = rgb.y;
    float b = rgb.z;

    float maxC = max(r, max(g, b));
    float minC = min(r, min(g, b));
    float delta = maxC - minC;

    float l = (maxC + minC) * 0.5f;
    float h = 0.0f;
    float s = 0.0f;

    if (delta > 1e-6f)
    {
        s = (l <= 0.5f) ? (delta / (maxC + minC))
                        : (delta / (2.0f - maxC - minC));

        if (maxC == r)
            h = 60.0f * fmod((g - b) / delta + 6.0f, 6.0f);
        else if (maxC == g)
            h = 60.0f * ((b - r) / delta + 2.0f);
        else
            h = 60.0f * ((r - g) / delta + 4.0f);

        if (h < 0.0f) h += 360.0f;
        if (h >= 360.0f) h -= 360.0f;
    }

    return float3(h, s, l);
}

// Takes float3(H, S, L) where H is in [0, 360) degrees
float3 HSLToRGB(float3 hsl)
{
    float h = hsl.x;
    float s = hsl.y;
    float l = hsl.z;

    if (s < 1e-6f)
        return float3(l, l, l);

    // Normalize hue to [0, 1]
    float hNorm = h / 360.0f;
    hNorm = hNorm - floor(hNorm);

    float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    float p = 2.0f * l - q;

    float r = HueToChannel(p, q, hNorm + 1.0f / 3.0f);
    float g = HueToChannel(p, q, hNorm);
    float b = HueToChannel(p, q, hNorm - 1.0f / 3.0f);

    return float3(r, g, b);
}

// ---------------------------------------------------------------------------
// 3x3 matrix multiply
// ---------------------------------------------------------------------------

float3 ApplyMatrix3x3(float3 v, float3x3 m)
{
    return mul(m, v);
}

#endif // VEGA_COMMON_HLSLI
