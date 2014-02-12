/* mkdir.c - Make directories
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/mkdir.html

USE_MKDIR(NEWTOY(mkdir, "<1vpm:", TOYFLAG_BIN|TOYFLAG_UMASK))

config MKDIR
  bool "mkdir"
  default y
  help
    usage: mkdir [-vp] [-m mode] [dirname...]
    Create one or more directories.

    -m	set permissions of directory to mode.
    -p	make parent directories as needed.
    -v	verbose
*/

#define FOR_mkdir
#include "toys.h"
int r_mkdir(char*, mode_t, int);

GLOBALS(
  char *arg_mode;

  mode_t mode;
)

static int do_mkdir(char *dir)
{
  mode_t mode = (toys.optflags&FLAG_m ? TT.mode : 0777&~toys.old_umask);
  int flags = 0;

  if (toys.optflags & FLAG_p) flags |= 1;
  if (toys.optflags & FLAG_v) flags |= 2;
  if (r_mkdir(dir,mode,flags))
    return 1;
  return 0;
}

void mkdir_main(void)
{
  char **s;

  if(toys.optflags&FLAG_m) TT.mode = string_to_mode(TT.arg_mode, 0777);

  for (s=toys.optargs; *s; s++) if (do_mkdir(*s)) perror_msg("'%s'", *s);
}
