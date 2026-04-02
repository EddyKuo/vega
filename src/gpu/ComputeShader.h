#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <filesystem>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace vega {

class ComputeShader {
public:
    bool compileFromFile(ID3D11Device* device, const std::filesystem::path& path,
                         const char* entry_point = "CSMain", const char* target = "cs_5_0");
    bool loadFromCSO(ID3D11Device* device, const std::filesystem::path& cso_path);

    void bind(ID3D11DeviceContext* ctx) const;
    void dispatch(ID3D11DeviceContext* ctx, uint32_t x, uint32_t y, uint32_t z = 1) const;

    bool isValid() const { return shader_ != nullptr; }
    const std::string& lastError() const { return last_error_; }

private:
    ComPtr<ID3D11ComputeShader> shader_;
    std::string last_error_;
};

} // namespace vega
