FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(EngineTests
    engine/WavetableTests.cpp
    engine/OscillatorTests.cpp
    engine/AdsrTests.cpp)
target_include_directories(EngineTests PRIVATE ${CMAKE_SOURCE_DIR}/plugin)
target_compile_features(EngineTests PRIVATE cxx_std_20)
target_link_libraries(EngineTests PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(EngineTests)
