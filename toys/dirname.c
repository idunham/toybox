/* vi: set sw=4 ts=4:
 *
 * dirname.c - show directory portion of path
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/dirname.html

USE_DIRNAME(NEWTOY(dirname, "<1", TOYFLAG_USR|TOYFLAG_BIN))

config DIRNAME
	bool "dirname"
	default y
	help
	  usage: dirname PATH

	  Show directory portion of path.
*/

#include "toys.h"

void dirname_main(void)
{
	puts(dirname(*toys.optargs));
}
