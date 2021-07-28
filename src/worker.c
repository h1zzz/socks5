/* worker.c */

#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "ev.h"

struct fd_list
{
  struct fd_list *next;
  int fd;
  event_handler_t *fn;
  void *data;
  uint16_t flags;
};

static void worker_process_quit (int signo);
static void worker_process (void);
static void worker_process_restart (void);
static void worker_process_wait (int signo);

static struct fd_list *fd_list;        /* listen fd list. */
static int is_exit_worker = 0;         /* Exit the old worker process. */
static pid_t *worker_pids;             /* worker process pid array. */
static struct event_base *worker_base; /* woker process event_base. */

void
worker_listen_add (int fd, uint16_t flags, event_handler_t *fn, void *data)
{
  struct fd_list *newfd;

  if (fd < 0 || flags == EV_NONE || !fn)
    return;

  newfd = calloc (1, sizeof (struct fd_list));
  if (!newfd)
    {
      pw_error ("calloc");
      return;
    }

  newfd->fd = fd;
  newfd->fn = fn;
  newfd->data = data;
  newfd->flags = flags;
  newfd->next = fd_list;
  fd_list = newfd;

  worker_process_restart ();
}

void
worker_listen_delete (int fd)
{
  struct fd_list *prev = NULL, *curr;

  /* delete listen fd. */
  for (curr = fd_list; curr; curr = curr->next)
    {
      if (curr->fd == fd)
        {
          /* delete fd. */
          if (prev)
            prev->next = curr->next;
          else
            fd_list = curr->next;
          free (curr);
          break;
        }
      prev = curr;
    }

  worker_process_restart ();
}

static void
worker_quit (int signo)
{
  struct fd_list *curr;
  int ret;

  is_exit_worker = 1;

  /* delete listen fd for event. */
  for (curr = fd_list; curr; curr = curr->next)
    {
      pw_debug ("delete listen fd: %d\n", curr->fd);
      ret = event_base_delete (worker_base, curr->fd, curr->flags);
      if (ret == -1)
        {
          pw_debug ("delete %d failed\n", curr->fd);
        }
    }
}

static void
worker_process (void)
{
  struct sigaction action = {};
  struct timeval tv = {};
  struct fd_list *curr;
  int ret;

  pw_debug ("start worker process %d\n", getpid ());

  action.sa_handler = worker_quit;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction (SIGQUIT, &action, NULL) == -1)
    {
      pw_error ("sigaction");
      abort ();
    }

  worker_base = event_base_new (g_opt.worker_connections);
  if (!worker_base)
    {
      pw_debug ("event_base_new failed\n");
      abort ();
    }

  /* add listen fd to event. */
  for (curr = fd_list; curr; curr = curr->next)
    {
      ret = event_base_add (worker_base, curr->fd, curr->flags, curr->fn,
                            curr->data);
      if (ret == -1)
        {
          pw_debug ("event_base_add %d failed\n", curr->fd);
        }
    }

  while (1)
    {
      tv.tv_sec = 5;
      tv.tv_usec = 0;

      ret = event_base_loop (worker_base, &tv);
      if (ret == -1)
        {
          pw_error ("event_base_loop");
        }

      if (ret == 0) /* timeout */
        {
          pw_debug ("worker event timeout %d, %d\n", ret,
                    worker_base->event_num);
          if (is_exit_worker && worker_base->event_num == 0)
            break; /* exit woker process. */
        }
    }

  event_base_destroy (worker_base);

  pw_debug ("exit worker process %d\n", getpid ());

  exit (0);
}

static void
worker_process_restart (void)
{
  struct sigaction action = {};
  sigset_t sigs;
  pid_t *pids;
  int i;

  pids = calloc (g_opt.worker_processes, sizeof (pid_t));
  if (!pids)
    {
      pw_error ("calloc");
      abort ();
    }

  sigemptyset (&sigs);
  sigaddset (&sigs, SIGCHLD);

  if (sigprocmask (SIG_BLOCK, &sigs, NULL) == -1)
    {
      pw_error ("sigprocmask");
      abort ();
    }

  for (i = 0; i < g_opt.worker_processes; i++)
    {
      pids[i] = fork ();

      if (pids[i] == -1)
        {
          pw_error ("fork");
          continue;
        }

      if (pids[i] == 0) /* child process. */
        {
          /* close parent fds. */
          worker_process ();
        }

      /* parent process. */
      if (worker_pids) /* stop old worker process. */
        kill (worker_pids[i], SIGQUIT);
    }

  if (worker_pids)
    free (worker_pids);

  action.sa_handler = worker_process_wait;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction (SIGCHLD, &action, NULL) == -1)
    {
      pw_error ("sigaction");
      abort ();
    }

  if (sigprocmask (SIG_UNBLOCK, &sigs, NULL) == -1)
    {
      pw_error ("sigprocmask");
      abort ();
    }

  worker_pids = pids;
}

static void
worker_process_wait (int signo)
{
  int status;
  pid_t pid;

  while (1)
    {
      pid = waitpid (-1, &status, WNOHANG);
      if (pid == -1)
        {
          if (errno != ECHILD)
            {
              pw_error ("waitpid");
            }
          break;
        }
      if (pid == 0)
        break;
      pw_debug ("%d child %d process exit, status: %d\n", getpid (), pid,
                WEXITSTATUS (status));
    }
}
