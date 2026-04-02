# ── ImGui (source build) ──
add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC ${D3D11_LIB} ${DXGI_LIB})

# ── Core ──
add_library(vega_core STATIC
    core/Logger.cpp
    core/Timer.cpp
    core/CrashHandler.cpp
    core/Settings.cpp
    core/WindowsIntegration.cpp
)
target_include_directories(vega_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_core PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json dwmapi)

# ── GPU ──
add_library(vega_gpu STATIC
    gpu/D3D11Context.cpp
    gpu/ComputeShader.cpp
    gpu/TexturePool.cpp
)
target_include_directories(vega_gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_gpu PUBLIC
    vega_core
    ${D3D11_LIB}
    ${D3DCOMPILER_LIB}
    ${DXGI_LIB}
)

# ── Pipeline ──
add_library(vega_pipeline STATIC
    pipeline/SimplePipeline.cpp
    pipeline/EditRecipe.cpp
    pipeline/EditHistory.cpp
    pipeline/Pipeline.cpp
    pipeline/GPUPipeline.cpp
    pipeline/PreviewManager.cpp
    pipeline/cpu/WhiteBalanceNode.cpp
    pipeline/cpu/ExposureNode.cpp
    pipeline/cpu/ToneCurveNode.cpp
    pipeline/cpu/HSLNode.cpp
    pipeline/cpu/ColorSpaceNode.cpp
)
target_include_directories(vega_pipeline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_pipeline PUBLIC vega_core vega_gpu nlohmann_json::nlohmann_json)

# ── Raw ──
add_library(vega_raw STATIC
    raw/RawDecoder.cpp
    raw/ExifReader.cpp
)
target_include_directories(vega_raw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_raw PUBLIC vega_core libraw::raw Exiv2::exiv2lib)

# ── Export ──
add_library(vega_export STATIC
    export/ExportManager.cpp
)
target_include_directories(vega_export PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_export PUBLIC vega_core JPEG::JPEG PNG::PNG TIFF::TIFF)

# ── Catalog ──
add_library(vega_catalog STATIC
    catalog/Database.cpp
    catalog/ThumbnailCache.cpp
    catalog/ImportManager.cpp
)
target_include_directories(vega_catalog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_catalog PUBLIC vega_core vega_raw unofficial::sqlite3::sqlite3 ${D3D11_LIB})

# ── UI ──
add_library(vega_ui STATIC
    ui/ImageViewport.cpp
    ui/DevelopPanel.cpp
    ui/HistogramView.cpp
    ui/BeforeAfter.cpp
    ui/ExportDialog.cpp
    ui/GridView.cpp
    ui/Toolbar.cpp
    ui/StatusBar.cpp
)
target_include_directories(vega_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_ui PUBLIC imgui vega_pipeline vega_catalog vega_export ${D3D11_LIB})

# ── Main executable ──
add_executable(vega WIN32
    main.cpp
    ${CMAKE_SOURCE_DIR}/resources/vega.rc
)
target_link_libraries(vega PRIVATE
    vega_core
    vega_gpu
    vega_pipeline
    vega_ui
    vega_raw
    vega_export
    vega_catalog
    imgui
    ${D3D11_LIB}
    ${D3DCOMPILER_LIB}
    ${DXGI_LIB}
    dwmapi
    comdlg32
    shell32
    ole32
)
target_include_directories(vega PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
