/* event_test.c */

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "ev.h"
#include "misc.h"
#include "debug.h"

static void accept_worker (struct event_base *base, int fd, uint16_t flags,
                           void *data);
static void read_worker (struct event_base *base, int fd, uint16_t flags,
                         void *data);

int
main (int argc, char *argv[])
{
  struct event_base *base;
  struct timeval tv = {};
  int fd, ret;

  fd = create_listen (1080);
  if (fd == -1)
    abort ();

  set_nonblocking (fd, 1);

  base = event_base_new (4);
  event_base_add (base, fd, EV_READ, accept_worker, NULL);

  while (1)
    {
      tv.tv_sec = 10;
      tv.tv_usec = 0;
      ret = event_base_loop (base, &tv);
      pw_debug ("ret: %d\n", ret);
    }

  event_base_destroy (base);

  return 0;
}

static void
accept_worker (struct event_base *base, int fd, uint16_t flags, void *data)
{
  int conn;

  conn = accept (fd, NULL, NULL);
  if (conn == -1)
    {
      pw_error ("accept");
      return;
    }

  putf ("recv new connection: %d\n", conn);

  set_nonblocking (conn, 1);

  event_base_add (base, conn, EV_READ, read_worker, NULL);
}

static void
read_worker (struct event_base *base, int fd, uint16_t flags, void *data)
{
  char buf[BUFSIZ] = {};
  int n;

  n = read (fd, buf, sizeof (buf));
  if (n == -1)
    {
      pw_error ("read");
      return;
    }

  if (n == 0)
    {
      putf ("close connection: %d\n", fd);
      event_base_delete (base, fd, flags);
      close (fd);
      return;
    }

  putf ("recv %d: %s", fd, buf);

  putf ("send %d: hello world!!!\n", fd);
  write (fd, "hello world!!!\n", 16);
}
