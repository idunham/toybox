/* vi: set sw=4 ts=4:
 *
 * cmp.c - Compare two files.
 *
 * Copyright 2012 Timothy Elliott <tle@holymonkey.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/cmp.html

USE_CMP(NEWTOY(cmp, "<2>2ls", TOYFLAG_USR|TOYFLAG_BIN))

config CMP
	bool "cmp"
	default y
	help
	  usage: cmp [-l] [-s] FILE1 FILE2

	  Compare the contents of two files.

	  -l show all differing bytes
	  -s silent
*/

#include "toys.h"

#define FLAG_s	1
#define FLAG_l	2

DEFINE_GLOBALS(
	int fd;
	char *name;
)

#define TT this.cmp

// This handles opening the file and 

void do_cmp(int fd, char *name)
{
	int i, len1, len2, min_len, size = sizeof(toybuf)/2;
	long byte_no = 1, line_no = 1;
	char *buf2 = toybuf+size;

	// First time through, cache the data and return.
	if (!TT.fd) {
		TT.name = name;
		// On return the old filehandle is closed, and this assures that even
		// if we were called with stdin closed, the new filehandle != 0.
		TT.fd = dup(fd);
		return;
	}

	for (;;) {
		len1 = readall(TT.fd, toybuf, size);
		len2 = readall(fd, buf2, size);

		min_len = len1 < len2 ? len1 : len2;
		for (i=0; i<min_len; i++) {
			if (toybuf[i] != buf2[i]) {
				toys.exitval = 1;
				if (toys.optflags & FLAG_l)
					printf("%ld %o %o\n", byte_no, toybuf[i], buf2[i]);
				else {
					if (!(toys.optflags & FLAG_s)) {
						printf("%s %s differ: char %ld, line %ld\n",
							TT.name, name, byte_no, line_no);
						toys.exitval++;
					}
					goto out;
				}
			}
			byte_no++;
			if (toybuf[i] == '\n') line_no++;
		}
		if (len1 != len2) {
			if (!(toys.optflags & FLAG_s)) {
				printf("cmp: EOF on %s\n",
					len1 < len2 ? TT.name : name);
			}
			toys.exitval = 1;
			break;
		}
		if (len1 < 1) break;
	}
out:
	if (CFG_TOYBOX_FREE) close(TT.fd);
}

void cmp_main(void)
{
	loopfiles_rw(toys.optargs, O_RDONLY, 0, toys.optflags&FLAG_s, do_cmp);
}

