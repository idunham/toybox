/* vi: set sw=4 ts=4:
 *
 * who.c - display who is on the system
 *
 * Copyright 2012 ProFUSION Embedded Systems
 *
 * by Luis Felipe Strano Moraes <lfelipe@profusion.mobi>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/who.html

USE_WHO(NEWTOY(who, NULL, TOYFLAG_BIN))

config WHO
	bool "who"
	default n
	help
	  usage: who

	  Print logged user information on system

*/

#include "toys.h"

void who_main(void)
{
    struct utmpx *entry;

    setutxent();

    while ((entry = getutxent())) {
        if (entry->ut_type == USER_PROCESS) {
            time_t time;
            int time_size;
            char * times;

            time = entry->ut_tv.tv_sec;
            times = ctime(&time);
            time_size = strlen(times) - 2;
            printf("%s\t%s\t%*.*s\t(%s)\n", entry->ut_user, entry->ut_line, time_size, time_size, ctime(&time), entry->ut_host);

        }
    }

    endutxent();
}
