/* common.h */

#ifndef _COMMON_H
#define _COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ev.h"

struct g_option {
    uint32_t worker_processes;
    uint32_t worker_connections;
    int is_daemon;
    const char *user;
    const char *passwd;
    const char *host;
    uint16_t port;
};

extern struct g_option g_opt; /* definition main.c */

void worker_listen_add(int fd, uint16_t flags, event_handler_t *fn, void *data);
void worker_listen_delete(int fd);

#endif /* common.h */
