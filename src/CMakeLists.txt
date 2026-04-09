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
    core/UIStateDB.cpp
    core/WindowsIntegration.cpp
    core/i18n.cpp
)
target_include_directories(vega_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_core PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json unofficial::sqlite3::sqlite3 dwmapi)

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
    pipeline/cpu/SharpenNode.cpp
    pipeline/cpu/DenoiseNode.cpp
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
    ui/FolderPanel.cpp
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

# ── Precompile HLSL shaders to CSO ──
if(FXC_EXECUTABLE)
    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    set(VEGA_HLSL_SHADERS
        demosaic.hlsl
        white_balance_exposure.hlsl
        tone_curve.hlsl
        hsl_adjust.hlsl
        denoise.hlsl
        sharpen_usm.hlsl
        gamma_output.hlsl
        histogram_compute.hlsl
    )

    set(FXC_FLAGS /nologo /T cs_5_0 /E CSMain)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND FXC_FLAGS /Zi /Od)
    else()
        list(APPEND FXC_FLAGS /O3)
    endif()

    set(SHADER_CSO_OUTPUTS "")
    foreach(SHADER_FILE ${VEGA_HLSL_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        set(SHADER_INPUT  "${SHADER_SOURCE_DIR}/${SHADER_FILE}")
        set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.cso")

        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${FXC_EXECUTABLE} ${FXC_FLAGS}
                    /I "${SHADER_SOURCE_DIR}"
                    /Fo "${SHADER_OUTPUT}"
                    "${SHADER_INPUT}"
            DEPENDS ${SHADER_INPUT} "${SHADER_SOURCE_DIR}/common.hlsli"
            COMMENT "Compiling shader ${SHADER_FILE} -> ${SHADER_NAME}.cso"
            VERBATIM
        )
        list(APPEND SHADER_CSO_OUTPUTS ${SHADER_OUTPUT})
    endforeach()

    add_custom_target(compile_shaders ALL DEPENDS ${SHADER_CSO_OUTPUTS})
    add_dependencies(vega compile_shaders)

    # Copy CSO files next to the executable after build
    add_custom_command(TARGET vega POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vega>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:vega>/shaders"
        COMMENT "Copying compiled shaders to output directory"
    )

    # In Debug, also copy HLSL sources for live editing
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(TARGET vega POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SHADER_SOURCE_DIR}"
                "$<TARGET_FILE_DIR:vega>/shaders"
            COMMENT "Copying HLSL sources for debug"
        )
    endif()
endif()

# ── Build language.db generator tool ──
add_executable(gen_langdb EXCLUDE_FROM_ALL
    "${CMAKE_SOURCE_DIR}/tools/gen_langdb.cpp"
)
target_link_libraries(gen_langdb PRIVATE unofficial::sqlite3::sqlite3)
target_include_directories(gen_langdb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(LANG_SEED "${CMAKE_SOURCE_DIR}/resources/language_seed.sql")
set(LANG_DB   "${CMAKE_CURRENT_BINARY_DIR}/language.db")

add_custom_command(
    OUTPUT ${LANG_DB}
    COMMAND $<TARGET_FILE:gen_langdb> "${LANG_SEED}" "${LANG_DB}"
    DEPENDS gen_langdb ${LANG_SEED}
    COMMENT "Generating language.db from SQL seed"
    VERBATIM
)
add_custom_target(generate_language_db ALL DEPENDS ${LANG_DB})
add_dependencies(vega generate_language_db)

add_custom_command(TARGET vega POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LANG_DB}"
        "$<TARGET_FILE_DIR:vega>/language.db"
    COMMENT "Copying language.db to output directory"
)
