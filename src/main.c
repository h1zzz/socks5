/* node.c */

#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "misc.h"
#include "socks.h"
#include "handler.h"

static void usage (const char *name);
static void read_options (int argc, char **argv);
static void initializer (void);
static void master_process (void);

struct g_option g_opt = {
  .worker_processes = 1,
  .worker_connections = 1024,
};

int
main (int argc, char *argv[])
{
  read_options (argc, argv);
  initializer ();

  master_process ();

  return 0;
}

static void
usage (const char *name)
{
  fprintf (
      stderr,
      "%s Usage: --host 0.0.0.0 --port 1080 --user admin --passwd 123456 -d\n"
      "Options:\n"
      "  -d, --daemon\n"
      "      --host\n"
      "      --port\n"
      "  -u, --user\n"
      "  -p, --passwd\n"
      "  -C, --worker_connections\n"
      "  -P, --worker_processes\n"
      "  -h, --help\n"
      "  -v, --version\n",
      name);
  exit (0);
}

static void
read_options (int argc, char **argv)
{
  static struct option options[] = {
    { "daemon", no_argument, NULL, 'd' },
    { "host", required_argument, NULL, 1 },
    { "port", required_argument, NULL, 2 },
    { "user", required_argument, NULL, 'u' },
    { "passwd", required_argument, NULL, 'p' },
    { "worker_connections", required_argument, NULL, 'C' },
    { "worker_processes", required_argument, NULL, 'P' },
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    {},
  };
  int opt, opt_index = 0, i;

  while ((opt = getopt_long (argc, argv, "hv", options, &opt_index)) != -1)
  {
    switch (opt)
    {
    case 1:
      g_opt.host = optarg;
      break;
    case 2:
      g_opt.port = atoi (optarg);
      break;
    case 'd':
      g_opt.is_daemon = 1;
      break;
    case 'u':
      g_opt.user = optarg;
      break;
    case 'p':
      g_opt.passwd = optarg;
      break;
    case 'C':
      g_opt.worker_connections = atoi (optarg);
      break;
    case 'P':
      g_opt.worker_processes = atoi (optarg);
      break;
    case 'v':
      fprintf (stdout, "%s version " NODE_VERSION "\n", basename (argv[0]));
      exit (0);
    case 'h':
      usage (basename (argv[0]));
      break;
    case '?':
      usage (basename (argv[0]));
      break;
    default:
      usage (basename (argv[0]));
    }
  }

  if (g_opt.host == NULL || g_opt.port == 0)
    usage (basename (argv[0]));
}

static void
initializer (void)
{
  if (g_opt.is_daemon)
    daemonize ();
}

static void
master_process (void)
{
  struct socks *s;
  int fd;

  pw_debug ("start master process %d\n", getpid ());

  /* s = socks_create ("0.0.0.0", 1080, "admin", "123456"); */
  s = socks_create (g_opt.host, g_opt.port, g_opt.user, g_opt.passwd);
  if (!s)
  {
    pw_debug ("%s:%d %s:%s\n", g_opt.host, g_opt.port, g_opt.user,
              g_opt.passwd);
    abort ();
  }

  fd = s->fd;

  worker_listen_add (fd, EV_READ, handler_socks, s);

  while (1)
    sleep (10);

  pw_debug ("kill session group all process.\n");

  /* kill all child process. */
  kill (0, SIGQUIT);
}
