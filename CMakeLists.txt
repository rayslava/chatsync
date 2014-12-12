cmake_minimum_required(VERSION 2.8.4)
project(chatsync)

enable_language(C CXX)

set(CMAKE_MODULE_PATH
${CMAKE_MODULE_PATH}
${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

set(SOURCE_FILES
    src/channel.cpp
    src/config.cpp
    src/ircchannel.cpp
    )

include(ConfigSafeGuards)
find_package(GTest)
if (DEFINED GTEST_INCLUDE_DIRS)
include_directories(${GTEST_INCLUDE_DIRS})
endif()

set(STANDARD "-std=c++1y")

set(CMAKE_CXX_FLAGS         "-Wall -Wno-unknown-pragmas -Wno-sign-compare -Woverloaded-virtual -Wwrite-strings -Wno-unused ${STANDARD} -Wextra -pedantic -Werror")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3  ${STANDARD}")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -fwhole-program ${STANDARD}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage  ${STANDARD}")
set(CMAKE_CXX_LINK_FLAGS "-fprofile-arcs -ftest-coverage  ${STANDARD}")

add_executable(chatsync src/main.cpp ${SOURCE_FILES})

enable_testing()

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    )

#add_library(
#   example
#    src/main.cpp
#    )


add_executable(
    unit_tests
    test/main.cpp
    test/channel.cpp
    ${SOURCE_FILES}
    ${GTEST_SOURCES}
    )

target_link_libraries(
    unit_tests
    ${GTEST_BOTH_LIBRARIES}
    pthread
    )

include(CTest)
enable_testing()

add_test(unit ${PROJECT_BINARY_DIR}/unit_tests)
