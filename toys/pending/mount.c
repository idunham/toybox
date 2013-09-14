/* mount.c - A mount program.
 *
 * Copyright 2012 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2012 Madhur Verma <mad.flexi@gmail.com>  -- Added Support for mounting NFS
 * Copyright 2012 Kyungwan Han <asura321@gamil.com>
 * Reviewed by Jaehyun Yoo <kjhyoo@gmail.com>
 *
 * Not in SUSv4.

USE_MOUNT(NEWTOY(mount, ">2o*t:O:farwv", TOYFLAG_BIN))

config MOUNT
  bool "mount"
  default n
  help
    usage: mount [-arw] [-t type] [-o OPT] [-O OPT] [dev] [dir]

    -a        Mount all filesystems in fstab.
    -f        Dry run.
    -v        Verbose
    -t FSTYPE filesystem type
    -r        Read-only mount
    -w        Read-Write mount(default)
    -o OPT    options
    -O OPT    options, mount only filestsystems with these options (used with -a only)

    OPT:
    loop    loop device
    [a]sync   Writes are [a]synchronous
    [no]atime   Disable/enable updates to inode access times
    [no]diratime  Disable/enable atime updates to directories
    [no]relatime  Disable/enable atime updates relative to modification time
    [no]dev   (Dis)allow use of special device files
    [no]exec  (Dis)allow use of executable files
    [no]suid  (Dis)allow set-user-id-root programs
    [r]shared   Convert [recursively] to a shared subtree
    [r]slave  Convert [recursively] to a slave subtree
    [r]private  Convert [recursively] to a private subtree
    [un]bindable  Make mount point [un]able to be bind mounted
    [r]bind   Bind a file or directory [recursively] to another location
    move    Relocate an existing mount point
    remount   Remount a mounted filesystem, changing flags
    ro/rw     Same as -r/-w

*/

#define FOR_mount
#include "toys.h"

GLOBALS(
  char *O_options;
  char *type;
  struct arg_list *pkt_opt;
  char *o_options;
)

#include <linux/loop.h>
#include <mntent.h>

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#ifndef MS_DIRSYNC
#define MS_DIRSYNC (1 << 7)
#endif

#ifndef MS_UNION
# define MS_UNION       (1 << 8)                                                                                                                                                
#endif

#ifndef MS_REC
#define MS_REC 16384
#endif

#ifndef MS_SILENT
#define MS_SILENT (1 << 15)
#endif

#ifndef MS_UNBINDABLE
#define MS_UNBINDABLE (1 << 17)
#endif

#ifndef MS_PRIVATE
#define MS_PRIVATE (1 << 18)
#endif
#ifndef MS_SLAVE
#define MS_SLAVE (1 << 19)
#endif

#ifndef MS_SHARED
#define MS_SHARED (1 << 20)
#endif

#ifndef MS_RELATIME
#define MS_RELATIME  (1 << 21)
#endif

#ifndef MS_STRICTATIME
#define MS_STRICTATIME  (1 << 24)
#endif

#ifndef MS_NOAUTO
#define MS_NOAUTO (1 << 29)
#endif

#ifndef MS_USER  
#define MS_USER (1 << 28)
#endif

struct mount_opts {
  const char str[16];
  unsigned long rwmask;
  unsigned long rwset;
  unsigned long rwnoset;
};

struct extra_opts {
  char *str;
  char *end;
  int used_size;
  int alloc_size;
};

/*
 * These options define the function of "mount(2)".
 */
#define MS_TYPE  (MS_REMOUNT|MS_BIND|MS_MOVE)


