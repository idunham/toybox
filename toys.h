/* vi: set ts=4 :*/
/* Toybox infrastructure.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "generated/config.h"

#include "lib/portability.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pty.h>
#include <pwd.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>

#define _XOPEN_SOURCE 600
#include <time.h>

#include "lib/lib.h"
#include "toys/e2fs.h"

// Get list of function prototypes for all enabled command_main() functions.

#define NEWTOY(name, opts, flags) void name##_main(void);
#define OLDTOY(name, oldname, opts, flags)
#include "generated/newtoys.h"
#include "generated/globals.h"

// These live in main.c

struct toy_list *toy_find(char *name);
void toy_init(struct toy_list *which, char *argv[]);
void toy_exec(char *argv[]);

// List of available applets

#define TOYFLAG_USR      (1<<0)
#define TOYFLAG_BIN      (1<<1)
#define TOYFLAG_SBIN     (1<<2)
#define TOYMASK_LOCATION ((1<<4)-1)

#define TOYFLAG_NOFORK   (1<<4)
#define TOYFLAG_UMASK    (1<<5)

extern struct toy_list {
        char *name;
        void (*toy_main)(void);
        char *options;
        int flags;
} toy_list[];

// Global context for any applet.

extern struct toy_context {
	struct toy_list *which;  // Which entry in toy_list is this one?
	int exitval;             // Value error_exit feeds to exit()
	char **argv;             // Original command line arguments
	unsigned optflags;       // Command line option flags from get_optflags()
	char **optargs;          // Arguments left over from get_optflags()
	int optc;                // Count of optargs
	int exithelp;            // Should error_exit print a usage message first?  (Option parsing.)
	int old_umask;
} toys;

// One big temporary buffer, for use by applets (not library functions).

extern char toybuf[4096];

#define DEFINE_GLOBALS(...)
