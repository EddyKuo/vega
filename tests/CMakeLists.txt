find_package(Catch2 CONFIG REQUIRED)

add_executable(vega_tests
    test_pipeline.cpp
)
target_link_libraries(vega_tests PRIVATE
    Catch2::Catch2WithMain
    vega_core
    vega_raw
    vega_pipeline
)
target_include_directories(vega_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Run tests from the project root so samples/ path resolves
set_target_properties(vega_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
