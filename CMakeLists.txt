cmake_minimum_required(VERSION 3.20)
project(axle LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 20)

enable_testing()


# Directories
set(AXLE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(AXLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(AXLE_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(AXLE_EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)

set(AXLE_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(AXLE_GOOGLETEST_DIR ${AXLE_THIRD_PARTY_DIR}/googletest)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

option(ENABLE_MSAN "Enable Memory Sanitizer" OFF)
option(ENABLE_USAN "Enable Undefined Behavior Sanitizer" OFF)

if(ENABLE_MSAN)
    if(APPLE)
        add_compile_options(
            -fsanitize=address
            -fno-omit-frame-pointer
            -g
        )
        add_link_options(-fsanitize=address)
    else()
        add_compile_options( -fsanitize=memory
            -fno-omit-frame-pointer
            -g
        )
        add_link_options(-fsanitize=memory)
    endif()

    message("MSAN enabled")
elseif(ENABLE_UBSAN)
    add_compile_options(
        -fsanitize=undefined
        -g
    )
    add_link_options(-fsanitize=undefined)
    message("UBSAN enabled")
else()
    message("No sanitizers enabled")
endif()

# Dependencies
add_subdirectory(${AXLE_GOOGLETEST_DIR} EXCLUDE_FROM_ALL)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project version
file(READ VERSION AXLE_PROJECT_VERSION)
string(STRIP ${AXLE_PROJECT_VERSION} AXLE_PROJECT_VERSION)

# and commit
execute_process(COMMAND git describe --always --dirty
    OUTPUT_VARIABLE AXLE_GIT_COMMIT_HASH
    ERROR_VARIABLE AXLE_GIT_COMMIT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE AXLE_GIT_COMMIT_RESULT
)
if (NOT ${AXLE_GIT_COMMIT_RESULT} STREQUAL 0)
    message(WARNING "Could not retrieve git commit: " ${AXLE_GIT_COMMIT_ERROR})
    set(AXLE_GIT_COMMIT_HASH unknown)
endif()

message("axle version information: " ${AXLE_PROJECT_VERSION} " - " ${AXLE_GIT_COMMIT_HASH})
configure_file(${AXLE_SRC_DIR}/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/gen/version.cpp @ONLY)

# Source files
set(AXLE_SRC_LIST
    ${AXLE_SRC_DIR}/async_socket.cpp
    ${AXLE_SRC_DIR}/axle.cpp
    ${AXLE_SRC_DIR}/event.cpp
    ${AXLE_SRC_DIR}/fiber.cpp
    ${AXLE_SRC_DIR}/scheduler.cpp
    ${AXLE_SRC_DIR}/socket.cpp
    ${AXLE_SRC_DIR}/fiber_arm64.cpp
    ${AXLE_SRC_DIR}/asm/fiber_arm64.s
    ${CMAKE_CURRENT_BINARY_DIR}/gen/version.cpp
)

# Test files
set(AXLE_TEST_LIST
    ${AXLE_TEST_DIR}/axle_test.cpp
    ${AXLE_TEST_DIR}/event_test.cpp
    ${AXLE_TEST_DIR}/fiber_test.cpp
    ${AXLE_TEST_DIR}/status_test.cpp
)

add_library(axle-lib ${AXLE_SRC_LIST})
target_include_directories(axle-lib PUBLIC ${AXLE_INCLUDE_DIR} ${AXLE_SRC_DIR})
set_target_properties(axle-lib PROPERTIES OUTPUT_NAME axle)

add_executable(axle-tests ${AXLE_TEST_LIST})
target_include_directories(axle-tests PRIVATE ${AXLE_SRC_DIR})
target_link_libraries(axle-tests axle-lib gtest_main)

add_test(unit-tests axle-tests)

add_executable(echo_server ${AXLE_EXAMPLES_DIR}/echo_server/main.cpp)
target_link_libraries(echo_server axle-lib)

add_executable(async_echo_server ${AXLE_EXAMPLES_DIR}/async_echo_server/main.cpp)
target_link_libraries(async_echo_server axle-lib)

file(GLOB_RECURSE HDR_FILES "${AXLE_SRC_DIR}/*.h" "${AXLE_INCLUDE_DIR}/*.h")
add_custom_target(lint
  # COMMAND /usr/local/bin/clang-tidy -p "${CMAKE_BINARY_DIR}" --checks="-*,misc-include-cleaner" ${HDR_FILES}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/run-clang-tidy.py -allow-no-checks -p=${CMAKE_BINARY_DIR}
  COMMENT "Running clang-tidy..."
)
