
set(TARGET lib)

set(SOURCES
  ev_hash.c
  ev.c
  misc.c
  socks.c
)

set(HEADERS
  debug.h
  ev_hash.h
  ev.h
  misc.h
  socks.h
)

set(LIBS
)

if(WIN32)
# Windows, TODO
else()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux
    list(APPEND SOURCES epoll.c)
    add_definitions(-D_GNU_SOURCE)
  else()
    # Unix
    list(APPEND SOURCES kqueue.c)
  endif()
endif()

add_library(${TARGET} ${SOURCES} ${HEADERS})
target_link_libraries(${TARGET} ${LIBS})
