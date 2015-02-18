cmake_minimum_required(VERSION 2.8.4)
project(chatsync)

enable_language(C CXX)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  )

find_package(Tox)

set(SOURCE_FILES
  src/config.cpp
  src/channel.cpp
  src/ircchannel.cpp
  src/filechannel.cpp
  src/hub.cpp
  )

set(TEST_FILES
  test/main.cpp
  test/channel.cpp
  test/hub.cpp
  test/config.cpp
  )

if (DEFINED TOX_FOUND)
  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/toxchannel.cpp
    )
  set(TEST_FILES
    ${TEST_FILES}
    test/tox.cpp
    )
endif()

include(ConfigSafeGuards)
include(CodeCoverage)
include(Coveralls)

find_package(GTest)
if (DEFINED GTEST_INCLUDE_DIRS)
  include_directories(${GTEST_INCLUDE_DIRS})
endif()

find_package (Threads REQUIRED)

set(STANDARD "-std=c++1y")

set(CMAKE_CXX_FLAGS         "-Wall -Wno-unknown-pragmas -Wno-sign-compare -Woverloaded-virtual -Wwrite-strings -Wno-unused ${STANDARD} -Wextra -pedantic -Werror")

set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -ggdb ${STANDARD}")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast ${STANDARD}")
set(CMAKE_CXX_FLAGS_PROFILED "${CMAKE_CXX_FLAGS_RELEASE} -pg")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unknown-warning-option -Wno-error=braced-scalar-init")
endif()


if ((COVERALLS OR COVERAGE) AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "will generate coverage data")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -W -fprofile-arcs -ftest-coverage --coverage")
  SET(CMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  if (COVERAGE)
    setup_target_for_coverage(coverage unit_tests src coverage)
  endif()
  if (COVERALLS)
    setup_target_for_coveralls(coveralls unit_tests src)
  endif()
endif()

add_executable(chatsync src/main.cpp ${SOURCE_FILES})

enable_testing()

include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${TOX_INCLUDE_DIR}
  )

#add_library(
#   example
#    src/main.cpp
#    )


add_executable(
  unit_tests
  ${SOURCE_FILES}
  ${GTEST_SOURCES}
  ${TEST_FILES}
  )

target_link_libraries(
  unit_tests
  ${GTEST_BOTH_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${TOX_LIBRARY}
  )

target_link_libraries(
  chatsync
  ${CMAKE_THREAD_LIBS_INIT}
  ${TOX_LIBRARY}
  )

include(CTest)
enable_testing()

add_test(unit unit_tests)
