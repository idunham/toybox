/* id-db.c - look up textual descriptions in tab-based databases
 *
 * Copyright 2013 AD Isaac Dunham
 */

#include "toys.h"

char * id_check_match(char * id, char * buf)
{
	int i = 0;
	while (id[i]) {
		if (id[i] == buf[i]) {
			i++;
		} else {
			return (char *)0L;
		}
	}
	return (buf + i + 2);
}

/*
 * In: vendid, devid, fil
 * Out: vname, devname
 * Out must be zeroed before use.
 * vmame and devname must be char[256], zeroed out
 * Returns (2 - number of IDs matched): vendor must be matched for 
 * dev to be matched
 */
int find_in_db(char * vendid, char * devid, FILE * fil,
		char * vname, char * devname)
{
	char buf[256], *vtext = 0L, *dtext = 0L;
	while (!(vname[0])) {
		//loop through
		if (fgets(buf, 255, fil)==NULL) return 2;
		if (vtext = checkmatch(vendid, buf))
			strncpy(vname, vtext, strlen(vtext) - 1);
	}
	while (!(devname[0])) {
		if ((fgets(buf, 255, fil)==NULL) || (buf[0] != '\t' ))
			return 1;
		if (dtext = checkmatch(devid, buf + 1))
			strncpy(devname, dtext, strlen(dtext) - 1);
	}
	return 0; /* Succeeded in matching both */
}


