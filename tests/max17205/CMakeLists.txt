# Host-side unit tests for src/drivers/max17205.c.
#
# Standalone CMake project, same shape as tests/i2c_bus: no Pico SDK, no cross
# toolchain, plain host compiler.
#
#   cmake -S tests/max17205 -B build/tests/max17205
#   cmake --build build/tests/max17205
#   ctest --test-dir build/tests/max17205 --output-on-failure
#
# The cut is one layer higher than the i2c_bus suite: max17205.c is linked
# against fakes/i2c_bus_fake.c rather than src/board/i2c_bus.c, so a failure
# here is a driver bug and never an i2c_bus bug. The Pico SDK headers still
# have to resolve (argus/i2c_bus.h includes them), so the stubs from
# tests/i2c_bus/fakes are reused for those -- no second copy to keep in step.
#
# Requires the Unity submodule at <repo>/unity:
#   git submodule update --init --recursive
# Generating the test runner also needs Ruby (macOS ships /usr/bin/ruby).

cmake_minimum_required(VERSION 3.16)  # 3.16 for SKIP_REGULAR_EXPRESSION
project(max17205_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(UNITY_DIR "${REPO_ROOT}/unity")
set(I2C_BUS_TEST_DIR "${REPO_ROOT}/tests/i2c_bus")

if(NOT EXISTS "${UNITY_DIR}/src/unity.c")
    message(FATAL_ERROR
        "Unity not found at ${UNITY_DIR}.\n"
        "Unity is a git submodule (see the `unity` branch). Fetch it with:\n"
        "  git submodule update --init --recursive")
endif()

find_program(RUBY_EXECUTABLE NAMES ruby)
if(NOT RUBY_EXECUTABLE)
    message(FATAL_ERROR
        "Ruby not found. Unity's test-runner generator "
        "(unity/auto/generate_test_runner.rb) needs it. macOS ships "
        "/usr/bin/ruby; otherwise `brew install ruby`.")
endif()

# Built directly rather than via add_subdirectory(): Unity's own CMakeLists
# compiles with -Werror and a very wide warning set we do not want to inherit.
if(NOT TARGET unity)
    add_library(unity STATIC "${UNITY_DIR}/src/unity.c")
    target_include_directories(unity PUBLIC "${UNITY_DIR}/src")
    # Compiles UnityParseOptions, which gives the generated runner its -l and
    # -n flags. Test discovery below depends on both.
    target_compile_definitions(unity PUBLIC UNITY_USE_COMMAND_LINE_ARGS)
endif()

# The fake i2c_bus, plus the SDK header stubs it and max17205.h need.
#
# Include order matters: fakes/ here supplies pico/float.h (which max17205.h
# includes and the i2c_bus suite never needed); tests/i2c_bus/fakes supplies
# hardware/i2c.h, pico/sync.h, pico/stdlib.h and pico/binary_info.h.
add_library(i2c_bus_fake STATIC
    fakes/i2c_bus_fake.c
    "${I2C_BUS_TEST_DIR}/fakes/pico_fake.c"   # only for the i2c0/i2c1 instances
)
target_include_directories(i2c_bus_fake PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
    "${I2C_BUS_TEST_DIR}/fakes"
    "${REPO_ROOT}/include"
)
target_compile_options(i2c_bus_fake PRIVATE -Wall -Wextra)

# The code under test, compiled for the host against the fake bus.
add_library(max17205_uut STATIC "${REPO_ROOT}/src/drivers/max17205.c")
target_link_libraries(max17205_uut PUBLIC i2c_bus_fake)
# Warnings on but not fatal: the driver is still a stub and -Werror would turn
# every unused parameter into a build break instead of a failing test.
target_compile_options(max17205_uut PRIVATE -Wall -Wextra -Wno-unused-parameter)

# ---------------------------------------------------------------------------
# Test runner generation
#
# Unity's Ruby generator scans test_max17205.c for functions named test_* and
# emits a runner holding main() and the RUN_TEST calls, so the RUN_TEST list is
# never hand-maintained: write a `void test_foo(void)` and it runs.
#
# The generator's regex only matches functions declared `^void test_...`, so
# test functions must not be `static`. Helpers in the test file still can be.
# ---------------------------------------------------------------------------
set(TEST_SOURCE "${CMAKE_CURRENT_LIST_DIR}/test_max17205.c")
set(TEST_RUNNER "${CMAKE_CURRENT_BINARY_DIR}/test_max17205_Runner.c")
set(RUNNER_GENERATOR "${UNITY_DIR}/auto/generate_test_runner.rb")

add_custom_command(
    OUTPUT  "${TEST_RUNNER}"
    COMMAND "${RUBY_EXECUTABLE}" "${RUNNER_GENERATOR}"
            --cmdline_args=1
            "${TEST_SOURCE}" "${TEST_RUNNER}"
    DEPENDS "${TEST_SOURCE}" "${RUNNER_GENERATOR}"
    COMMENT "Generating Unity test runner for test_max17205.c"
    VERBATIM
)

add_executable(test_max17205 "${TEST_SOURCE}" "${TEST_RUNNER}")
target_link_libraries(test_max17205 PRIVATE max17205_uut unity)
target_compile_options(test_max17205 PRIVATE -Wall -Wextra)

# ---------------------------------------------------------------------------
# Test discovery
#
# After every link, ask the runner which tests it contains and write one
# add_test() per test, so the CTest list can never drift from the runner.
# The discovery script is shared with the i2c_bus suite.
# ---------------------------------------------------------------------------
enable_testing()

set(CTEST_TESTS_FILE "${CMAKE_CURRENT_BINARY_DIR}/test_max17205_tests.cmake")

add_custom_command(
    TARGET test_max17205 POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            -D "TEST_EXECUTABLE=$<TARGET_FILE:test_max17205>"
            -D "TEST_PREFIX=max17205."
            -D "CTEST_FILE=${CTEST_TESTS_FILE}"
            -P "${I2C_BUS_TEST_DIR}/cmake/DiscoverUnityTests.cmake"
    COMMENT "Discovering Unity tests"
    VERBATIM
)

# Placeholder so `ctest` before the first build fails with a useful message
# rather than complaining about a missing include file.
if(NOT EXISTS "${CTEST_TESTS_FILE}")
    file(WRITE "${CTEST_TESTS_FILE}"
        "message(FATAL_ERROR \"Tests not discovered yet -- build first: cmake --build ${CMAKE_CURRENT_BINARY_DIR}\")\n")
endif()

set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${CTEST_TESTS_FILE}")
