/* cpio.c - a basic cpio
 *
 * Written 2013 AD by Isaac Dunham; this code is placed under the 
 * same license as toybox or as CC0, at your option.
USE_CPIO(NEWTOY(cpio, "H:iot))

config CPIO
  bool "cpio"
  default n
  help
    usage: cpio { -i | -o | -t } [-H fmt] 

    copy files into and out of an archive
    -i  extract from archive into file system (stdin is an archive)
    -o  create archive (stdin is a list of files, stdout is an archive)
    -t  list files (stdin is an archive, stdout is a list of files)
    -H fmt   Write archive in specified format:
    newc  new character format
*/
#define FOR_cpio
#include "toys.h"

GLOBALS(
char * fmt;
)

/* Iterate through a list of files, read from stdin.
 * No users need rw.
 */
void stdin_loopfiles(void (*function)(int fd, char *name))
{
  int fd;
  char *name = toybuf;

  while (name != NULL){
    memset(toybuf, 0, sizeof(toybuf));
    name = fgets(toybuf, sizeof(toybuf) - 1, stdin);
    if (name != NULL) {
      if (toybuf[strlen(name)] == '\n') { //if a line didn't fit, ignore.
        toybuf[strlen(name)] = '\0';
	fd = open(name, O_RDONLY);
	if (fd > 0) {
          function(fd, name);
	  close(fd);
	}
      }
    }
  }
}

struct cpio_newc_header {
  char    c_magic[6];
  char    c_ino[8];
  char    c_mode[8];
  char    c_uid[8];
  char    c_gid[8];
  char    c_nlink[8];
  char    c_mtime[8];
  char    c_filesize[8];
  char    c_devmajor[8];
  char    c_devminor[8];
  char    c_rdevmajor[8];
  char    c_rdevminor[8];
  char    c_namesize[8];
  char    c_check[8];
};

void write_cpio_member(int fd, char *name)
{
  struct cpio_newc_header hdr;
  memset(&hdr, 0, sizeof(hdr));
  struct stat buf;
  if (stat(name, &buf) == -1) return;
  //strcpy(hdr.c_magic, "070701");
  snprintf(&hdr, sizeof(hdr), "070701%08o%08o%08o%08o",
	   buf.st_ino, buf.st_mode, buf.st_uid, buf.st_gid, buf.st_nlink);
  
}

void cpio_main(void)
{
  error_exit("Sorry, cpio isn't implemented yet");
}
