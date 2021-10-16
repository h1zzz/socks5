/* socks.h */

#ifndef _PW_SOCKS_H
#define _PW_SOCKS_H

#include <arpa/inet.h>
#include <stdint.h>

#define SOCKS_VER 5

enum {
    SOCKS_METHOD = 0x01,
    SOCKS_AUTH = 0x02,
    SOCKS_CMD = 0x03,
    SOCKS_SERVE = 0x04,
};

enum {
    SOCKS_CONNECT = 0x01,
    SOCKS_BIND = 0x02,
    SOCKS_UDP_ASSOCIATE_UDP = 0x03,
};

enum {
    SOCKS_IPv4 = 0x01,
    SOCKS_DOMAIN = 0x03,
    SOCKS_IPv6 = 0x04,
};

struct socks {
    char user[256];
    char passwd[256];
    uint8_t use_auth;
    int fd;
};

struct socks_conn {
    struct socks *socks;
    uint8_t state;
    struct sockaddr_in addr;
    socklen_t addrlen;
    int srcfd;
    int dstfd;
    uint8_t method;
    uint8_t address_type;
    uint8_t command;
};

struct socks *socks_create(const char *host, uint16_t port, const char *u,
                           const char *p);
void socks_close(struct socks *s);
struct socks_conn *socks_accept_conn(struct socks *s);
void socks_close_conn(struct socks_conn *c);
int socks_get_method(struct socks_conn *c);
int socks_authenticate(struct socks_conn *c);
int socks_command(struct socks_conn *c);
int socks_serve(struct socks_conn *c, int fd);

#endif /* socks.h */
