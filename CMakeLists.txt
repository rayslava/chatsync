cmake_minimum_required(VERSION 3.2)
project(chatsync)
cmake_policy(SET CMP0049 OLD)

enable_language(C CXX)
include (CheckCSourceCompiles)
include (CheckCXXSourceCompiles)
include (CMakeDependentOption)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  )

find_package (Threads REQUIRED)

# Optional plugins
option (FEATURE_TOX "Build Tox support" ON)

# Optional plugins
option (TLS_SUPPORT "Build TLS connection support (using gnutlsxx)" ON)
option (FEATURE_TELEGRAM "Build Telegram chat support" ON)

if(FEATURE_TOX)
  option (USE_C_TOXCORE "Build Tox support using c-toxcore from TokTok" OFF)
endif()


set(SOURCE_FILES
  src/config.cpp
  src/channel.cpp
  src/ircchannel.cpp
  src/filechannel.cpp
  src/hub.cpp
  src/logging.cpp
  src/http.cpp
  )

macro(create_test name files)
  message(STATUS "Creating test '${name}' of ${files}")

  if (NOT (${name} STREQUAL "unit"))
    # Collecting names for globas "unit_tests" binary
    list(APPEND all_test_files
      "${files}")
  endif()

  set(${name}_TEST_FILES
    test/main.cpp
    ${files}
    )

  set(test_files
    "\${${name}_TEST_FILES}"
    )

  add_executable(
    ${name}_tests
    ${SOURCE_FILES}
    ${GTEST_SOURCES}
    ${GMOCK_SOURCES}
    ${test_files}
    )

  target_link_libraries(
    ${name}_tests
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_BOTH_LIBRARIES}
    ${GNUTLSXX_LIBRARY}
    ${GNUTLS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TOX_LIBRARY}
    ${SODIUM_LIBRARY}
    ${STATIC_LIBRT}
    )

  add_test(${name} ${name}_tests)
endmacro(create_test)

if (FEATURE_TOX)
  message (STATUS "Compiling with Tox support")
  find_package (Tox REQUIRED)
  find_package (Sodium REQUIRED)
  CHECK_C_SOURCE_COMPILES(
    "#include \"tox/tox.h\"
     #if !(TOX_VERSION_IS_API_COMPATIBLE(0,1,5))
     #error \"Please use c-toxcore\"
     #endif
     int main() {return 0;}
     " C_TOXCORE)
  if (C_TOXCORE)
    message (STATUS "c-toxcore found will use it for Tox support")
    add_definitions(-DCTOXCORE)
  endif()
  if (USE_C_TOXCORE AND NOT C_TOXCORE)
     message(FATAL_ERROR "Could not find c-toxcore")
  endif()
  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/toxchannel.cpp
    )
endif()

if (TLS_SUPPORT)
  message (STATUS "Compiling with TLS support")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GNUTLS REQUIRED gnutls>=3.5.12)
  CHECK_CXX_SOURCE_COMPILES(
    "#include \"gnutls/gnutlsxx.h\"
     int main() {return 0;}
     " GNUTLSXX)
  if (NOT GNUTLSXX)
    message(FATAL_ERROR "Can't find gnutlsxx")
  endif()
  add_definitions(-DTLS_SUPPORT)
  set(GNUTLSXX_LIBRARY "gnutlsxx")
endif()

if (FEATURE_TELEGRAM)
  if (NOT TLS_SUPPORT)
    message(FATAL_ERROR "Telegram support requires TLS")
  endif()
  message (STATUS "Compiling with Telegram support")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RAPIDJSON REQUIRED RapidJSON>=1.1.0)
  add_definitions(-DTELEGRAM)
  set(SOURCE_FILES ${SOURCE_FILES} src/telegram.cpp)
endif()

include(ConfigSafeGuards)

find_package(GTest)
if (DEFINED GTEST_INCLUDE_DIRS)
  include_directories(${GTEST_INCLUDE_DIRS})
endif()

find_package(GMock)
if (DEFINED GMOCK_INCLUDE_DIRS)
  include_directories(${GMOCK_INCLUDE_DIRS})
endif()

if (TRAVIS_BUILD)
  add_definitions(-DTRAVIS_BUILD)
endif()

set(STANDARD "-std=c++14")

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -fvisibility=hidden -Wall -Wno-unknown-pragmas -Wno-sign-compare -Woverloaded-virtual -Wwrite-strings -Wno-unused ${STANDARD} -Wextra -pedantic -Werror -pthread")
set(CMAKE_CXX_FLAGS_PROFILED "${CMAKE_CXX_FLAGS} -pg")

if (STATIC)
  set(BUILD_SHARED_LIBS OFF)
  set(GNUTLS_LIBRARIES ${GNUTLS_LIBRARIES_STATIC})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -fPIC -fPIE")
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
  endif()
endif()
if (SANITIZED)
  set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 ${STANDARD}")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast ${STANDARD}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unknown-warning-option -Wno-error=braced-scalar-init")
endif()

if ((COVERALLS OR COVERAGE) AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "will generate coverage data")
  include(CodeCoverage)
  include(Coveralls)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -O0 -Wall -W -fprofile-arcs -ftest-coverage --coverage")
  set(CMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov")
endif()

add_executable(chatsync src/main.cpp ${SOURCE_FILES})

if (STATIC)
  SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  SET (STATIC_LIBRT "-lrt")
  set_target_properties(chatsync PROPERTIES LINK_SEARCH_START_STATIC 1)
  set_target_properties(chatsync PROPERTIES LINK_SEARCH_END_STATIC 1)
endif()

enable_testing()

include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${TOX_INCLUDE_DIR}
  )

target_link_libraries(
  chatsync
  ${CMAKE_THREAD_LIBS_INIT}
  ${GNUTLSXX_LIBRARY}
  ${GNUTLS_LIBRARIES}
  ${TOX_LIBRARY}
  ${SODIUM_LIBRARY}
  ${STATIC_LIBRT}
  )

include(CTest)
enable_testing()

create_test(channel test/channel.cpp)

create_test(hub test/hub.cpp)

create_test(config test/config.cpp)

create_test(logging test/logging.cpp)

create_test(http test/http.cpp)

if (FEATURE_TOX)
  create_test(tox test/tox.cpp)
endif()

if (FEATURE_TELEGRAM)
  create_test(telegram test/telegram.cpp)
endif()

create_test(unit "${all_test_files}")

if (COVERAGE)
  setup_target_for_coverage(coverage unit_tests . coverage)
endif()

if (COVERALLS)
  setup_target_for_coveralls(coveralls unit_tests src)
endif()
