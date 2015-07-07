set(SODIUM_SEARCH_PATHS
  /usr/local/
  /usr
  /opt
  )
find_path(SODIUM_INCLUDE_DIR sodium.h
  HINTS ${SODIUM_ROOT}
  PATH_SUFFIXES include
  PATHS ${SODIUM_SEARCH_PATHS}
  )
find_library(SODIUM_LIBRARY sodium
  HINTS ${SODIUM_ROOT}
  PATH_SUFFIXES lib64 lib bin
  PATHS ${SODIUM_SEARCH_PATHS}
  )
if(SODIUM_INCLUDE_DIR AND SODIUM_LIBRARY)
  set(SODIUM_FOUND TRUE)
endif()

if(SODIUM_FOUND)
  message(STATUS "Found libsodium: ${SODIUM_LIBRARY}")
else()
  message(WARNING "Could not find libsodium")
  set(SODIUM_LIBRARY "")
  set(SODIUM_INCLUDE_DIR "")
endif()