static const struct mount_opts options[] = {
  /* name      mask    set    noset    */
  { "defaults",    0,  0,    0  },
  { "async",    MS_SYNCHRONOUS,  0,    MS_SYNCHRONOUS  },
  { "atime",    MS_NOATIME,  0,    MS_NOATIME  },
  { "bind",    MS_TYPE,  MS_BIND,  0,    },
  { "dev",    MS_NODEV,  0,    MS_NODEV  },
  { "diratime",  MS_NODIRATIME,  0,    MS_NODIRATIME  },
  { "dirsync",  MS_DIRSYNC,  MS_DIRSYNC,  0    },
  { "exec",    MS_NOEXEC,  0,    MS_NOEXEC  },
  { "move",    MS_TYPE,  MS_MOVE,  0    },
  { "mand",    MS_MANDLOCK, MS_MANDLOCK, 0    },
  { "relatime",    MS_RELATIME, MS_RELATIME,  0    },
  { "strictatime", MS_STRICTATIME, MS_STRICTATIME,  0    },
  { "rbind",    MS_TYPE,  MS_BIND|MS_REC,  0,    },
  { "recurse",  MS_REC,    MS_REC,    0    },
  { "remount",  MS_TYPE,  MS_REMOUNT,  0    },
  { "ro",      MS_RDONLY,  MS_RDONLY,  0    },
  { "rw",      MS_RDONLY,  0,    MS_RDONLY  },
  { "suid",    MS_NOSUID,  0,    MS_NOSUID  },
  { "sync",    MS_SYNCHRONOUS,  MS_SYNCHRONOUS,  0    },
  { "shared",    MS_SHARED,  MS_SHARED,  0    },
  { "slave",    MS_SLAVE,  MS_SLAVE,  0    },
  { "private",  MS_PRIVATE,  MS_PRIVATE,  0    },
  { "bindable",  MS_UNBINDABLE,  0, MS_UNBINDABLE  },
  { "rshared",  MS_SHARED,  MS_SHARED|MS_REC,  0    },
  { "rslave",    MS_SLAVE,  MS_SLAVE|MS_REC,  0    },
  { "rprivate",  MS_PRIVATE,  MS_PRIVATE|MS_REC,  0    },
  { "unbindable",  MS_UNBINDABLE,  MS_UNBINDABLE, 0  },
  { "verbose",  MS_SILENT,  MS_SILENT,  0    },
  { "loud",  MS_SILENT,  0, MS_SILENT    },
  { "user",  MS_USER,   MS_USER, 0   },
  { "users",  MS_USER,   MS_USER, 0   },
  { "auto",  MS_NOAUTO,  0,  MS_NOAUTO   },
  { "union",  MS_UNION,  MS_UNION, 0   },
};


static void add_extra_option(struct extra_opts *extra, char *s)
{
  int len = strlen(s);
  int newlen; 

  if (extra->str) len++;      /* +1 for ',' */

  newlen = extra->used_size + len;
  if (newlen >= extra->alloc_size) {
    char *new;
    new = (char *)xrealloc(extra->str, newlen + 1);  /* +1 for NUL */
    extra->str = new;
    extra->end = extra->str + extra->used_size;
    extra->alloc_size = newlen;
  }

  if (extra->used_size) {
    *extra->end = ',';
    extra->end++;
  }
  strcpy(extra->end, s);
  extra->used_size += len;
}

static unsigned long parse_mount_options(char *arg, unsigned long rwflag, struct extra_opts *extra, int* loop, char **loopdev)
{
  char *s, *p;

  if (strcmp(arg, "defaults") == 0) return rwflag;
  else p = arg;

  *loop = 0;
  while ((s = strsep(&p, ",")) != NULL) {
    char *opt = s;
    unsigned int i;
    int res, no = s[0] == 'n' && s[1] == 'o';

    if (no) s += 2;
    if (strncmp(s, "loop=", 5) == 0) {
      *loop = 1;
      *loopdev = xstrdup(s+5);
      continue;
    }

    if (strcmp(s, "loop") == 0) {
      *loop = 1;
      continue;
    }
    for (i = 0, res = 1; i < ARRAY_LEN(options); i++) {
      res = strcmp(s, options[i].str);

      if (res == 0) {
        rwflag &= ~options[i].rwmask;
        if (no) rwflag |= options[i].rwnoset;
        else rwflag |= options[i].rwset;
        break;
      }
    }

    if (res != 0 && s[0])
      add_extra_option(extra, opt);
  }

  return rwflag;
}

static char* get_device_name(char *label, int uuid)
{
  struct stat st;
  char *s = NULL, *ret = NULL;
  struct dirent *entry = NULL;
  char *path = xmsprintf("/dev/disk/by-%s", ((uuid)? "uuid" : "label"));
  DIR *dp = opendir(path);
  if (!dp) perror_exit("opendir '%s'", path);

  while((entry = readdir(dp))) {
    if (!strcmp(label, entry->d_name)) {
      char *fname = xmsprintf("%s/%s", path, entry->d_name);
      if (!lstat(fname, &st) && S_ISLNK(st.st_mode)) {
        errno = 0;
        s = xreadlink(fname);
        if (s) {
          free(fname);
          fname = xmsprintf("%s/%s", path, s);
          free(s);
          ret = xabspath(fname, 1);
        }
      }
      free(fname);
      break;
    }
  } // while
  closedir(dp);
  free(path);
  return ret;
}

static char* resolve_mount_dev(char *dev)
{
  char *tmp = dev;
  if (!strncmp(dev, "UUID=", 5))
    tmp = get_device_name(dev + 5, 1);
  if (!strncmp(dev, "LABEL=", 6))        
    tmp = get_device_name(dev + 6, 0);

  if (!tmp) perror_exit("%s", dev);
  return tmp;
}

