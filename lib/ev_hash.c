/* ev_hash.c */

#include "ev_hash.h"

#include <stddef.h>
#include <stdlib.h>

#include "ev.h"
#include "debug.h"

struct entry
{
  struct entry *next;
  struct event *ev;
};

struct ev_hash
{
  struct entry **table;
  uint32_t size;
};

static uint32_t
sdbm_hash (int fd)
{
  uint32_t hash = 0;
  size_t i;
  char *ptr = (char *)&fd;

  for (i = 0; i < sizeof (uint32_t); i++)
    {
      hash = ptr[i] + (hash << 6) + (hash << 16) - hash;
    }

  return hash & 0x7FFFFFFF;
}

int
ev_hash_new (struct event_base *base)
{
  struct ev_hash *hash;

  hash = calloc (1, sizeof (struct ev_hash));
  if (!hash)
    goto err;

  hash->table = calloc (base->events_size, sizeof (struct entry));
  if (!hash->table)
    goto err;

  hash->size = base->events_size;
  base->events = hash;

  return 0;
err:
  pw_error ("calloc");
  if (hash)
    ev_hash_destroy (base);
  return -1;
}

struct event *
ev_hash_get (struct event_base *base, int fd)
{
  struct ev_hash *hash = base->events;
  struct entry *p;
  uint32_t i;

  i = sdbm_hash (fd) % hash->size;
  p = hash->table[i];

  while (p)
    {
      if (p->ev->fd == fd)
        return p->ev;
      p = p->next;
    }

  return NULL;
}

int
ev_hash_set (struct event_base *base, int fd, struct event *ev)
{
  struct ev_hash *hash = base->events;
  struct entry *p, *n;
  uint32_t i;

  i = sdbm_hash (fd) % hash->size;
  p = hash->table[i];

  while (p)
    {
      if (p->ev->fd == fd)
        {
          free (p->ev);
          p->ev = ev;
          return 0;
        }
      p = p->next;
    }

  n = calloc (1, sizeof (struct entry));
  if (!n)
    {
      pw_error ("calloc");
      return -1;
    }

  n->ev = ev;

  p = hash->table[i];
  if (p)
    n->next = p;

  hash->table[i] = n;

  return 0;
}

int
ev_hash_delete (struct event_base *base, int fd)
{
  struct ev_hash *hash = base->events;
  struct entry *p = NULL, *c;
  uint32_t i;

  i = sdbm_hash (fd) % hash->size;
  c = hash->table[i];

  while (c)
    {
      if (c->ev->fd == fd)
        {
          if (p)
            {
              p->next = c->next;
              free (c->ev);
              free (c);
            }
          else
            {
              if (c->next)
                hash->table[i] = c->next;
              else
                hash->table[i] = NULL;
              free (c->ev);
              free (c);
            }
          return 0;
        }
      p = c;
      c = c->next;
    }

  return -1;
}

void
ev_hash_destroy (struct event_base *base)
{
  struct ev_hash *hash = base->events;
  struct entry *p, *n;
  int i;

  if (hash)
    {
      if (hash->table)
        {
          for (i = 0; i < hash->size; i++)
            {
              p = hash->table[i];
              while (p)
                {
                  n = p->next;
                  free (p->ev);
                  free (p);
                  p = n;
                }
            }
          free (hash->table);
        }
      free (base->events);
      base->events = NULL;
    }
}
