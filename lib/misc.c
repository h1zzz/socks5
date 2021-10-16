/* misc.c */

#include "misc.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"

int set_nonblocking(int fd, int nonblocking)
{
    int f;

    f = fcntl(fd, F_GETFL);
    if (fd == -1) {
        pw_error("fcntl");
        return -1;
    }

    if (nonblocking)
        f |= O_NONBLOCK;
    else
        f &= ~O_NONBLOCK;

    if (fcntl(fd, F_SETFL, f) == -1) {
        pw_error("fcntl");
        return -1;
    }
    return 0;
}

void daemonize(void)
{
    pid_t pid;
    int fd;

    pid = fork();
    if (pid == -1) {
        pw_error("frok");
        exit(-1);
    }

    if (pid != 0)
        exit(0); /* exit parent process. */

    /* clild process. */

    if (setsid() == -1) {
        pw_error("setsid");
        exit(0);
    }

    pid = fork();
    if (pid == -1) {
        pw_error("frok");
        exit(-1);
    }

    if (pid != 0)
        exit(0); /* exit parent process. */

    chdir("/");

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }

    dup2(STDOUT_FILENO, fd);
    dup2(STDERR_FILENO, fd);
    dup2(STDIN_FILENO, fd);
}
