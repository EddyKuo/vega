#include "gpu/ComputeShader.h"
#include "core/Logger.h"

#include <fstream>
#include <vector>

namespace vega {

bool ComputeShader::compileFromFile(ID3D11Device* device, const std::filesystem::path& path,
                                    const char* entry_point, const char* target)
{
    if (!device) {
        last_error_ = "compileFromFile called with null device";
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    if (!std::filesystem::exists(path)) {
        last_error_ = "Shader file not found: " + path.string();
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    UINT compile_flags = 0;
#ifdef _DEBUG
    compile_flags |= D3DCOMPILE_DEBUG;
    compile_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error_blob;

    // Convert path to wide string for D3DCompileFromFile
    std::wstring wide_path = path.wstring();

    HRESULT hr = D3DCompileFromFile(
        wide_path.c_str(),
        nullptr,                    // defines
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // include handler (supports #include)
        entry_point,
        target,
        compile_flags,
        0,                          // effect flags
        &blob,
        &error_blob);

    if (FAILED(hr)) {
        if (error_blob) {
            last_error_ = std::string(
                static_cast<const char*>(error_blob->GetBufferPointer()),
                error_blob->GetBufferSize());
        } else {
            last_error_ = "D3DCompileFromFile failed with HRESULT 0x" +
                          std::to_string(static_cast<uint32_t>(hr));
        }
        VEGA_LOG_ERROR("Shader compilation failed for '{}': {}", path.string(), last_error_);
        return false;
    }

    // Log warnings if any
    if (error_blob && error_blob->GetBufferSize() > 0) {
        std::string warnings(
            static_cast<const char*>(error_blob->GetBufferPointer()),
            error_blob->GetBufferSize());
        VEGA_LOG_WARN("Shader compilation warnings for '{}': {}", path.string(), warnings);
    }

    hr = device->CreateComputeShader(
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        nullptr,
        &shader_);

    if (FAILED(hr)) {
        last_error_ = "CreateComputeShader failed with HRESULT 0x" +
                      std::to_string(static_cast<uint32_t>(hr));
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    VEGA_LOG_INFO("Compiled compute shader '{}' (entry: {}, target: {})",
                  path.filename().string(), entry_point, target);
    last_error_.clear();
    return true;
}

bool ComputeShader::loadFromCSO(ID3D11Device* device, const std::filesystem::path& cso_path)
{
    if (!device) {
        last_error_ = "loadFromCSO called with null device";
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    if (!std::filesystem::exists(cso_path)) {
        last_error_ = "CSO file not found: " + cso_path.string();
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    // Read the entire .cso file into memory
    std::ifstream file(cso_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        last_error_ = "Failed to open CSO file: " + cso_path.string();
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    auto file_size = file.tellg();
    if (file_size <= 0) {
        last_error_ = "CSO file is empty: " + cso_path.string();
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    std::vector<uint8_t> bytecode(static_cast<size_t>(file_size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytecode.data()), file_size);
    file.close();

    HRESULT hr = device->CreateComputeShader(
        bytecode.data(),
        bytecode.size(),
        nullptr,
        &shader_);

    if (FAILED(hr)) {
        last_error_ = "CreateComputeShader from CSO failed with HRESULT 0x" +
                      std::to_string(static_cast<uint32_t>(hr));
        VEGA_LOG_ERROR("{}", last_error_);
        return false;
    }

    VEGA_LOG_INFO("Loaded precompiled compute shader '{}'", cso_path.filename().string());
    last_error_.clear();
    return true;
}

void ComputeShader::bind(ID3D11DeviceContext* ctx) const
{
    if (!ctx || !shader_) return;
    ctx->CSSetShader(shader_.Get(), nullptr, 0);
}

void ComputeShader::dispatch(ID3D11DeviceContext* ctx, uint32_t x, uint32_t y, uint32_t z) const
{
    if (!ctx || !shader_) return;
    ctx->CSSetShader(shader_.Get(), nullptr, 0);
    ctx->Dispatch(x, y, z);
}

} // namespace vega