static struct extra_opts extra;
static unsigned long rwflag;

/*
 * Set the loop device on a given file/device at the supplied offset
 */
static int set_loop(char **loopdev, char *file, off_t offset)
{
  int file_fd, device_fd;
  struct loop_info info;
  int rc = -1, n;
  char *tmploop = NULL;

  file_fd = open(file, O_RDWR);
  if (file_fd < 0) {
    file_fd = open(file, O_RDONLY);
    if (file_fd < 0 ) {
      perror_msg("%s: open backing file failed",file);
      return 1;
    }
  }
  for(n = 0; rc && n < 1048576; n++) {
    if (loopdev && *loopdev) tmploop = *loopdev;
    else tmploop = xmsprintf("/dev/loop%d",n);
    device_fd = open(tmploop, O_RDWR);
    if (device_fd < 0) {
      device_fd = open(tmploop, O_RDONLY);
      if (device_fd < 0) {
        perror_msg("open loop device failed");
        xclose(file_fd);
        return 1;
      }
    }
    if ((rc = ioctl(device_fd, LOOP_GET_STATUS, &info)) && errno == ENXIO) { //device is free;
      if (ioctl(device_fd, LOOP_SET_FD, file_fd) ==0) {
        memset(&info, 0, sizeof(info));
        strncpy(info.lo_name, file, LO_NAME_SIZE);
        info.lo_name[LO_NAME_SIZE - 1] = '\0';
        info.lo_offset = offset;
        if (!ioctl(device_fd, LOOP_SET_STATUS, &info)) rc = 0;
        else ioctl(device_fd, LOOP_CLR_FD, 0);
      }
    } else if (strcmp(file, (char *)info.lo_name) != 0
        || offset != info.lo_offset
        ) {
      rc = -1;
    }
    xclose(device_fd);
    if (*loopdev) break;
  }

  xclose(file_fd);
  if (!rc && loopdev && !*loopdev)
    *loopdev = xstrdup(tmploop);
  return rc;
}

static int do_mount(char *dev, char *dir, char *type, unsigned long rwflag, void *data, int loop,
     char *loopdev)
{
  char *s;
  int error = 0;
  FILE* fp = NULL;
  char fstype[80] = {'\0'}, mytype[80] = {'\0'};

  if (toys.optflags & FLAG_f) {
    if (toys.optflags & FLAG_v)
      xprintf("mount: %s on %s\n", dev, dir);
    return 0; //Dry run
  }
  
  dev = resolve_mount_dev(dev);
  if (loop) {
    if (set_loop(&loopdev, dev, 0)) return 1;
    dev = loopdev;
  }

  if (type == NULL) {//if type not specified on command line
    fp = fopen("/proc/filesystems", "r");
    if (fp) {
      while(fgets(fstype, 80, fp)) {
        if (!strstr(fstype, "nodev")) {
          int sz = 0;
          sscanf(fstype, "%s", mytype);
          if (!type) type = xstrdup("");
          sz = strlen(type) + strlen(mytype)  + 2;
          type = (char *)xrealloc(type, sz); //+2 .. 1 for ',' and 1 for '\0'
          strcat(type, mytype);
          strcat(type, ",");
        }

        strcpy(fstype, "");
      }
      fclose(fp);
      fp = NULL;
    }
    if (type) type[strlen(type) - 1] = '\0';
  }

  while ((s = strsep(&type, ",")) != NULL) {
retry:
    if (rwflag & (MS_TYPE| MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE)) s = NULL;
    if (mount(dev, dir, s, rwflag, data) == -1) {
      error = errno;

      if (error == ENODEV || error == EINVAL)
        continue;

      if (error == EACCES &&
          (rwflag & (MS_REMOUNT|MS_RDONLY)) == 0) {
        rwflag |= MS_RDONLY;
        goto retry;
      }
    }
    else {
      error = 0;
      break;
    }
  }

  if (error) {
    errno = error;
    perror_msg("mount %s on %s failed", dev, dir);
    return 255;
  }

  return 0;
}



static void free_mnt(struct mntent *tmp)
{
  free(tmp->mnt_fsname);
  free(tmp->mnt_dir);
  free(tmp->mnt_opts);
  free(tmp->mnt_type);
  free(tmp);
}

static struct mntent* get_mounts_dev_dir(char *arg, char **dev, char **dir, int check_fstab)
{
  FILE *f;
  char *abs_name = NULL;
  struct mntent *mnt = NULL, *tmp = NULL;
  char *file = (check_fstab)? "/etc/fstab" : "/proc/mounts";

