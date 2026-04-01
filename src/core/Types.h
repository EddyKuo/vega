#pragma once
#include <cstdint>
#include <cstddef>

namespace vega
{

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

struct Rect
{
    f32 x, y, width, height;
};

struct Size
{
    u32 width, height;
};

} // namespace vega
