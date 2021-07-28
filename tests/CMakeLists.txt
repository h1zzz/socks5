
set(LIBS lib)

include_directories(
    ${PROJECT_BINARY_DIR}
    ../lib
)

if(WIN32)
# Windows
else()
  set(UNIX_COMMON_HEADERS
    util.h
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux
    set(EPOLL_TEST_SOURCES
      epoll_test.c
    )
    add_executable(epoll_test ${EPOLL_TEST_SOURCES} ${UNIX_COMMON_HEADERS})
    target_link_libraries(epoll_test ${LIBS})
  else()
    # Unix
    set(KQUEUE_TEST_SOURCES
      kqueue_test.c
    )
    add_executable(kqueue_test ${KQUEUE_TEST_SOURCES} ${UNIX_COMMON_HEADERS})
    target_link_libraries(kqueue_test ${LIBS})
  endif()
endif()

add_executable(event_test event_test.c)
target_link_libraries(event_test ${LIBS})

add_executable(socks_test socks_test.c)
target_link_libraries(socks_test ${LIBS})

add_executable(my_test my_test.c)
target_link_libraries(my_test ${LIBS})
