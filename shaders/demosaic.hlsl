// ============================================================================
// Vega -- demosaic.hlsl
// GPU Bayer demosaic (bilinear) + camera WB + color matrix
// Input:  single-channel float bayer texture (t0)
// Output: linear RGB float4 texture (u0)
// ============================================================================

cbuffer DemosaicParams : register(b0)
{
    uint   cb_width;
    uint   cb_height;
    uint   cb_bayer_pattern;  // 0=RGGB, 1=BGGR, 2=GRBG, 3=GBRG
    float  cb_pad0;

    float  cb_wb_r;           // camera WB multiplier (normalized, G=1)
    float  cb_wb_g;
    float  cb_wb_b;
    float  cb_pad1;

    float4 cb_color_row0;     // color matrix row 0
    float4 cb_color_row1;     // color matrix row 1
    float4 cb_color_row2;     // color matrix row 2
};

Texture2D<float>    BayerInput : register(t0);
RWTexture2D<float4> RGBOutput  : register(u0);

// Bayer channel lookup: 0=R, 1=G, 2=B
static const int bayerTable[4][4] = {
    {0, 1, 1, 2},  // RGGB
    {2, 1, 1, 0},  // BGGR
    {1, 0, 2, 1},  // GRBG
    {1, 2, 0, 1},  // GBRG
};

int getBayerChannel(int row, int col)
{
    uint p = cb_bayer_pattern < 4 ? cb_bayer_pattern : 0;
    return bayerTable[p][(row & 1) * 2 + (col & 1)];
}

float sampleWB(int r, int c)
{
    r = clamp(r, 0, (int)cb_height - 1);
    c = clamp(c, 0, (int)cb_width - 1);
    float val = BayerInput.Load(int3(c, r, 0));
    int ch = getBayerChannel(r, c);
    float wb[3] = { cb_wb_r, cb_wb_g, cb_wb_b };
    return val * wb[ch];
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    int r = (int)dtid.y;
    int c = (int)dtid.x;
    if ((uint)c >= cb_width || (uint)r >= cb_height)
        return;

    int ch = getBayerChannel(r, c);
    float center = sampleWB(r, c);
    float R, G, B;

    if (ch == 0) // Red pixel
    {
        R = center;
        G = (sampleWB(r-1,c) + sampleWB(r+1,c) + sampleWB(r,c-1) + sampleWB(r,c+1)) * 0.25f;
        B = (sampleWB(r-1,c-1) + sampleWB(r-1,c+1) + sampleWB(r+1,c-1) + sampleWB(r+1,c+1)) * 0.25f;
    }
    else if (ch == 2) // Blue pixel
    {
        B = center;
        G = (sampleWB(r-1,c) + sampleWB(r+1,c) + sampleWB(r,c-1) + sampleWB(r,c+1)) * 0.25f;
        R = (sampleWB(r-1,c-1) + sampleWB(r-1,c+1) + sampleWB(r+1,c-1) + sampleWB(r+1,c+1)) * 0.25f;
    }
    else // Green pixel
    {
        G = center;
        int chUp = getBayerChannel(r-1, c);

        if (chUp == 0) {
            R = (sampleWB(r-1,c) + sampleWB(r+1,c)) * 0.5f;
            B = (sampleWB(r,c-1) + sampleWB(r,c+1)) * 0.5f;
        } else if (chUp == 2) {
            B = (sampleWB(r-1,c) + sampleWB(r+1,c)) * 0.5f;
            R = (sampleWB(r,c-1) + sampleWB(r,c+1)) * 0.5f;
        } else {
            int chLeft = getBayerChannel(r, c-1);
            if (chLeft == 0) {
                R = (sampleWB(r,c-1) + sampleWB(r,c+1)) * 0.5f;
                B = (sampleWB(r-1,c) + sampleWB(r+1,c)) * 0.5f;
            } else {
                B = (sampleWB(r,c-1) + sampleWB(r,c+1)) * 0.5f;
                R = (sampleWB(r-1,c) + sampleWB(r+1,c)) * 0.5f;
            }
        }
    }

    // Apply color matrix (camera RGB -> sRGB)
    float3 rgb = float3(R, G, B);
    float3 out_rgb;
    out_rgb.x = dot(rgb, cb_color_row0.xyz);
    out_rgb.y = dot(rgb, cb_color_row1.xyz);
    out_rgb.z = dot(rgb, cb_color_row2.xyz);

    // Check if matrix is identity/zero (no matrix available)
    bool has_matrix = any(cb_color_row0.xyz != float3(0,0,0));
    if (!has_matrix)
        out_rgb = rgb;

    RGBOutput[dtid.xy] = float4(out_rgb, 1.0f);
}
