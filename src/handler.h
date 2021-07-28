/* socks_handler.h */

#ifndef _SOCKS_HANDLER_H
#define _SOCKS_HANDLER_H

#include <stdint.h>

#include "ev.h"

void handler_socks (struct event_base *base, int fd, u_int16_t flags,
                    void *data);

#endif /* socks_handler.h */
