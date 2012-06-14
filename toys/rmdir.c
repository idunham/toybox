/* vi: set sw=4 ts=4:
 *
 * rmdir.c - remove directory/path
 *
 * Copyright 2008 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/rmdir.html

USE_RMDIR(NEWTOY(rmdir, "<1p", TOYFLAG_BIN))

config RMDIR
	bool "rmdir"
	default y
	help
	  usage: rmdir [-p] [dirname...]
	  Remove one or more directories.

	  -p	Remove path.
*/

#include "toys.h"

static void do_rmdir(char *name)
{
	for (;;) {
		char *temp;

		if (rmdir(name)) {
			perror_msg("%s",name);
			return;
		}
		if (!toys.optflags) return;
		if (!(temp=strrchr(name,'/'))) return;
		*temp=0;
	}
}

void rmdir_main(void)
{
	char **s;

	for (s=toys.optargs; *s; s++) do_rmdir(*s);
}
