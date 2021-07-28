/* event_test.c */

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <netdb.h>

#include "ev.h"
#include "misc.h"
#include "debug.h"
#include "socks.h"

/**
 * curl --proxy socks5://admin:123456@127.0.0.1:1080 https://www.google.com
 * curl --proxy socks5h://admin:123456@127.0.0.1:1080 https://www.google.com
 */

static void accept_worker (struct event_base *base, int fd, uint16_t flags,
                           void *data);
static void read_worker (struct event_base *base, int fd, uint16_t flags,
                         void *data);

int
main (int argc, char *argv[])
{
  struct event_base *base;
  struct socks *s;
  struct timeval tv = {};
  int ret;

  s = socks_create ("0.0.0.0", 1080, "admin", "123456");

  set_nonblocking (s->fd, 1);

  base = event_base_new (4);
  event_base_add (base, s->fd, EV_READ, accept_worker, s);

  while (1)
    {
      tv.tv_sec = 10;
      tv.tv_usec = 0;
      ret = event_base_loop (base, &tv);
      pw_debug ("event_base_loop ret: %d\n", ret);
      pw_debug ("event number: %d\n", base->event_num);
    }
  event_base_delete (base, s->fd, EV_READ);
  event_base_destroy (base);

  return 0;
}

static void
accept_worker (struct event_base *base, int fd, uint16_t flags, void *data)
{
  struct socks_conn *c;

  c = socks_accept_conn (data);
  if (!c)
    return;

  pw_debug ("new socks conn: %s:%d\n", inet_ntoa (c->addr.sin_addr),
            ntohs (c->addr.sin_port));

  set_nonblocking (c->srcfd, 1);
  event_base_add (base, c->srcfd, EV_READ, read_worker, c);
}

static void
read_worker (struct event_base *base, int fd, uint16_t flags, void *data)
{
  struct socks_conn *c = data;

  switch (c->state)
    {
    case SOCKS_METHOD:
      if (socks_get_method (c) == -1)
        {
          pw_debug ("socks get method failed\n");
          goto done;
        }
      break;
    case SOCKS_AUTH:
      if (socks_authenticate (c) == -1)
        {
          pw_debug ("socks authenticate failed\n");
          goto done;
        }
      break;
    case SOCKS_CMD:
      if (socks_command (c) == -1)
        {
          pw_debug ("socks handle command failed\n");
          goto done;
        }
      set_nonblocking (c->dstfd, 1);
      if (event_base_add (base, c->dstfd, EV_READ, read_worker, c) == -1)
        goto done;
      break;
    case SOCKS_SERVE:
      if (socks_serve (c, fd) == -1)
        goto done;
      break;
    default:
      goto done;
    }

  return;
done:
  pw_debug ("close connection: %d\n", fd);
  event_base_delete (base, c->srcfd, EV_READ);
  event_base_delete (base, c->dstfd, EV_READ);
  socks_close_conn (c);
}
