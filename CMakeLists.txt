cmake_minimum_required(VERSION 2.8.12)
project(chatsync)

enable_language(C CXX)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  )

find_package (Threads REQUIRED)

# Optional plugins
option (FEATURE_TOX "Build Tox support" ON)

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

if (FEATURE_TOX)
  message ("Compiling with Tox support")
  find_package (Tox REQUIRED)
  find_package (Sodium REQUIRED)
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

find_package(GTest)
if (DEFINED GTEST_INCLUDE_DIRS)
  include_directories(${GTEST_INCLUDE_DIRS})
endif()

if (TRAVIS_BUILD)
  add_definitions(-DTRAVIS_BUILD)
endif()

set(STANDARD "-std=c++14")

set(CMAKE_CXX_FLAGS         "-Wall -Wno-unknown-pragmas -Wno-sign-compare -Woverloaded-virtual -Wwrite-strings -Wno-unused ${STANDARD} -Wextra -pedantic -Werror -pthread")
set(CMAKE_CXX_FLAGS_PROFILED "${CMAKE_CXX_FLAGS} -pg")

if (STATIC)
  set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -fPIC -fPIE")
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
  endif()
endif()
if (SANITIZED)
  set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -ggdb ${STANDARD}")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast ${STANDARD}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unknown-warning-option -Wno-error=braced-scalar-init")
endif()

if ((COVERALLS OR COVERAGE) AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "will generate coverage data")
  include(CodeCoverage)
  include(Coveralls)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -W -fprofile-arcs -ftest-coverage --coverage")
  set(CMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  if (COVERAGE)
    setup_target_for_coverage(coverage unit_tests src coverage)
  endif()
  if (COVERALLS)
    setup_target_for_coveralls(coveralls unit_tests src)
  endif()
endif()

if (STATIC)
  SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  SET (STATIC_LIBRT "-lrt")
endif()

add_executable(chatsync src/main.cpp ${SOURCE_FILES})

enable_testing()

include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${TOX_INCLUDE_DIR}
  )

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
  ${SODIUM_LIBRARY}
  ${STATIC_LIBRT}
  )

target_link_libraries(
  chatsync
  ${CMAKE_THREAD_LIBS_INIT}
  ${TOX_LIBRARY}
  ${SODIUM_LIBRARY}
  ${STATIC_LIBRT}
  )

include(CTest)
enable_testing()

add_test(unit unit_tests)
