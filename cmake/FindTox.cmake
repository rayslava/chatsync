set(TOX_SEARCH_PATHS
  /usr/local/
  /usr
  /opt
  )
find_path(TOX_INCLUDE_DIR tox/tox.h
  HINTS ${TOX_ROOT}
  PATH_SUFFIXES include
  PATHS ${TOX_SEARCH_PATHS}
  )
find_library(TOX_LIBRARY toxcore
  HINTS ${TOX_ROOT}
  PATH_SUFFIXES lib64 lib bin
  PATHS ${TOX_SEARCH_PATHS}
  )
if(TOX_INCLUDE_DIR AND TOX_LIBRARY)
  set(TOX_FOUND TRUE)
endif()

if(TOX_FOUND)
  message(STATUS "Found libtoxcore: ${TOX_LIBRARY}")
else()
  message(WARNING "Could not find libtoxcore")
  set(TOX_LIBRARY "")
  set(TOX_INCLUDE_DIR "")
endif()