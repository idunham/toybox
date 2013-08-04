/*
 * lspci - written by Isaac Dunham

USE_LSPCI(NEWTOY(lspci, "emkn@", TOYFLAG_USR|TOYFLAG_BIN))

config LSPCI
  bool "lspci"
  default n
  help
    usage: lspci [-ekmn@]

    List PCI devices.
    -e  Print all 6 digits in class (like elspci)
    -k  Print kernel driver
    -m  Machine parseable format
    -n  Numeric output

config LSPCI_TEXT
  bool "lspci readable output"
  depends on LSPCI
  default n
  help
    lspci without -n prints readable descriptions;
    lspci -nn prints both readable and numeric description
*/
#define FOR_lspci
#include "toys.h"
extern int find_in_db(char * , char * , FILE * , char * , char * );

GLOBALS(
long numeric;

FILE * db;
)

int do_lspci(struct dirtree *new)
{
  int alen = 8, dirfd, res = 2; //no textual descriptions read
  char *dname = dirtree_path(new, &alen);
  memset(toybuf, 0, 4096);
  struct {
    char class[16], vendor[16], device[16], module[256],
    vname[256], devname[256];
  } *bufs = (void*)(toybuf + 2);

  if (!strcmp("/sys/bus/pci/devices", dname)) return DIRTREE_RECURSE;
  errno = 0;
  dirfd = open(dname, O_RDONLY);
  if (dirfd > 0) {
    char *p, **fields = (char*[]){"class", "vendor", "device", ""};

    for (p = toybuf; **fields; p+=16, fields++) {
      int fd, size;

      if ((fd = openat(dirfd, *fields, O_RDONLY)) < 0) continue;
      size = ((toys.optflags & FLAG_e) && (p == toybuf)) ? 8 : 6;
      p[read(fd, p, size)] = '\0';
      close(fd);
    }

    close(dirfd);
    if (!errno) {
      char *driver = "";
      char *fmt = toys.optflags & FLAG_m ? "%s, \"%s\" \"%s\" \"%s\" \"%s\"\n"
                                                   : "%s Class %s: %s:%s %s\n";

      if (toys.optflags & FLAG_k) {
        strcat(dname, "/driver");
        if (readlink(dname, bufs->module, sizeof(bufs->module)) != -1)
          driver = basename(bufs->module);
      }
      if (CFG_LSPCI_TEXT && (TT.numeric != 1)) {
        //Look up text
        fseek(TT.db, 0, SEEK_SET);
        res = find_in_db(bufs->vendor, bufs->device, TT.db,
                            bufs->vname, bufs->devname);
      }
      if (CFG_LSPCI_TEXT && (TT.numeric == 2)) {
        fmt = toys.optflags & FLAG_m 
            ? "%s, \"%s\" \"%s [%s]\" \"%s [%s]\" \"%s\"\n"
            : "%s Class %s: %s [%s] %s [%s] %s\n";
        printf(fmt, new->name + 5, bufs->class, bufs->vname, bufs->vendor,
               bufs->devname, bufs->device, driver);
      } else {
        printf(fmt, new->name + 5, bufs->class, 
               (res < 2) ? bufs->vname : bufs->vendor, 
               !(res) ? bufs->devname : bufs->device, driver);
      }
      
    }
  }
  return 0;
}

void lspci_main(void)
{
  if (CFG_LSPCI_TEXT && (TT.numeric != 1)) {
    TT.db = fopen("/usr/share/misc/pci.ids", "r");
    if (errno) {
      TT.numeric = 1;
      error_msg("could not open PCI ID db");
    }
  }

  dirtree_read("/sys/bus/pci/devices", do_lspci);
}
