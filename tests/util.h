/* net_util.h */

#ifndef _NET_UTIL_H
#define _NET_UTIL_H

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "debug.h"

#define MAX_CONN 1024

static int
create_listen (unsigned short port)
{
  struct sockaddr_in si = {};
  int fd;

  fd = socket (PF_INET, SOCK_STREAM, 0);
  if (fd == -1)
    {
      pw_error ("socket");
      return -1;
    }

  si.sin_addr.s_addr = INADDR_ANY;
  si.sin_port = htons (port);
  si.sin_family = AF_INET;

  int opt = 1;
  setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof (opt));

  if (bind (fd, (struct sockaddr *)&si, sizeof (si)) == -1)
    {
      pw_error ("bind");
      return -1;
    }

  listen (fd, SOMAXCONN);

  return fd;
}

#endif /* net_util.h */
