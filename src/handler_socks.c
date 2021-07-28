/* socks_handler.c */

#include "handler.h"

#include "socks.h"
#include "debug.h"
#include "misc.h"

static void
handler_socks_conn (struct event_base *base, int fd, u_int16_t flags,
                    void *data)
{
  struct socks_conn *c = data;
  int ret;

  switch (c->state)
    {
    case SOCKS_METHOD:
      ret = socks_get_method (c);
      if (ret == -1)
        {
          pw_debug ("socks get method failed\n");
          goto done;
        }
      break;
    case SOCKS_AUTH:
      ret = socks_authenticate (c);
      if (ret == -1)
        {
          pw_debug ("socks authenticate failed\n");
          goto done;
        }
      break;
    case SOCKS_CMD:
      ret = socks_command (c);
      if (ret == -1)
        {
          pw_debug ("socks handle command failed\n");
          goto done;
        }
      set_nonblocking (c->dstfd, 1);
      ret = event_base_add (base, c->dstfd, EV_READ, handler_socks_conn, c);
      if (ret == -1)
        goto done;
      break;
    case SOCKS_SERVE:
      ret = socks_serve (c, fd);
      if (ret == -1)
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

void
handler_socks (struct event_base *base, int fd, u_int16_t flags, void *data)
{
  struct socks_conn *c;

  c = socks_accept_conn (data);
  if (!c)
    {
      pw_debug ("accept new socks conn failed\n");
      return;
    }

  pw_debug ("new connection: %d\n", c->srcfd);

  set_nonblocking (c->srcfd, 1);

  if (event_base_add (base, c->srcfd, EV_READ, handler_socks_conn, c) == -1)
    {
      pw_debug ("event_base_add %d failed\n", c->srcfd);
      socks_close_conn (c);
      return;
    }
}
