/* test_event.c */

#include <sys/socket.h>
#include <sys/event.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "util.h"
#include "misc.h"

static char str[] = "hello world";

static void
register_fd (int kq, int fd)
{
  struct kevent ev;

  EV_SET (&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, str);

  if (kevent (kq, &ev, 1, NULL, 0, NULL) == -1)
    {
      perror ("kevent");
    }
}

static void
unregister_fd (int kq, int fd)
{
  struct kevent ev;

  EV_SET (&ev, fd, EVFILT_READ, EV_DELETE | EV_DISABLE, 0, 0, NULL);

  if (kevent (kq, &ev, 1, NULL, 0, NULL) == -1)
    {
      perror ("kevent");
    }
}

static void
serve (int kq, int fd)
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
      unregister_fd (kq, fd);
      close (fd);
      fprintf (stdout, "close %d\n", fd);
      return;
    }
  fprintf (stdout, "fd: %d, %d, %s", fd, n, (char *)buffer);
}

static void
handle_accept (int kq, int fd)
{
  struct sockaddr_in si = {};
  socklen_t len = sizeof (si);
  int conn;

  conn = accept (fd, (struct sockaddr *)&si, &len);
  set_nonblocking (conn, 1);
  fprintf (stdout, "accept fd: %d\n", conn);
  register_fd (kq, conn);
}

int
main (int argc, char *argv[])
{
  struct kevent *events;
  int fd, kq, nev, i, conn;

  events = (struct kevent *)calloc (MAX_CONN, sizeof (struct kevent));

  fd = create_listen (1080);
  if (fd == -1)
    perror ("create_listen");

  set_nonblocking (fd, 1);

  /* fprintf(stdout, "fd: %d\n", fd); */

  kq = kqueue ();
  if (kq == -1)
    perror ("kevent");

  register_fd (kq, fd);

  while (1)
    {
      nev = kevent (kq, NULL, 0, events, MAX_CONN, NULL);
      if (nev == -1)
        perror ("kevent");
      /* fprintf(stdout, "nev: %d\n", nev); */
      for (i = 0; i < nev; i++)
        {
          /* fprintf(stdout, "sock: %d, data: %d\n", sock, data); */
          fprintf (stdout, "udata: %s\n", (char *)events[i].udata);
          if (events[i].ident == fd)
            {
              /* listen fd */
              fprintf (stdout, "listen fd accept\n");
              handle_accept (kq, events[i].ident);
            }
          else
            {
              switch (events[i].filter)
                {
                case EVFILT_READ:
                  serve (kq, events[i].ident);
                  break;
                case EVFILT_WRITE:
                  break;
                default:
                  fprintf (stdout, "unknown event\n");
                  exit (-1);
                }
            }
        }
    }

  return 0;
}
