/* resolve_modalias.c - Try to resolve module alias to name
 *
 * Written by Isaac Dunham, released as CC0 in 2013

USE_RESOLVE_MODALIAS(NEWTOY(resolve_modalias, "<1d:", TOYFLAG_BIN))

config RESOLVE_MODALIAS
  bool "resolve_modalias"
  default "n"
  help
    usage: resolve_modalias [-d MODDIR/modules.alias]  alias1 ...

    Output a module name (as accepted by modinfo or modprobe) 
    for each module alias given.
    returns number not matched
*/

#define FOR_resolve_modalias
#include "toys.h"
#include <fnmatch.h>

GLOBALS(
char *aliases;
)

char * resolve_name(FILE * fil, char *alias)
{
  char *ret, *ptr;
  int slen;
  errno = 0;

  while (!errno) {
    ret = fgets(toybuf, sizeof(toybuf), fil);
    if ((ret==toybuf) && (ret==strstr(ret, "alias "))) {
      slen = strlen(toybuf);
      ptr = ret = toybuf + 6;
      ret = strstr(ptr, " ");
      if ((long)ret - (long)toybuf < slen + 1) {
        if (toybuf[slen - 1] == '\n') toybuf[slen-1] = '\0';
        ret[0] = '\0'; ret++;
        if (!fnmatch( ptr, alias, FNM_PERIOD)) return ret;
      } 
    }
  }
  return (char *)0;
}

void resolve_modalias_main(void)
{
  FILE * fil;
  char * modname;

  if (!TT.aliases) {
    struct utsname uts;

    if (uname(&uts) < 0) perror_exit("bad uname");
    sprintf(toybuf, "/lib/modules/%s/modules.alias", uts.release);
  }
  fil = xfopen(toybuf, "r");
  for(int i = 0; toys.optargs[i]; i++) {
    if (modname = resolve_name(fil, toys.optargs[i])) {
      printf("%s\n", modname);
    } else {
      toys.exitval++;
    }
  }
}
