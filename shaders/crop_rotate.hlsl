// ============================================================================
// Vega — crop_rotate.hlsl
// Final geometry pass: crop + rotation in sRGB space.
//
// Runs AFTER the gamma pass. The output texture has the cropped dimensions.
// Rotation is applied around the center of the crop rectangle.
// Out-of-bounds pixels (only possible when rotation is non-zero) are filled
// with opaque black.
// ============================================================================

#include "common.hlsli"

cbuffer CropRotateParams : register(b0)
{
    float cb_crop_left;
    float cb_crop_top;
    float cb_crop_right;
    float cb_crop_bottom;
    float cb_rotation;   // degrees (stored for reference; not used in shader — use sin/cos)
    float cb_sin_r;
    float cb_cos_r;
    float cb_pad0;
    uint  cb_src_width;
    uint  cb_src_height;
    uint  cb_dst_width;
    uint  cb_dst_height;
};

Texture2D<float4>   Input  : register(t0);
RWTexture2D<float4> Output : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= cb_dst_width || dtid.y >= cb_dst_height)
        return;

    // Map output pixel to normalised crop-space [0, 1]
    float u = (float(dtid.x) + 0.5f) / float(cb_dst_width);
    float v = (float(dtid.y) + 0.5f) / float(cb_dst_height);

    // Map to source normalised coordinates [crop_left..crop_right] x [crop_top..crop_bottom]
    float src_u = cb_crop_left + u * (cb_crop_right  - cb_crop_left);
    float src_v = cb_crop_top  + v * (cb_crop_bottom - cb_crop_top);

    // Apply rotation around the crop centre in pixel-aspect-corrected space
    // (normalized UV space is not square, so rotate in equal-pixel-size space)
    if (abs(cb_rotation) > 0.001f)
    {
        float cx = (cb_crop_left + cb_crop_right)  * 0.5f;
        float cy = (cb_crop_top  + cb_crop_bottom) * 0.5f;
        float aspect = float(cb_src_width) / float(cb_src_height);
        float dx = (src_u - cx) * aspect;  // scale to square pixel space
        float dy = src_v - cy;
        float rot_x = dx * cb_cos_r - dy * cb_sin_r;
        float rot_y = dx * cb_sin_r + dy * cb_cos_r;
        src_u = cx + rot_x / aspect;       // scale back to normalized
        src_v = cy + rot_y;
    }

    // Black-fill pixels that rotate outside the source image boundary
    if (src_u < 0.0f || src_u > 1.0f || src_v < 0.0f || src_v > 1.0f)
    {
        Output[dtid.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    // Convert to pixel coordinates (fractional, centre-of-pixel convention)
    float px = src_u * float(cb_src_width)  - 0.5f;
    float py = src_v * float(cb_src_height) - 0.5f;

    // Bilinear interpolation via integer Load (avoids sampler dependency)
    int x0 = (int)floor(px);
    int y0 = (int)floor(py);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    float fx = px - float(x0);
    float fy = py - float(y0);

    x0 = clamp(x0, 0, (int)cb_src_width  - 1);
    x1 = clamp(x1, 0, (int)cb_src_width  - 1);
    y0 = clamp(y0, 0, (int)cb_src_height - 1);
    y1 = clamp(y1, 0, (int)cb_src_height - 1);

    float4 p00 = Input.Load(int3(x0, y0, 0));
    float4 p10 = Input.Load(int3(x1, y0, 0));
    float4 p01 = Input.Load(int3(x0, y1, 0));
    float4 p11 = Input.Load(int3(x1, y1, 0));

    float4 result = lerp(lerp(p00, p10, fx), lerp(p01, p11, fx), fy);

    Output[dtid.xy] = result;
}
