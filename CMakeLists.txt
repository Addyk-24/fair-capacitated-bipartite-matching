cmake_minimum_required(VERSION 3.20)
project(fair_capacitated_bipartite_matching VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(DAA_BUILD_TESTS "Build the unit and integration tests" ON)

# Core library: shared code in src/ plus every algorithm stage in algorithms/.
# New algorithms/*.cpp files are picked up automatically; re-run cmake if you add one.
file(GLOB DAA_CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB DAA_ALGORITHM_SOURCES CONFIGURE_DEPENDS algorithms/*.cpp)

add_library(daa_core STATIC ${DAA_CORE_SOURCES} ${DAA_ALGORITHM_SOURCES})
target_include_directories(daa_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
  target_compile_options(daa_core PUBLIC /W4)
else()
  target_compile_options(daa_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(matching_runner experiments/runners/matching_runner.cpp)
target_link_libraries(matching_runner PRIVATE daa_core)

add_executable(dataset_generator experiments/runners/dataset_generator.cpp)
target_link_libraries(dataset_generator PRIVATE daa_core)

if(DAA_BUILD_TESTS)
  enable_testing()

  # One executable per test file; name = file stem. Run from the build directory,
  # where tests may create scratch folders (test_tmp_*).
  set(DAA_TEST_SOURCES
    tests/unit/test_generator.cpp
    tests/unit/test_validation.cpp
    tests/unit/test_instance_io.cpp
    tests/integration/test_pipeline.cpp
  )
  foreach(test_source IN LISTS DAA_TEST_SOURCES)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE daa_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()
