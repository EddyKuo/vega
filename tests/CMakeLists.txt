find_package(Catch2 CONFIG REQUIRED)
include(Catch2::Catch2WithMain)

# Tests will be added as modules are implemented
# add_executable(vega_tests test_raw_decode.cpp)
# target_link_libraries(vega_tests PRIVATE Catch2::Catch2WithMain vega_core)
# catch_discover_tests(vega_tests)
