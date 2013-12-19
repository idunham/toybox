/* getty.c - get a controlling tty
 *
 * Copyright 2013 Isaac Dunham <ibid.ag@gmail.com>
 *
 * No standard.

USE_GETTY(NEWTOY(getty, "il:", TOYFLAG_NEEDROOT|TOYFLAG_SBIN))

config GETTY
  bool "getty"
  default n
  help
    usage: getty [-i] [-l LOGIN_CMD]
    open a tty as controlling terminal, spawn a login program
    -i  Ignore /etc/issue (default for now)
    -l  login command (instead of "login")
*/

#define FOR_getty
#include <toys.h>

GLOBALS(
  char *login;

  char *nul; // so we can xexec(&TT.login)
)


void getty_main(void)
{
  char *tty = ttyname(0);
  int fd;
  if (!TT.login) TT.login = "login";

  if (!tty) perror_exit("no tty");
  close(0); close(1); close(2);
  errno = 0;

  fd = xopen(tty, O_RDWR);
  if (fd) dup2(fd, 0);
  dup2(0, 1);
  dup2(0, 2);
  if (errno) xexit();
  xexec(&TT.login);
}
