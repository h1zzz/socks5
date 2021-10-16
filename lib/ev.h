/* ev.h */

#ifndef _PW_EV_H
#define _PW_EV_H

#include <sys/types.h>
#include <sys/time.h>
#include <stdint.h>
#include <stddef.h>

enum {
    EV_NONE = 0x00,
    EV_READ = 0x01,
    EV_WRITE = 0x02,
};

struct event_base;
struct ev_hash;

typedef void event_handler_t(struct event_base *, int, uint16_t, void *);

struct event {
    int fd;
    uint16_t flags;
    event_handler_t *fn;
    void *data;
    uint8_t count;
};

struct event_base {
    struct ev_hash *events;
    uint32_t events_size;
    uint32_t event_num;
    void *op;
};

struct event_base *event_base_new(uint32_t events_size);
int event_base_add(struct event_base *base, int fd, uint16_t flags,
                   event_handler_t *fn, void *data);
int event_base_delete(struct event_base *base, int fd, int flags);
/* error return -1, timeout return 0. */
int event_base_loop(struct event_base *base, const struct timeval *tv);
void event_base_destroy(struct event_base *base);

#endif /* ev.h */
