/*
 * lspci - written by Isaac Dunham

USE_LSPCI(NEWTOY(lspci, "emkns:", TOYFLAG_USR|TOYFLAG_BIN))

config LSPCI
  bool "lspci"
  default n
  help
    usage: lspci [-ekmn]

    List PCI devices.
    -e  Print all 6 digits in class (like elspci)
    -k  Print kernel driver
    -m  Machine parseable format
    -n  Numeric output (default)
*/
#define FOR_lspci
#include "toys.h"
char * readat_name(int dirfd, char *fname, size_t nbyte)
{
  int fd;
  if ((fd = openat(dirfd, fname, O_RDONLY)) < 0) return NULL;
  char *buf = xzalloc(nbyte+1);
  read(fd, buf, nbyte);
  close(fd);
  return buf;
}

int do_lspci(struct dirtree *new)
{
  int alen = 8;
  char *dname = dirtree_path(new, &alen);

  if (!strcmp("/sys/bus/pci/devices", dname))
    return DIRTREE_RECURSE;
  errno = 0;
  int dirfd = open(dname, O_RDONLY);
  if (dirfd > 0) {
    char *class = readat_name(dirfd, "class",
                (toys.optflags & FLAG_e) ? 8 : 6);
    char *vendor = readat_name(dirfd, "vendor", 6);
    char *device = readat_name(dirfd, "device", 6);

    close(dirfd);
    if (!errno) {
      char *driver = "";
      char *fmt = toys.optflags & FLAG_m ?  "%s, \"%s\" \"%s\" \"%s\" \"%s\"\n"
                                                    : "%s Class %s: %s:%s %s\n";

      if (toys.optflags & FLAG_k) {
        char module[256] = "";
        strcat(dname, "/driver");
        if (-1 != readlink(dname, module, sizeof(module)))
          driver = basename(module);
      }
      printf(fmt, new->name + 5, class + 2, vendor + 2, device + 2,
               driver);
    }
  }
  return 0;
}

void lspci_main(void)
{
  dirtree_read("/sys/bus/pci/devices", do_lspci);
}
