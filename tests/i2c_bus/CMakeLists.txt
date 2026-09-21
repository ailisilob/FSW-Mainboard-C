# Host-side unit tests for src/board/i2c_bus.c.
#
# This is a standalone CMake project: it deliberately does NOT pull in the Pico
# SDK, so it builds and runs with a plain host compiler and no cross toolchain.
# The SDK headers i2c_bus.c includes are satisfied by the fakes in fakes/.
#
#   cmake -S tests/i2c_bus -B build/tests
#   cmake --build build/tests
#   ctest --test-dir build/tests --output-on-failure
#
# (build/ is already gitignored, so build/tests keeps the tree clean.)
#
# Each Unity test is registered as its own CTest entry, so `ctest` reports them
# individually and you can run one with
#   ctest --test-dir build/tests -R test_bus_init_stores_timeout
#
# Requires the Unity submodule, which lands at <repo>/unity:
#   git submodule update --init --recursive
# Generating the test runner also needs Ruby (macOS ships /usr/bin/ruby).

cmake_minimum_required(VERSION 3.16)  # 3.16 for SKIP_REGULAR_EXPRESSION
project(i2c_bus_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(UNITY_DIR "${REPO_ROOT}/unity")

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

# Build Unity directly rather than add_subdirectory(): Unity's own CMakeLists
# compiles with -Werror and a very wide warning set, which we do not want to
# inherit here. Guard the target in case a parent project already added it.
if(NOT TARGET unity)
    add_library(unity STATIC "${UNITY_DIR}/src/unity.c")
    target_include_directories(unity PUBLIC "${UNITY_DIR}/src")
    # Compiles UnityParseOptions, which gives the generated runner its -l and
    # -n flags. Test discovery below depends on both.
    target_compile_definitions(unity PUBLIC UNITY_USE_COMMAND_LINE_ARGS)
endif()

# The fake Pico SDK. `fakes/` comes first on the include path so that
# "hardware/i2c.h", "pico/sync.h" and friends resolve to the stubs.
add_library(pico_fakes STATIC fakes/pico_fake.c)
target_include_directories(pico_fakes PUBLIC "${CMAKE_CURRENT_LIST_DIR}/fakes")

# The code under test, compiled for the host against the fakes.
add_library(i2c_bus_uut STATIC "${REPO_ROOT}/src/board/i2c_bus.c")
target_include_directories(i2c_bus_uut PUBLIC "${REPO_ROOT}/include")
target_link_libraries(i2c_bus_uut PUBLIC pico_fakes)
# Warnings on, but not fatal: i2c_bus.c currently has real warnings that these
# tests are meant to surface, and -Werror would hide them behind a build break.
target_compile_options(i2c_bus_uut PRIVATE -Wall -Wextra -Wno-unused-parameter)

# ---------------------------------------------------------------------------
# Test runner generation
#
# Unity's Ruby generator scans test_i2c_bus.c for functions named test_* and
# emits a runner holding main() and the RUN_TEST calls. That means the RUN_TEST
# list is never hand-maintained: write a `void test_foo(void)` and it runs.
#
# The generator's regex only matches functions declared `^void test_...`, so
# test functions must not be `static`. Helpers in the test file still can be.
# ---------------------------------------------------------------------------
set(TEST_SOURCE "${CMAKE_CURRENT_LIST_DIR}/test_i2c_bus.c")
set(TEST_RUNNER "${CMAKE_CURRENT_BINARY_DIR}/test_i2c_bus_Runner.c")
set(RUNNER_GENERATOR "${UNITY_DIR}/auto/generate_test_runner.rb")

add_custom_command(
    OUTPUT  "${TEST_RUNNER}"
    COMMAND "${RUBY_EXECUTABLE}" "${RUNNER_GENERATOR}"
            --cmdline_args=1
            "${TEST_SOURCE}" "${TEST_RUNNER}"
    DEPENDS "${TEST_SOURCE}" "${RUNNER_GENERATOR}"
    COMMENT "Generating Unity test runner for test_i2c_bus.c"
    VERBATIM
)

add_executable(test_i2c_bus "${TEST_SOURCE}" "${TEST_RUNNER}")
target_link_libraries(test_i2c_bus PRIVATE i2c_bus_uut unity)
target_compile_options(test_i2c_bus PRIVATE -Wall -Wextra)

# ---------------------------------------------------------------------------
# Test discovery
#
# After every link, ask the runner which tests it contains and write one
# add_test() per test. Doing this at build time rather than configure time
# keeps the CTest list exactly in step with the runner.
# ---------------------------------------------------------------------------
enable_testing()

set(CTEST_TESTS_FILE "${CMAKE_CURRENT_BINARY_DIR}/test_i2c_bus_tests.cmake")

add_custom_command(
    TARGET test_i2c_bus POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            -D "TEST_EXECUTABLE=$<TARGET_FILE:test_i2c_bus>"
            -D "TEST_PREFIX=i2c_bus."
            -D "CTEST_FILE=${CTEST_TESTS_FILE}"
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/DiscoverUnityTests.cmake"
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
