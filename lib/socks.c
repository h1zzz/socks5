/* socks.c */

#include "socks.h"

#include <sys/socket.h>
#include <sys/select.h> /* connect timeout */
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>

#include "debug.h"

/**
 * X'00' succeeded
 * X'01' general SOCKS server failure
 * X'02' connection not allowed by ruleset
 * X'03' Network unreachable
 * X'04' Host unreachable
 * X'05' Connection refused
 * X'06' TTL expired
 * X'07' Command not supported
 * X'08' Address type not supported
 * X'09' to X'FF' unassigned
 */
enum {
    SOCKS_SUCCEEDED = 0x00,
    SOCKS_FAILURE = 0x01,
    SOCKS_CONNECTION_NOT_ALLOWED_BY_RULESET = 0x02,
    SOCKS_NETWORK_UNREACHABLE = 0x03,
    SOCKS_HOST_UNREACHABLE = 0x04,
    SOCKS_CONNECTION_REFUSED = 0x05,
    SOCKS_TTL_EXPIRED = 0x06,
    SOCKS_COMMAND_NOT_SUPPORTED = 0x07,
    SOCKS_ADDRESS_TYPE_NOT_SUPPORTED = 0x08,
};

static int socks_replies(struct socks_conn *c, uint8_t rep, uint8_t atyp,
                         const char *addr, int addrlen, uint16_t port);
static int socks_ip_connect(struct socks_conn *c, in_addr_t addr,
                            in_port_t port);
static int socks_domain_connect(struct socks_conn *c, const char *domain,
                                in_port_t port);

struct socks *socks_create(const char *host, uint16_t port, const char *u,
                           const char *p)
{
    struct sockaddr_in si = {};
    struct socks *s;
#ifndef NDEBUG
    int opt;
#endif

    s = calloc(1, sizeof(struct socks));
    if (!s) {
        pw_error("calloc");
        goto err;
    }

    if (u && p) {
        memcpy(s->user, u, strlen(u));
        memcpy(s->passwd, p, strlen(p));
        s->use_auth = 1;
    }

    s->fd = socket(PF_INET, SOCK_STREAM, 0);
    if (s->fd == -1) {
        pw_error("socket");
        goto err;
    }

    si.sin_addr.s_addr = inet_addr(host);
    si.sin_port = htons(port);
    si.sin_family = AF_INET;

#ifndef NDEBUG
    opt = 1;
    setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt,
               sizeof(opt));
#endif

    if (bind(s->fd, (struct sockaddr *)&si, sizeof(si)) == -1) {
        pw_error("bind");
        goto err;
    }

    if (listen(s->fd, SOMAXCONN) == -1) {
        pw_error("listen");
        goto err;
    }

    return s;
err:
    if (s)
        socks_close(s);
    return NULL;
}

void socks_close(struct socks *s)
{
    if (s) {
        if (s->fd != -1)
            close(s->fd);
        free(s);
    }
}

struct socks_conn *socks_accept_conn(struct socks *s)
{
    struct socks_conn *c;

    c = calloc(1, sizeof(struct socks_conn));
    if (!c) {
        pw_error("calloc");
        return NULL;
    }

    c->socks = s;
    c->state = SOCKS_METHOD;
    c->dstfd = -1;
    c->addrlen = sizeof(c->addr);

    c->srcfd = accept(s->fd, (struct sockaddr *)&c->addr, &c->addrlen);
    if (c->srcfd == -1) {
        if (errno != EINTR) {
            pw_error("accept");
        }
        return NULL;
    }

    return c;
}

void socks_close_conn(struct socks_conn *c)
{
    if (c) {
        if (c->dstfd != -1)
            close(c->dstfd);
        if (c->srcfd != -1)
            close(c->srcfd);
        free(c);
    }
}

