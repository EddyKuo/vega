# GPU Pipeline Implementation Plan

## Current State Audit

### What runs on CPU (ALL of these block the UI or background thread)

| Operation | CPU Time (Release, 5920x3950) | Where |
|-----------|-------------------------------|-------|
| Demosaic (bilinear) | 230ms | `GPUPipeline::uploadRawData()`, `Pipeline::demosaicAndTransform()` |
| White Balance (cam_mul on Bayer) | 30ms | Inside demosaic |
| Color Matrix (rgb_cam 3x3) | 20ms | Inside demosaic |
| memcpy demosaic cache (140MB) | 50ms | `Pipeline::process()` |
| Exposure + Contrast + H/S/W/B | 35ms | `ExposureNode` |
| Tone Curve (cubic spline LUT) | <1ms (identity skip) | `ToneCurveNode` |
| HSL 8-channel + Vibrance | ~80ms (when active) | `HSLNode` |
| Denoise (box blur YCbCr) | 300ms | `DenoiseNode` |
| Sharpen (USM, box blur) | 230ms | `SharpenNode` |
| sRGB gamma + RGBA8 quantize | 80ms | `Pipeline::toRGBA8()` |
| Texture upload (90MB RGBA8) | 10ms | `uploadToGPU()` |
| Histogram (23M pixel scan) | 20ms | `HistogramView::compute()` |
| **Total (all active)** | **~1100ms** | |

### What runs on GPU (currently disabled due to output bug)

| Shader | File | Status |
|--------|------|--------|
| WB + Exposure + Contrast + H/S | `white_balance_exposure.hlsl` | Compiles, output not verified |
| Tone Curve (1D LUT sample) | `tone_curve.hlsl` | Compiles, output not verified |
| HSL + Vibrance + sRGB gamma | `hsl_adjust.hlsl` | Compiles (warning), output not verified |
| Histogram (InterlockedAdd) | `histogram_compute.hlsl` | Compiles, not wired to display |

### Missing GPU operations (still CPU-only)

1. **Demosaic** -- done on CPU, uploaded as float RGBA texture
2. **Denoise** -- no GPU shader exists
3. **Sharpen** -- no GPU shader exists
4. **RGBA8 conversion** -- CPU `toRGBA8()`, should be final GPU pass
5. **Histogram readback** -- GPU histogram exists but not wired to HistogramView

---

## Implementation Plan

### Phase G1: Fix existing GPU pipeline output (Priority: CRITICAL)

The GPU pipeline compiles and dispatches all 3 passes but produces blank output.
Root cause investigation needed:

**Likely issues:**
1. `uploadRawData()` packs RGB as RGBA float but the WB shader reads `Input[dtid.xy]`
   which expects the texture format to match. Verify the raw texture format is
   `R32G32B32A32_FLOAT` and the alpha channel is 1.0.
2. The WB shader applies its own WB multipliers on top of data that already has
   camera WB applied (double WB). The `uploadRawData()` does CPU demosaic + WB + color
   matrix, then the shader applies recipe WB again. Need to separate camera WB
   (baked into upload) from user WB adjustment (shader does relative correction).
3. HSL shader outputs sRGB gamma, but the output texture is `R32G32B32A32_FLOAT`.
   ImGui samples it as float, so values in [0,1] after gamma should display correctly.
   Verify the shader actually writes to the UAV.

**Steps:**
- Add D3D11 debug layer validation (requires Windows Graphics Tools feature)
- Add readback of output texture to verify pixel values
- Compare GPU output vs CPU output for same recipe
- Fix any mismatches

### Phase G2: GPU Demosaic shader (saves 230ms)

Create `shaders/demosaic_bilinear.hlsl`:

```
Input:  Texture2D<float> BayerData (single channel, W x H)
Output: RWTexture2D<float4> RGB (3 channel + alpha)
cbuffer: width, height, bayer_pattern, wb_mul[4], color_matrix[9]
```

Algorithm:
- Each thread handles one pixel
- Read bayer channel from pattern lookup table
- Bilinear interpolate missing channels from neighbors (same as CPU)
- Apply WB multipliers per-channel BEFORE interpolation (critical for correct color)
- Apply rgb_cam 3x3 color matrix
- Write float4(r, g, b, 1.0)

This eliminates the CPU demosaic (230ms) and the 140MB memcpy.
Raw Bayer data uploads once as `R32_FLOAT` texture (single channel).

```hlsl
[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= width || dtid.y >= height) return;

    int ch = bayerChannel(bayer_pattern, dtid.y, dtid.x);
    float center = BayerData[dtid.xy] * wb_mul[ch];

    // Bilinear interpolate other channels (with WB)
    float R, G, B;
    // ... same logic as CPU demosaic but reading from texture with clamp addressing ...

    // Apply color matrix
    float3 srgb;
    srgb.r = M[0]*R + M[1]*G + M[2]*B;
    srgb.g = M[3]*R + M[4]*G + M[5]*B;
    srgb.b = M[6]*R + M[7]*G + M[8]*B;

    Output[dtid.xy] = float4(srgb, 1.0);
}
```

### Phase G3: GPU Denoise shader (saves 300ms)

Create `shaders/denoise.hlsl`:

Two approaches, in order of implementation difficulty:

**Option A: Separable box blur (simple, fast)**
```
Pass 1: Horizontal blur on Y channel (groupshared memory for row caching)
Pass 2: Vertical blur on Y channel
Pass 3: Horizontal blur on Cb, Cr (can be combined)
Pass 4: Vertical blur on Cb, Cr
Pass 5: Blend Y (edge-preserving) + write RGB
```

