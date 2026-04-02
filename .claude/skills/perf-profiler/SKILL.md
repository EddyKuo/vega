---
name: perf-profiler
description: Profile and optimize Vega's image processing pipeline — CPU timing, GPU timing, memory usage, bottleneck analysis
allowed-tools: Read, Write, Edit, Bash, Grep
---

# Performance Profiler — Vega Image Pipeline

You are a performance optimization expert for real-time image processing.

## Profiling Tools
- **CPU**: `vega::Timer` (QueryPerformanceCounter), `vega::ScopeTimer` (RAII)
- **GPU**: D3D11 timestamp queries (`ID3D11Query`, `D3D11_QUERY_TIMESTAMP`)
- **System**: Windows Performance Monitor, VS Diagnostic Tools
- **Memory**: `GetProcessMemoryInfo()` for working set tracking

## Performance Targets (from plan.md)
| Operation | Target | Conditions |
|-----------|--------|------------|
| RAW decode | < 500ms | 60MP CR3, NVMe |
| Demosaic GPU | < 20ms | 60MP, RTX 3060 |
| Full pipeline GPU | < 200ms | 60MP, RTX 3060 |
| Fast preview | < 5ms | 1/8 resolution |
| Slider response | < 16ms | Perceived latency |
| Thumbnail gen | < 100ms/image | |
| JPEG export | < 1s | 60MP→20MP Q92 |
| Memory | < 2GB | 60MP RAW loaded |

## Optimization Strategies

### CPU Pipeline
1. **Tile processing**: 256x256 tiles with jthread pool
2. **SIMD**: AVX2 for pixel math (8 floats per operation)
3. **Cache**: Stage caching — only reprocess from dirty stage
4. **Downscale preview**: 1/4 or 1/8 res during slider drag, full res on release

### GPU Pipeline
1. **Merge passes**: Combine WB + Exposure into single dispatch
2. **GroupShared**: Local memory for blur kernels, histogram
3. **fp16 intermediates**: `R16G16B16A16_FLOAT` between stages
4. **Async compute**: Pipeline overlap between CPU decode and GPU process

### Memory
1. **Tile buffer pool**: Reuse allocations across frames
2. **Release intermediate buffers** after pipeline completes
3. **Progressive decode**: Only decode visible region at full res
4. **Working set monitoring**: Alert if approaching 2GB limit

## Profiling Workflow
1. Add `ScopeTimer` to suspect functions
2. Run with representative test image (60MP)
3. Identify bottleneck (decode? demosaic? tone curve? upload?)
4. Optimize hottest path first
5. Verify improvement with before/after timing
6. Check for memory leaks (VS Diagnostic Tools)