int socks_get_method(struct socks_conn *c)
{
    char *methods, buf[512] = {};
    int n, i;
    uint8_t n_methods;

    n = read(c->srcfd, buf, sizeof(buf));
    if (n <= 0) {
        if (n == -1) {
            pw_error("read");
        }
        return -1;
    }

    /* pw_debug("version: %d, n_methods: %d\n", buf[0], buf[1]); */

    if (buf[0] != SOCKS_VER) {
        pw_debug("Version %d is not supported\n", buf[0]);
        return -1;
    }

    n_methods = buf[1];
    methods = buf + 2;

    for (i = 0; i < n_methods; i++) {
        if (methods[i] == 0x00 && !c->socks->use_auth) {
            c->method = 0x00; /* X'00' NO AUTHENTICATION AUTHENTICATION */
            break;
        } else if (methods[i] == 0x02 && c->socks->use_auth) {
            c->method = 0x02; /* X'02' USERNAME/PASSWORD */
            break;
        } else {
            /**
             * X'01' GSSAPI
             * X'03' to X'7F' IANA ASSIGNED
             * X'80' to X'FE' RESERVED FOR PRIVATE METHODS
             * X'FF' NO ACCEPTABLE METHODS
             */
            c->method = 0xff; /* NO ACCEPTABLE METHODS */
        }
    }

    buf[0] = SOCKS_VER;
    buf[1] = c->method;

    if (write(c->srcfd, buf, 2) == -1) {
        pw_error("write");
        return -1;
    }

    if (c->method == 0xff) { /* NO ACCEPTABLE METHODS */
        pw_debug("Unsupported authentication method\n");
        return -1;
    }

    c->state = SOCKS_AUTH;

    return 0;
}

int socks_authenticate(struct socks_conn *c)
{
    uint8_t ver, ulen, plen;
    char buf[512] = {};
    char *u, *p;
    int n, i;

    n = read(c->srcfd, buf, sizeof(buf));
    if (n <= 0) {
        if (n == -1) {
            pw_error("read");
        }
        return -1;
    }

    ver = buf[0];
    ulen = buf[1];
    plen = buf[2 + ulen];

    u = buf + 2;
    p = buf + 3 + ulen;

    buf[1] = 1;

    if (memcmp(c->socks->user, u, strlen(c->socks->user)) == 0 &&
        memcmp(c->socks->passwd, p, strlen(c->socks->passwd)) == 0)
        buf[1] = 0;

    if (write(c->srcfd, buf, 2) == -1) {
        pw_error("write");
        return -1;
    }

    if (buf[1] != 0)
        return -1;

    c->state = SOCKS_CMD;

    return 0;
}

int socks_command(struct socks_conn *c)
{
    /**
     * +----+-----+-------+------+----------+----------+
     * |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
     * +----+-----+-------+------+----------+----------+
     * | 1  |  1  | X'00' |  1   | Variable |    2     |
     * +----+-----+-------+------+----------+----------+
     */
    char buf[512] = {};
    struct {
        uint8_t ver;
        uint8_t cmd;
        uint8_t rsv;
        uint8_t atyp;
    } *hdr = (void *)buf;
    int n, ret;
    char domain[256] = {};
    uint8_t domain_len;
    struct in_addr addr = {};
    in_port_t port;

    n = read(c->srcfd, buf, sizeof(buf));
    if (n <= 0) {
        if (n == -1) {
            pw_error("read");
        }
        return -1;
    }

    /* verify version */
    if (hdr->ver != SOCKS_VER)
        return -1;

    c->command = hdr->cmd;
    c->address_type = hdr->atyp;

    switch (hdr->cmd) {
    case SOCKS_CONNECT:

        switch (hdr->atyp) {
        case SOCKS_IPv4:

            memcpy(&addr, buf + 4, 4);
            memcpy(&port, buf + 8, 2);

            pw_debug("connect to %s:%d\n", inet_ntoa(addr), ntohs(port));

            ret = socks_ip_connect(c, addr.s_addr, ntohs(port));
            if (ret == 0)
                c->state = SOCKS_SERVE;

            return socks_replies(c, ret, 0, NULL, 0, 0);

        case SOCKS_DOMAIN:

            domain_len = buf[4];

            memcpy(domain, buf + 5, domain_len);
            memcpy(&port, buf + 5 + domain_len, 2);

            pw_debug("connect to %s:%d\n", domain, ntohs(port));

            ret = socks_domain_connect(c, domain, ntohs(port));
            if (ret == 0)
                c->state = SOCKS_SERVE;

            return socks_replies(c, ret, 0, NULL, 0, 0);
        case SOCKS_IPv6: /* TODO */
        default:
            pw_debug("Unsupported address type: %c\n", hdr->atyp);
            return socks_replies(c, SOCKS_ADDRESS_TYPE_NOT_SUPPORTED, 0, NULL,
                                 0, 0);
        }
        break;

    case SOCKS_BIND:              /* TODO */
    case SOCKS_UDP_ASSOCIATE_UDP: /* TODO */
    default:
        pw_debug("Unsupported command: %c\n", hdr->cmd);
        return socks_replies(c, SOCKS_COMMAND_NOT_SUPPORTED, 0, NULL, 0, 0);
    }

    return 0;
}

