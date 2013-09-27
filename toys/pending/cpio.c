/* cpio.c - a basic cpio
 *
 * Written 2013 AD by Isaac Dunham; this code is placed under the 
 * same license as toybox or as CC0, at your option.
USE_CPIO(NEWTOY(cpio, "H:iot", TOYFLAG_BIN))

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
    newc  SVR4 new character format (default)
*/
#define FOR_cpio
#include "toys.h"

GLOBALS(
char * fmt;
)

/* Iterate through a list of files, read from stdin.
 * No users need rw.
 */
void loopfiles_stdin(void (*function)(int fd, char *name))
{
  int fd;
  char *name = toybuf;

  while (name != NULL){
    memset(toybuf, 0, sizeof(toybuf));
    name = fgets(toybuf, sizeof(toybuf) - 1, stdin);
    
    if (name != NULL) {
      if (toybuf[strlen(name) - 1] == '\n' ) { 
        toybuf[strlen(name) - 1 ] = '\0';
	fd = open(name, O_RDONLY);
	if (fd > 0) {
          function(fd, name);
	  close(fd);
	}
	errno = 0;
      }
    }
  }
}

struct newc_header {
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

void write_cpio_member(int fd, char *name, struct stat buf)
{
  struct newc_header *hdr;
  size_t out = 0;
  unsigned int n = 0x00000000, nlen = strlen(name);

  hdr = malloc(sizeof(struct newc_header) + 1);
  memset(hdr, '0', sizeof(struct newc_header));
  snprintf((char *)(hdr), sizeof(struct newc_header)+1, 
          "070701%08X%08X" "%08X%08X"
	  "%08X%08X%08X"
	   "%08X%08X" "%08X%08X%08X00000000",
	   (unsigned int)(buf.st_ino), buf.st_mode, buf.st_uid, buf.st_gid, 
	   buf.st_nlink, (uint32_t)(buf.st_mtime), (uint32_t)(buf.st_size), 
	   major(buf.st_dev), minor(buf.st_dev),
           major(buf.st_rdev), minor(buf.st_rdev), nlen+1);
  write(1, hdr, sizeof(struct newc_header));
  write(1, name, nlen);
  write(1, &n, 4 - ((nlen + 2) % 4));
  for (; (lseek(fd, 0, SEEK_CUR) < (uint32_t)(buf.st_size));) {
    out = read(fd, toybuf, sizeof(toybuf));
    if (out > 0) { 
      write(1, toybuf, out);
    }
    if (errno || out < sizeof(toybuf)) break;
  }
  write(1, &n, 4 - (buf.st_size % 4));
}

void write_cpio_call(int fd, char *name)
{
  struct stat buf;
  if (stat(name, &buf) == -1) return;
  write_cpio_member(fd, name, buf);
}

//convert hex to uint; mostly to allow using bits of non-terminated strings
unsigned int htou(char * hex)
{
  unsigned int ret = 0, i = 0;
  for (;(i <8 || hex[i]);) {
     ret *= 16;
     switch(hex[i]) { 
     case '0':
       break;
     case '1': 
     case '2': 
     case '3': 
     case '4': 
     case '5': 
     case '6': 
     case '7': 
     case '8': 
     case '9': 
       ret += hex[i] - '1';
       break;
     case 'A': 
     case 'B': 
     case 'C': 
     case 'D': 
     case 'E': 
     case 'F': 
       ret += hex[i] - 'A';
       break;
     }
  }
}

#define READ_VERBOSE 1
#define READ_EXTRACT 2
int read_cpio_member(int fd, int how)
{
  uint32_t nsize, fsize, mode;
  int pad, ofd = 0; 
  struct newc_header hdr;
  char *name;

  xreadall(fd, &hdr, sizeof(struct newc_header));
  //here we store anything we'll use, or set it up...
  nsize = htou(hdr.c_namesize);
  fsize = htou(hdr.c_filesize);
  mode = htou(hdr.c_mode);
  //and now read the name
  name = xmalloc(nsize);
  if (!strcmp("TRAILER!!!", name)) return 0;
  pad = 4 - ((readall(fd, name, nsize) + 2) % 4); 
  if (how | READ_EXTRACT) ofd = creat(name, (mode_t)mode);
  if (how | READ_VERBOSE && (ofd > 0)) puts(name);
  //and then the file
  memset(toybuf, 0, sizeof(toybuf));
  xread(fd, toybuf, nsize);
}

void cpio_main(void)
{
  switch (toys.optflags & (FLAG_i | FLAG_o | FLAG_t)) {
    case FLAG_o:
      loopfiles_stdin(write_cpio_call);
      struct stat fake;
      stat("/dev/null", &fake);
      write_cpio_member(open("/dev/null", O_RDONLY), "TRAILER!!!", fake);
      write(1, toybuf, 4096);
      break;
    case FLAG_t:
      read_cpio_member(1, READ_VERBOSE);
      break;
  default: 
  error_exit("Bad mix of flags");
  }
}
