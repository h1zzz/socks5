/* epoll_test.c */

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "misc.h"

static void
register_fd (int epfd, int fd)
{
  struct epoll_event ee = {};

  ee.events = EPOLLIN | EPOLLET;
  ee.data.fd = fd;

  if (epoll_ctl (epfd, EPOLL_CTL_ADD, fd, &ee) == -1)
    {
      perror ("epoll_ctl");
      return;
    }
}

static void
unregister_fd (int epfd, int fd)
{
  struct epoll_event ee = {};

  ee.events = EPOLLIN | EPOLLET;
  ee.data.fd = fd;

  if (epoll_ctl (epfd, EPOLL_CTL_DEL, fd, &ee) == -1)
    {
      perror ("epoll_ctl");
      return;
    }
}

static void
serve (int epfd, int fd)
{
  unsigned char buffer[2048] = { 0 };

  int n = read (fd, buffer, sizeof (buffer));
  if (n == -1)
    {
      perror ("read");
      return;
    }
  if (n == 0)
    {
      unregister_fd (epfd, fd);
      close (fd);
      fprintf (stdout, "close %d\n", fd);
      return;
    }
  fprintf (stdout, "fd: %d, %d, %s", fd, n, (char *)buffer);
}

static void
handle_accept (int epfd, int fd)
{
  struct sockaddr_in si = {};
  socklen_t len = sizeof (si);
  int conn;

  conn = accept (fd, (struct sockaddr *)&si, &len);
  set_nonblocking (conn, 1);
  fprintf (stdout, "accept fd: %d\n", conn);
  register_fd (epfd, conn);
}

int
main (int argc, char *argv[])
{
  int fd, epfd, nfds, i;
  struct epoll_event *events;

  fd = create_listen (1080);

  set_nonblocking (fd, 1);

  events
      = (struct epoll_event *)calloc (MAX_CONN, sizeof (struct epoll_event));

  epfd = epoll_create (MAX_CONN);
  if (epfd == -1)
    {
      perror ("epoll_create");
      return -1;
    }

  register_fd (epfd, fd);

  while (1)
    {
      nfds = epoll_wait (epfd, events, MAX_CONN, -1);
      if (nfds == -1)
        {
          perror ("epoll_wait");
          return -1;
        }
      for (i = 0; i < nfds; i++)
        {
          if (events[i].data.fd == fd)
            {
              /* listen fd */
              fprintf (stdout, "listen fd: %d\n", events[i].data.fd);
              handle_accept (epfd, events[i].data.fd);
              continue;
            }
          serve (epfd, events[i].data.fd);
        }
    }

  return 0;
}