int socks_serve(struct socks_conn *c, int fd)
{
    char buf[1024] = {0};
    int n;

    while (1) {
        n = read(fd, buf, sizeof(buf));
        if (n <= 0) {
            if (n == -1) {
                if (errno == EAGAIN)
                    break;
                pw_error("read");
            }
            return -1;
        }
        if (write(fd == c->srcfd ? c->dstfd : c->srcfd, buf, n) == -1) {
            pw_error("write");
            return -1;
        }
    }

    return 0;
}

static int socks_replies(struct socks_conn *c, uint8_t rep, uint8_t atyp,
                         const char *addr, int addrlen, uint16_t port)
{
    /**
     * +----+-----+-------+------+----------+----------+
     * |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
     * +----+-----+-------+------+----------+----------+
     * | 1  |  1  | X'00' |  1   | Variable |    2     |
     * +----+-----+-------+------+----------+----------+
     */
    char buf[] = {SOCKS_VER, rep, 0, SOCKS_IPv4, 0, 0, 0, 0, 0, 0};

    if (write(c->srcfd, buf, sizeof(buf)) == -1) {
        pw_error("write");
        return -1;
    }

#if 0
  /* TODO */
  char buf[1024] = {};
  struct
  {
    uint8_t ver;
    uint8_t rep;
    uint8_t rsv;
    uint8_t atyp;
  } *hdr = (void *)buf;

  hdr->ver = SOCKS_VER;
  hdr->rep = rep;
  hdr->rsv = 0;
  hdr->atyp = atyp;
#endif

    return 0;
}

static int socks_ip_connect(struct socks_conn *c, in_addr_t addr,
                            in_port_t port)
{
    struct sockaddr_in si = {
        .sin_family = AF_INET, .sin_addr = addr, .sin_port = htons(port)};
    int fd, ret = SOCKS_SUCCEEDED;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        pw_error("socket");
        ret = SOCKS_FAILURE;
        goto end;
    }

    /* TODO: add connect timeout */

    if (connect(fd, (struct sockaddr *)&si, sizeof(si)) == -1) {
        ret = (errno == ENETUNREACH)    ? SOCKS_NETWORK_UNREACHABLE
              : (errno == EHOSTUNREACH) ? SOCKS_HOST_UNREACHABLE
              : (errno == ECONNREFUSED) ? SOCKS_CONNECTION_REFUSED
                                        : SOCKS_FAILURE;
        close(fd);
        pw_error("connect");
    } else {
        c->dstfd = fd;
    }

end:
    return ret;
}

static int socks_domain_connect(struct socks_conn *c, const char *domain,
                                in_port_t port)
{
    struct addrinfo hints = {}, *addr_list, *curr;
    int fd, ret = SOCKS_SUCCEEDED;
    char portstr[8] = {};

    sprintf(portstr, "%d", port);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(domain, portstr, &hints, &addr_list) == -1) {
        pw_error("getaddrinfo");
        return -1;
    }

    for (curr = addr_list; curr; curr = curr->ai_next) {
        fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (fd == -1) {
            pw_error("socket");
            ret = -1;
            continue;
        }

        /* TODO: add connect timeout */

        if (connect(fd, curr->ai_addr, curr->ai_addrlen) == -1) {
            ret = (errno == ENETUNREACH)    ? SOCKS_NETWORK_UNREACHABLE
                  : (errno == EHOSTUNREACH) ? SOCKS_HOST_UNREACHABLE
                  : (errno == ECONNREFUSED) ? SOCKS_CONNECTION_REFUSED
                                            : SOCKS_FAILURE;
            pw_error("connect");
            close(fd);
            continue;
        }
        break;
    }

    c->dstfd = fd;
    freeaddrinfo(addr_list);

    return ret;
}
