
set(SOURCES
  main.c
  handler_socks.c
  worker.c
)

set(HEADERS
  common.h
  handler.h
)

set(LIBS
)

if(WIN32)
# Windows, TODO
else()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux
    add_definitions(-D_GNU_SOURCE)
  else()
    # Unix
  endif()
endif()

list(APPEND LIBS lib)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} ${LIBS})
