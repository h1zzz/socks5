/* my_test.c */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"

int
main (int argc, char *argv[])
{
  char buf[256] = {};
  int fd, n;
  struct sockaddr_in si = {};

  si.sin_addr.s_addr = inet_addr ("127.0.0.1");
  si.sin_port = htons (1080);
  si.sin_family = AF_INET;

  fd = socket (PF_INET, SOCK_STREAM, 0);
  if (fd == -1)
    {
      pw_error ("socket");
      return -1;
    }

  if (connect (fd, (struct sockaddr *)&si, sizeof (si)) == -1)
    {
      pw_error ("connect");
      return -1;
    }

  fork ();
  pw_debug ("pid: %d\n", getpid ());

  n = sprintf (buf, "hello pid: %d\n", getpid ());
  write (fd, buf, n);

  n = read (fd, buf, sizeof (buf));
  if (n == -1)
    {
      pw_error ("read");
    }

  pw_debug ("%d, %d, %s\n", getpid (), n, buf);
  sleep (30);
  close (fd);

  return 0;
}
