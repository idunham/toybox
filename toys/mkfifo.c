/* vi: set sw=4 ts=4:
 *
 * mkfifo.c - Create FIFOs (named pipes)
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://pubs.opengroup.org/onlinepubs/009695399/utilities/mkfifo.html
 *
 * TODO: Add -m

USE_MKFIFO(NEWTOY(mkfifo, "<1", TOYFLAG_BIN))

config MKFIFO
	bool "mkfifo"
	default y
	help
	  usage: mkfifo [fifo_name...]

	  Create FIFOs (named pipes).
*/

#include "toys.h"

DEFINE_GLOBALS(
	long mode;
)

#define TT this.mkfifo

void mkfifo_main(void)
{
	char **s;

	TT.mode = 0666;

	for (s = toys.optargs; *s; s++) {
		if (mknod(*s, S_IFIFO | TT.mode, 0) < 0) {
			perror_msg("cannot create fifo '%s'", *s);
			toys.exitval = 1;
		}
	}
}
