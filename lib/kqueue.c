/* kqueue.c */

#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "ev.h"
#include "debug.h"
#include "ev_hash.h"

struct kqueue_op {
    struct kevent *events;
    uint32_t events_size;
    int kq;
};

int op_init(struct event_base *base);
int op_add(struct event_base *base, int fd, uint16_t flags);
int op_delete(struct event_base *base, int fd, uint16_t flags);
int op_poll_wait(struct event_base *base, const struct timeval *tv);
int op_destroy(struct event_base *base);

int op_init(struct event_base *base)
{
    struct kqueue_op *op;

    op = calloc(1, sizeof(struct kqueue_op));
    if (!op) {
        pw_error("calloc");
        goto err;
    }

    op->kq = -1;

    op->events = calloc(base->events_size, sizeof(struct kevent));
    if (!op->events) {
        pw_error("calloc");
        goto err;
    }

    op->kq = kqueue();
    if (op->kq == -1) {
        pw_error("kqueue");
        goto err;
    }

    op->events_size = base->events_size;
    base->op = op;

    return 0;
err:
    if (op)
        op_destroy(base);
    return -1;
}

int op_add(struct event_base *base, int fd, uint16_t flags)
{
    struct kqueue_op *op = base->op;
    uint16_t mask = 0;
    struct kevent ke;

    if (flags & EV_READ)
        mask |= EVFILT_READ;

    if (flags & EV_WRITE)
        mask |= EVFILT_WRITE;

    EV_SET(&ke, fd, mask, EV_ADD | EV_ENABLE, 0, 0, NULL);

    if (kevent(op->kq, &ke, 1, NULL, 0, NULL) == -1) {
        pw_error("kevent");
        return -1;
    }

    return 0;
}

int op_delete(struct event_base *base, int fd, uint16_t flags)
{
    struct kqueue_op *op = base->op;
    uint16_t mask = 0;
    struct kevent ke;

    if (flags & EV_READ)
        mask |= EVFILT_READ;

    if (flags & EV_WRITE)
        mask |= EVFILT_WRITE;

    EV_SET(&ke, fd, mask, EV_DELETE | EV_DISABLE, 0, 0, NULL);

    if (kevent(op->kq, &ke, 1, NULL, 0, NULL) == -1) {
        pw_error("kevent");
        return -1;
    }

    return 0;
}

int op_poll_wait(struct event_base *base, const struct timeval *tv)
{
    struct kqueue_op *op = base->op;
    struct timespec timeout = {}, *tp = NULL;
    struct event *ev;
    int nev, i;
    uint16_t flags = 0;

    if (tv) {
        timeout.tv_sec = tv->tv_sec;
        timeout.tv_nsec = tv->tv_usec * 1000;
        tp = &timeout;
    }

again:
    nev = kevent(op->kq, NULL, 0, op->events, op->events_size, tp);
    if (nev == -1) {
        if (errno == EINTR)
            goto again;
        pw_error("kevent");
        return -1;
    }

    for (i = 0; i < nev; i++) {
        if (op->events[i].flags == EV_ERROR) {
            /* TODO */
            continue;
        }
        ev = ev_hash_get(base, op->events[i].ident);
        assert(ev);
        if (op->events[i].filter == EVFILT_READ) {
            flags |= EV_READ;
        }
        if (op->events[i].filter == EVFILT_WRITE) {
            flags |= EV_WRITE;
        }
        ev->fn(base, ev->fd, ev->flags, ev->data);
    }

    return nev;
}

int op_destroy(struct event_base *base)
{
    struct kqueue_op *op = base->op;

    if (op) {
        if (op->events)
            free(op->events);
        if (op->kq != -1)
            close(op->kq);
        free(op);
        base->op = NULL;
    }
    return 0;
}