  arg = resolve_mount_dev(arg);
  abs_name = xabspath(arg, 1);

  f = setmntent(file, "r");
  if (!f) perror_exit("%s", file);

  while((mnt = getmntent(f)) != NULL) {
    if (abs_name) {
      if ((strcmp(arg, mnt->mnt_fsname) != 0)
          && (strcmp(arg, mnt->mnt_dir) != 0)
          && (strcmp(abs_name, mnt->mnt_fsname) != 0)
          && (strcmp(abs_name, mnt->mnt_dir) != 0))
        continue;
    } else {
      if ((strcmp(arg, mnt->mnt_fsname) != 0)
          && (strcmp(arg, mnt->mnt_dir) != 0))
        continue;
    }

    if (tmp) {
      free(*dev);
      free(*dir);
      free_mnt(tmp);
      *dev = *dir = NULL;
      tmp = NULL;
    }
    *dev = xstrdup(mnt->mnt_fsname);
    *dir = xstrdup(mnt->mnt_dir);
    tmp = xmalloc(sizeof(struct mntent));
    tmp->mnt_fsname = xstrdup(mnt->mnt_fsname);
    tmp->mnt_dir = xstrdup(mnt->mnt_dir);
    tmp->mnt_type = xstrdup(mnt->mnt_type);
    tmp->mnt_opts = xstrdup(mnt->mnt_opts);
  }

  endmntent(f);
  f = NULL;
  return tmp;
}

static int get_optskip(char *opt, char* mntopt)
{
  char *s, *p, *p_list;
  int skip = 0, match =0;
  while((s=strsep(&opt,",")) != NULL) {
    p_list = xstrdup(mntopt);
    while((p = strsep(&p_list, ",")) != NULL) {
      if (strncmp(s, "no", 2) == 0) {
        if (strcmp(s+2, p) == 0) {
          skip = 1;
          match = 1;
          break;
        } else skip = 0;
      } else {
        if (strcmp(s, p) == 0) {
          skip = 0;
          match = 1;
          break;
        }
        else skip = 1;
      }
    }
    if (match == 1) break;
  }
  return skip;
}

static int get_typeskip(char *type, char *mnt_type)
{
  char *s;
  int skip_mount = 0;
  while ((s = strsep(&type, ","))) {  
    if (!strncmp(s, "no", 2)) {
      if (!strcmp(s+2, mnt_type)) {
        skip_mount = 1;
        break;
      }
      else skip_mount = 0;
    }
    else {
      if (!strcmp(s, mnt_type)) {
        skip_mount = 0;
        break;
      }
      else skip_mount = 1;
    }
  }
  return skip_mount;
}

static int fs_mounted(struct mntent *mnt)
{
  struct mtab_list *mntlist, *mnts;
  struct stat st;
  int block = 0;
  if (stat(mnt->mnt_dir, &st)) return 0;
  if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) block = 1;
  
  mnts = xgetmountlist("/proc/mounts");
  for ( mntlist = mnts; mntlist; mntlist = mntlist->next) {
    if (!strcmp(mnt->mnt_dir, mntlist->dir)
        || !strcmp(mnt->mnt_fsname, mntlist->device)){ /* String match. */
      return 1;
    }
    if (!block) continue;

    if (mntlist->stat.st_dev == st.st_dev) return 1;
  }
  return 0;
}

static int print_mounts()
{
  FILE* f;
  int type_skip = 0;
  struct mntent mtpair[2];
  char *type = NULL;

  f = setmntent("/proc/mounts", "r");
  if (!f) {
    error_msg("could not open /proc/mounts");
    return -1;
  }

  while (getmntent_r(f, &mtpair[0], toybuf, sizeof(toybuf))) {
    if (toys.optflags & FLAG_t) {
      type = xstrdup(TT.type);
      type_skip = get_typeskip(type, mtpair->mnt_type);
    }
    if (!type_skip)
      printf("%s on %s type %s (%s)\n", mtpair->mnt_fsname,
          mtpair->mnt_dir, mtpair->mnt_type,
          mtpair->mnt_opts);
  }          
  endmntent(f);   
  return 0;
}

