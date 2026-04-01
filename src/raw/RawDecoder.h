#pragma once
#include "RawImage.h"
#include "core/Result.h"
#include <filesystem>

namespace vega {

enum class RawDecodeError {
    FileNotFound,
    UnsupportedFormat,
    CorruptFile,
    OutOfMemory,
    LibRawError
};

class RawDecoder {
public:
    static Result<RawImage, RawDecodeError>
        decode(const std::filesystem::path& filepath);

    static Result<RawImageMetadata, RawDecodeError>
        readMetadata(const std::filesystem::path& filepath);

    static Result<std::vector<uint8_t>, RawDecodeError>
        extractThumbnail(const std::filesystem::path& filepath);
};

} // namespace vega