Each pass: [numthreads(256, 1, 1)] for horizontal, [numthreads(1, 256, 1)] for vertical.
Use `groupshared float` to cache a row/column tile, eliminating random texture reads.

**Option B: Single-pass bilateral approximation (advanced)**
```
For each pixel:
  Read NxN neighborhood from texture (texture cache handles locality)
  Weighted average based on spatial + range distance
  Write output
```

Bilateral on GPU is fast because texture cache is 2D-optimized.
[numthreads(16,16,1)], radius 3 = 49 texture reads per thread, all from cache.

Recommended: Option A first (correct, matches CPU), Option B later for quality.

### Phase G4: GPU Sharpen shader (saves 230ms)

Create `shaders/sharpen_usm.hlsl`:

```
Pass 1: Compute luminance + box blur (horizontal, groupshared)
Pass 2: Box blur vertical
Pass 3: USM application: pixel += amount * (lum - blurred_lum)
```

Can merge Pass 3 with the final gamma conversion to save a pass.

Alternative: single-pass USM with small radius (3x3 or 5x5 kernel hardcoded):
```hlsl
[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    float4 center = Input[dtid.xy];
    float lum_center = dot(center.rgb, float3(0.2126, 0.7152, 0.0722));

    // 3x3 average for blur
    float lum_blur = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            lum_blur += dot(Input[dtid.xy + int2(dx,dy)].rgb, float3(0.2126, 0.7152, 0.0722));
    lum_blur /= 9.0;

    float detail = lum_center - lum_blur;
    float3 sharpened = center.rgb + amount * detail;
    Output[dtid.xy] = float4(sharpened, 1.0);
}
```

### Phase G5: GPU Histogram (saves 20ms + enables real-time)

Already implemented in `histogram_compute.hlsl` with `groupshared` + `InterlockedAdd`.
Need to wire it:

1. Dispatch after the final processing pass
2. Readback the 4x256 `RWBuffer<uint>` to CPU (staging buffer, async)
3. Feed to `HistogramView` for rendering

Use `ID3D11DeviceContext::CopyResource` to staging buffer, then `Map` to read.
This can overlap with the next frame's rendering (async readback).

### Phase G6: Eliminate CPU-GPU copies

Full GPU pipeline eliminates ALL CPU processing after initial Bayer upload:

```
CPU: Read RAW file -> extract Bayer uint16 -> normalize to float -> upload R32_FLOAT texture (once)

GPU (every slider change, <10ms total on RTX 3090):
  Dispatch 1: Demosaic + WB + Color Matrix     (Bayer R32 -> RGBA32F)
  Dispatch 2: Exposure + Contrast + H/S/W/B    (RGBA32F -> RGBA32F)
  Dispatch 3: Tone Curve (1D LUT sample)        (RGBA32F -> RGBA32F)
  Dispatch 4: HSL + Vibrance + Saturation       (RGBA32F -> RGBA32F)
  Dispatch 5: Denoise (separable blur + blend)  (RGBA32F -> RGBA32F)
  Dispatch 6: Sharpen (USM)                     (RGBA32F -> RGBA32F)
  Dispatch 7: sRGB Gamma (final output)         (RGBA32F -> RGBA8, display-ready)
  Dispatch 8: Histogram (async readback)        (RGBA8 -> 4x256 uint buffer)

ImGui: display the RGBA8 SRV directly. Zero CPU readback for display.
```

**Expected performance (RTX 3090, 5920x3950):**

| Dispatch | Estimated Time |
|----------|---------------|
| Demosaic + WB + Color | 3-5ms |
| Exposure | 1-2ms |
| Tone Curve | <1ms |
| HSL | 1-2ms |
| Denoise (4-pass blur) | 2-4ms |
| Sharpen (USM) | 1-2ms |
| Gamma + RGBA8 | <1ms |
| Histogram | <1ms |
| **Total** | **~10-15ms** |

vs current CPU: **~1100ms** (all nodes active, full res)

---

## Shader Optimization Guidelines

1. **[numthreads(16,16,1)]** for 2D image ops (256 threads = 1 warp on NVIDIA x 8)
2. **groupshared memory** for blur kernels — cache a tile of input, sync, process
3. **Texture sampling** with `SamplerState` for bilinear reads (free hardware interpolation)
4. **Avoid divergence** — all threads in a group should take the same branch
5. **Merge passes** where possible (e.g. Exposure+Contrast in one shader)
6. **Float16 intermediates** — `DXGI_FORMAT_R16G16B16A16_FLOAT` between passes saves VRAM bandwidth (half the bytes)
7. **Boundary check** — `if (dtid.x >= width || dtid.y >= height) return;` at top of every shader
8. **Constant buffers** — 16-byte aligned, minimize CB updates between dispatches

---

## Implementation Priority

| Phase | Impact | Effort | Priority |
|-------|--------|--------|----------|
| G1: Fix GPU output | Unlocks all GPU | Medium | 1 - NOW |
| G2: GPU Demosaic | -230ms | Medium | 2 |
| G3: GPU Denoise | -300ms | Medium | 3 |
| G4: GPU Sharpen | -230ms | Low | 4 |
| G5: GPU Histogram | -20ms, real-time | Low | 5 |
| G6: Eliminate copies | -140ms | Low | 6 |

G1 is the blocker. Once the existing 3 shaders produce correct output,
the pipeline goes from 1100ms to ~40ms (WB+Exp+Tone+HSL on GPU, rest CPU).
Then G2-G6 incrementally remove the remaining CPU work.