void mount_main(void)
{
  char *type = NULL, *opts, *mntopts;
  char *dev = NULL, *device = NULL;
  char *dir = NULL, *direc = NULL;
  int loop = 0, free_dev = 0, free_direc = 0;
  char *loopdev = NULL;

  FILE* fs_ptr;
  int type_skip = 0, opt_skip = 0;
  struct mntent* mnt = NULL, *mt = NULL;

  rwflag = 0;//MS_SILENT;

  // mount with no arguments is equivalent to "cat /proc/mounts"
  if (!toys.optc && (!toys.optflags || !(toys.optflags & FLAG_a))) {
    print_mounts();
    return;
  }
  if (toys.optflags & FLAG_O && !(toys.optflags & FLAG_a))
    perror_exit("-O option is used along with -a option only\n");
  if (toys.optflags & FLAG_t) {
    type = TT.type;
    if (!strcmp(type, "auto")) type = NULL;
  }

  if (toys.optflags & FLAG_o) {
    TT.o_options = xstrdup("");
    for( ;TT.pkt_opt; TT.pkt_opt = TT.pkt_opt->next) {
      TT.o_options = xrealloc(TT.o_options, strlen(TT.o_options) 
          + strlen(TT.pkt_opt->arg) + 2); //1 for ',' + NULL
      strcat(TT.o_options, TT.pkt_opt->arg);
      if (TT.pkt_opt->next) strcat(TT.o_options, ",");
    }
    rwflag = parse_mount_options(xstrdup(TT.o_options), rwflag, &extra, &loop, &loopdev);
  }
  
  if (toys.optflags & FLAG_r) rwflag |= MS_RDONLY;
  if (toys.optflags & FLAG_w) rwflag &= ~MS_RDONLY;

  if (2 == toys.optc) {
    dev = toys.optargs[0];
    dir = toys.optargs[1];
  } else if (1 == toys.optc && rwflag & MS_REMOUNT) {
    if (!(mt = get_mounts_dev_dir(toys.optargs[0], &dev, &dir, 0))) {
      error_msg("Entry not found in /proc/mounts\n");
      return;
    }
  }
  else if (1 == toys.optc) {
    if (!(rwflag & (MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE))) {
      if ((mt = get_mounts_dev_dir(toys.optargs[0], &dev, &dir, 1))) {
        type = mt->mnt_type;
        rwflag = parse_mount_options(xstrdup(mt->mnt_opts), rwflag, &extra, &loop, &loopdev);
        rwflag |= ((TT.o_options) ? parse_mount_options(xstrdup(TT.o_options), rwflag, &extra, &loop, &loopdev) : 0);
      } else {
        error_msg("No entry found for '%s' in /etc/fstab\n", toys.optargs[0]);
        return;
      }
    }
    else {
      dev = "";
      dir = toys.optargs[0];
    }
  }
  else if (toys.optflags & FLAG_a) { //mount -a option
    if (geteuid() != 0) error_exit("Need to be root");

    fs_ptr = setmntent("/etc/fstab", "r");
    if (!fs_ptr) perror_exit("/etc/fstab file is not present\n");

    while((mnt = getmntent(fs_ptr))) {
      if (strstr(mnt->mnt_opts, "noauto"))
        continue;
      if (!strcmp(mnt->mnt_dir, "/") 
          || !strcmp(mnt->mnt_dir, "root")
          || !strcmp(mnt->mnt_type, "swap"))
        continue;
      if (fs_mounted(mnt)) {
        if (toys.optflags & FLAG_v)
          xprintf("according to /proc/mounts, %s is already mounted on %s\n", mnt->mnt_fsname, mnt->mnt_dir);
        continue;
      }
      type_skip = opt_skip = 0;
      memset(&extra, 0, sizeof(extra));

      if (toys.optflags & FLAG_t) {
        type = xstrdup(TT.type);
        type_skip = get_typeskip(type, mnt->mnt_type);
      }
      if (toys.optflags & FLAG_O) {
        opts = xstrdup(TT.O_options);
        mntopts = xstrdup(mnt->mnt_opts);
        opt_skip = get_optskip(opts, mntopts);
      }

      if (!type_skip && !opt_skip) {
        unsigned long flag = parse_mount_options(xstrdup(mnt->mnt_opts), rwflag, &extra, &loop, &loopdev);
        toys.exitval = do_mount(mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type, flag, extra.str, loop, loopdev);
      }
    }
    
    endmntent(fs_ptr);
    toys.exitval = 0;
    return;
  }

  if ((device = realpath(dev, NULL)))
    free_dev = 1;
  else device = dev;
  if ((direc = realpath(dir, NULL)))
    free_direc = 1;
  else direc = dir;

  toys.exitval = do_mount(device, direc, type, rwflag, extra.str, loop, loopdev);

  if (free_dev) {
    free(device);
    device = NULL;
  }
  if (free_direc) {
    free(direc);
    direc = NULL;
  }
}

