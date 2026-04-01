# ── ImGui (直接編譯原始碼) ──
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

# ── Core 靜態函式庫 ──
add_library(vega_core STATIC
    core/Logger.cpp
    core/Timer.cpp
)
target_include_directories(vega_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vega_core PUBLIC spdlog::spdlog)

# ── 主程式 ──
add_executable(vega WIN32
    main.cpp
    ${CMAKE_SOURCE_DIR}/resources/vega.rc
)
target_link_libraries(vega PRIVATE
    vega_core
    imgui
    ${D3D11_LIB}
    ${D3DCOMPILER_LIB}
    ${DXGI_LIB}
    dwmapi
)
target_include_directories(vega PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
