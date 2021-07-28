/* ev_hash.h */

#ifndef _PW_EV_HASH_H
#define _PW_EV_HASH_H

#include <stdint.h>

struct event;
struct event_base;

int ev_hash_new (struct event_base *base);
struct event *ev_hash_get (struct event_base *base, int fd);
int ev_hash_set (struct event_base *base, int fd, struct event *ev);
int ev_hash_delete (struct event_base *base, int fd);
void ev_hash_destroy (struct event_base *base);

#endif /* ev_hash.h */
