/* vi: set sw=4 ts=4:
 *
 * yes.c - Repeatedly output a string.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * Not in SUSv3.

config YES
	bool "yes"
	default y
	help
	  usage: yes [args...]

	  Repeatedly output line until killed.  If no args, output 'y'.
*/

#include "toys.h"

void yes_main(void)
{
	for (;;) {
		int i;
		for (i=0; toys.optargs[i]; i++) {
			if (i) xputc(' ');
			xprintf("%s", toys.optargs[i]);
		}
		if (!i) xputc('y');
		xputc('\n');
	}
}
