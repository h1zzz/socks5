/* ev.c */

#include "ev.h"

#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "debug.h"
#include "ev_hash.h"

int op_init (struct event_base *base);
int op_add (struct event_base *base, int fd, uint16_t flags);
int op_delete (struct event_base *base, int fd, uint16_t flags);
int op_poll_wait (struct event_base *base, const struct timeval *tv);
int op_destroy (struct event_base *base);

struct event_base *
event_base_new (uint32_t events_size)
{
  struct event_base *base;

  base = calloc (1, sizeof (struct event_base));
  if (!base)
  {
    pw_error ("calloc");
    goto err;
  }

  base->event_num = 0;
  base->events_size = events_size;

  if (ev_hash_new (base) == -1)
    goto err;

  if (op_init (base) == -1)
    goto err;

  return base;
err:
  if (base)
    event_base_destroy (base);
  return NULL;
}

int
event_base_add (struct event_base *base, int fd, uint16_t flags,
                event_handler_t *fn, void *data)
{
  struct event *ev;

  if (!base || fd < 0 || flags == EV_NONE || !fn)
    return -1;

  ev = ev_hash_get (base, fd);
  if (ev)
  {
    ev->flags |= flags;
    if (op_add (base, fd, flags) == -1)
      return -1;
  }
  else
  {
    ev = calloc (1, sizeof (struct event));
    if (!ev)
    {
      pw_error ("calloc");
      return -1;
    }

    ev->fd = fd;
    ev->flags = flags;

    ev_hash_set (base, fd, ev);

    if (op_add (base, fd, flags) == -1)
    {
      ev_hash_delete (base, fd);
      return -1;
    }

    base->event_num++;
  }

  ev->fn = fn;
  ev->data = data;

  return 0;
}

int
event_base_delete (struct event_base *base, int fd, int flags)
{
  struct event *ev;

  if (!base || fd < 0 || flags == EV_NONE)
    return -1;

  ev = ev_hash_get (base, fd);
  if (!ev)
    return -1;

  ev->flags &= ~flags;

  if (op_delete (base, fd, flags) == -1)
    return -1;

  if (ev->flags == EV_NONE)
    ev_hash_delete (base, fd);

  base->event_num--;

  return 0;
}

int
event_base_loop (struct event_base *base, const struct timeval *tv)
{
  if (!base)
    return -1;

  return op_poll_wait (base, tv);
}

void
event_base_destroy (struct event_base *base)
{
  if (base)
  {
    ev_hash_destroy (base);
    op_destroy (base);
    free (base);
  }
}
