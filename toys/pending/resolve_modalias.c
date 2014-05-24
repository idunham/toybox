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
*/

#define FOR_resolve_modalias
#include "toys.h"

GLOBALS(
char *aliases;
)

struct string_list * resolve_name(FILE * fil, char *alias, struct string_list *modules)
{
  char *ret, *ptr;
  int slen;
  errno = 0;

  for (;;) {
    ret = fgets(toybuf, sizeof(toybuf), fil);
    if (ret!=toybuf) return modules;
    if (ret==strstr(ret, "alias ")) {
      slen = strlen(toybuf);
      ptr = ret = toybuf + 6;
      ret = strstr(ptr, " ");
      if ((long)ret - (long)toybuf < slen + 1) {
        if (toybuf[slen - 1] == '\n') toybuf[slen-1] = '\0';
        ret[0] = '\0'; ret++;
        if (!fnmatch( ptr, alias, FNM_PERIOD)) {
          modules->next = xzalloc(sizeof(struct string_list) + strlen(ret) + 1);
          strcpy(modules->next->str, ret);
          modules=modules->next;
        }
      } 
    }
  }
}

void print_free(void * arg)
{
  puts((char *)arg + sizeof(char *));
  free(arg);
}

void resolve_modalias_main(void)
{
  FILE * fil;
  struct string_list *names = xzalloc(sizeof(struct string_list)), *curr;
  curr = names;


  if (!TT.aliases) {
    struct utsname uts;

    if (uname(&uts) < 0) perror_exit("bad uname");
    sprintf(toybuf, "/lib/modules/%s/modules.alias", uts.release);
  } else {
    snprintf(toybuf, sizeof(toybuf), "%s", TT.aliases);
  }
  fil = xfopen(toybuf, "r");
  int i;
  for(i = 0; toys.optargs[i]; i++) {
    curr = resolve_name(fil, toys.optargs[i], curr );
    fseek(fil, 0, SEEK_SET);
  }
  if (names && names->next) llist_traverse(names->next, print_free);
}
