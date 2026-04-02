---
name: dx11-gpu
description: DirectX 11 GPU programming — compute shaders, HLSL, texture management, D3D11 API
allowed-tools: Read, Write, Edit, Glob, Grep, Bash
---

# DirectX 11 & GPU Compute — Vega Project

You are an expert in D3D11 compute shader programming for real-time image processing.

## Architecture
- **API**: D3D11 with DXGI 1.2 (flip-discard swap chain)
- **Shaders**: HLSL Compute Shaders (cs_5_0)
- **Thread Groups**: `[numthreads(16, 16, 1)]` for 2D image processing
- **Texture Format**: `DXGI_FORMAT_R32G32B32A32_FLOAT` (pipeline), `DXGI_FORMAT_R8G8B8A8_UNORM` (display)

## D3D11 Patterns (Vega)
```cpp
// Use ComPtr for COM objects
using Microsoft::WRL::ComPtr;

// Creating textures
ComPtr<ID3D11Texture2D> tex = g_ctx.createTexture2D(w, h, format, /*uav=*/true);
ComPtr<ID3D11ShaderResourceView> srv = g_ctx.createSRV(tex.Get());
ComPtr<ID3D11UnorderedAccessView> uav = g_ctx.createUAV(tex.Get());

// Dispatch compute shader
ctx->CSSetShader(shader, nullptr, 0);
ctx->CSSetShaderResources(0, 1, srv.GetAddressOf());
ctx->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
ctx->Dispatch((w+15)/16, (h+15)/16, 1);
```

## HLSL Compute Shader Template
```hlsl
// common.hlsli — shared utilities
float3 LinearToSRGB(float3 c) {
    return select(c < 0.0031308, c * 12.92, 1.055 * pow(c, 1.0/2.4) - 0.055);
}
float Luminance(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

// Per-shader pattern
cbuffer Constants : register(b0) { float4 params; };
Texture2D<float4> Input : register(t0);
RWTexture2D<float4> Output : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    uint2 dims; Output.GetDimensions(dims.x, dims.y);
    if (dtid.x >= dims.x || dtid.y >= dims.y) return;
    float4 pixel = Input[dtid.xy];
    // ... processing ...
    Output[dtid.xy] = pixel;
}
```

## Shader Hot Reload (Debug)
- Watch `shaders/` directory with `ReadDirectoryChangesW`
- Recompile with `D3DCompileFromFile` on change
- Swap shader pointer atomically

## Release: Precompiled Shaders
- CMake custom command: `fxc.exe /T cs_5_0 /Fo output.cso input.hlsl`
- Load `.cso` at startup with `CreateComputeShader(bytecode, size, ...)`

## Performance Tips
- Minimize CPU↔GPU copies (use staging textures for readback)
- Use `ID3D11Query` (TIMESTAMP) for GPU profiling
- GroupShared memory for tile-local reductions (histogram, blur)
- Half precision (`min16float`) for intermediate buffers where acceptable
