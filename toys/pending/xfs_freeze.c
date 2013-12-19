/* xfs_freeze.c - freeze or thaw filesystem
 *

USE_XFS_FREEZE(NEWTOY(xfs_freeze, "<1>1f|u|[!fu]", TOYFLAG_USR|TOYFLAG_SBIN))


config XFS_FREEZE
  bool "xfs_freeze"
  default n
  help
    usage: xfs_freeze {-f  | -u} /PATH/TO/MOUNT
    Freeze or unfreeze a filesystem.
    -f  freeze
    -u  unfreeze
    
*/

#define FOR_xfs_freeze
#include "toys.h"
#include <linux/fs.h>


void xfs_freeze_main(void)
{
  long p = 1;
  int io_call = toys.optflags & FLAG_f ? FIFREEZE : FITHAW;
  toys.exitval = ioctl(xopen(*toys.optargs,O_RDONLY), io_call, &p);
}
