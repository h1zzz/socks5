/* epoll.c */

#include <sys/epoll.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "ev.h"
#include "debug.h"
#include "ev_hash.h"

struct epoll_op
{
  struct epoll_event *events;
  uint32_t events_size;
  int epfd;
};

int op_init (struct event_base *base);
int op_add (struct event_base *base, int fd, uint16_t flags);
int op_delete (struct event_base *base, int fd, uint16_t flags);
int op_poll_wait (struct event_base *base, const struct timeval *tv);
int op_destroy (struct event_base *base);

int
op_init (struct event_base *base)
{
  struct epoll_op *op;

  op = calloc (1, sizeof (struct epoll_op));
  if (!op)
  {
    pw_error ("calloc");
    goto err;
  }

  op->epfd = -1;

  op->events = calloc (base->events_size, sizeof (struct epoll_event));
  if (!op->events)
  {
    pw_error ("calloc");
    goto err;
  }

  op->epfd = epoll_create (base->events_size);
  if (op->epfd == -1)
  {
    pw_error ("epoll_create");
    goto err;
  }

  op->events_size = base->events_size;
  base->op = op;

  return 0;
err:
  if (op)
    op_destroy (base);
  return -1;
}

int
op_add (struct event_base *base, int fd, uint16_t flags)
{
  struct epoll_op *op = base->op;
  struct epoll_event ee = {};

  ee.events = EPOLLET;
  ee.data.fd = fd;

  if (flags & EV_READ)
    ee.events |= EPOLLIN;

  if (flags & EV_WRITE)
    ee.events |= EPOLLOUT;

  /* TODO: handle modify */
  if (epoll_ctl (op->epfd, EPOLL_CTL_ADD, fd, &ee) == -1)
  {
    pw_error ("epoll_ctl");
    return -1;
  }

  return 0;
}

int
op_delete (struct event_base *base, int fd, uint16_t flags)
{
  struct epoll_op *op = base->op;
  struct epoll_event ee = {};

  ee.events = EPOLLET;
  ee.data.fd = fd;

  if (flags & EV_READ)
    ee.events |= EPOLLIN;

  if (flags & EV_WRITE)
    ee.events |= EPOLLOUT;

  if (epoll_ctl (op->epfd, EPOLL_CTL_DEL, fd, &ee) == -1)
  {
    pw_error ("epoll_ctl");
    return -1;
  }

  return 0;
}

int
op_poll_wait (struct event_base *base, const struct timeval *tv)
{
  struct epoll_op *op = base->op;
  int i, nfds;
  struct event *ev;
  int timeout = -1;

  if (tv)
    timeout = (tv->tv_sec * 1000 + (tv->tv_usec + 999) / 1000);

again:
  nfds = epoll_wait (op->epfd, op->events, op->events_size, timeout);
  if (nfds == -1)
  {
    if (errno == EINTR)
      goto again;
    pw_error ("epoll_wait");
    return -1;
  }

  for (i = 0; i < nfds; i++)
  {
    ev = ev_hash_get (base, op->events[i].data.fd);
    assert (ev);
    ev->fn (base, ev->fd, ev->flags, ev->data);
  }

  return nfds;
}

int
op_destroy (struct event_base *base)
{
  struct epoll_op *op = base->op;

  if (op)
  {
    if (op->events)
      free (op->events);
    if (op->epfd != -1)
      close (op->epfd);
    free (op);
    base->op = NULL;
  }
  return 0;
}
