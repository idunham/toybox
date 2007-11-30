/* vi: set sw=4 ts=4: */
/*
 * echo.c - echo supporting -n and -e.
 */

#include "toys.h"

void echo_main(void)
{
	int i = 0;
	char *arg, *from = "\\abfnrtv", *to = "\\\a\b\f\n\r\t\v";

	for (;;) {
		arg = toys.optargs[i];
		if (!arg) break;
		if (i++) xputc(' ');

		// Handle -e

		if (toys.optflags&2) {
			int c, j = 0;
			for (;;) {
				c = arg[j++];
				if (!c) break;
				if (c == '\\') {
					char *found;
					int d = arg[j++];

					// handle \escapes

					if (d) {
						found = strchr(from, d);
						if (found) c = to[found-from];
						else if (d == 'c') goto done;
						else if (d == '0') {
							c = 0;
							while (arg[j]>='0' && arg[j]<='7')
								c = (c*8)+arg[j++]-'0';
						}
					}
				}
				xputc(c);
			}
		} else xprintf("%s", arg);
	}

	// Output "\n" if no -n
	if (!(toys.optflags&1)) xputc('\n');
done:
	xflush();
}
