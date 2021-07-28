/* debug.h */

#ifndef _PW_DEBUG_H
#define _PW_DEBUG_H

#include <libgen.h>
#include <stdio.h>

#include "config.h"

#define CBLK(s)  "\033[0;30m" s "\033[0m"
#define CRED(s)  "\033[0;31m" s "\033[0m"
#define CGRN(s)  "\033[0;32m" s "\033[0m"
#define CBRN(s)  "\033[0;33m" s "\033[0m"
#define CBLU(s)  "\033[0;34m" s "\033[0m"
#define CMGN(s)  "\033[0;35m" s "\033[0m"
#define CCYA(s)  "\033[0;36m" s "\033[0m"
#define CLGR(s)  "\033[0;37m" s "\033[0m"
#define CGRA(s)  "\033[1;90m" s "\033[0m"
#define CLRD(s)  "\033[1;91m" s "\033[0m"
#define CLGN(s)  "\033[1;92m" s "\033[0m"
#define CYEL(s)  "\033[1;93m" s "\033[0m"
#define CLBL(s)  "\033[1;94m" s "\033[0m"
#define CPIN(s)  "\033[1;95m" s "\033[0m"
#define CLCY(s)  "\033[1;96m" s "\033[0m"
#define CBRI(s)  "\033[1;97m" s "\033[0m"
#define BGBLK(s) "\033[40m" s "\033[0m"
#define BGRED(s) "\033[41m" s "\033[0m"
#define BGGRN(s) "\033[42m" s "\033[0m"
#define BGBRN(s) "\033[43m" s "\033[0m"
#define BGBLU(s) "\033[44m" s "\033[0m"
#define BGMGN(s) "\033[45m" s "\033[0m"
#define BGCYA(s) "\033[46m" s "\033[0m"
#define BGLGR(s) "\033[47m" s "\033[0m"
#define BGGRA(s) "\033[100m" s "\033[0m"
#define BGLRD(s) "\033[101m" s "\033[0m"
#define BGLGN(s) "\033[102m" s "\033[0m"
#define BGYEL(s) "\033[103m" s "\033[0m"
#define BGLBL(s) "\033[104m" s "\033[0m"
#define BGPIN(s) "\033[105m" s "\033[0m"
#define BGLCY(s) "\033[106m" s "\033[0m"
#define BGBRI(s) "\033[107m" s "\033[0m"

#define putf(args...) fprintf (stdout, args)

#ifndef NDEBUG
#  include <errno.h>
#  include <string.h>
#  include <unistd.h>
#  define pw_debug(args...)                                                   \
    ({                                                                        \
      fprintf (stderr, CGRA ("%s:%d ") CLGN ("[%d]") CGRA (" - "),            \
               basename (__FILE__), __LINE__, getpid ());                     \
      fprintf (stderr, args);                                                 \
    })
#  define pw_error(str) pw_debug (str CRED (": %s\n"), strerror (errno))
#else
#  define pw_debug(args...)
#  define pw_error(str)
#endif /* NDEBUG */

#endif /* pw_debug.h */
