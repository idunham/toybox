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
  default y
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

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <arpa/inet.h>
#include <netdb.h>
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

static int daemonize(int flags) {
  int fd;                 

  fd = open("/dev/null", O_RDWR);
  if (fd < 0) fd = open("/", O_RDONLY, 0666);

  pid_t pid = fork();   

  if(pid < 0){          
    printf("DAEMON: fail to fork");
    return -1;          
  }                     
  if (pid) exit(EXIT_SUCCESS); 

  setsid();             
  dup2(fd, 0);          
  dup2(fd, 1);          
  dup2(fd, 2);          
  while (fd > 2) {        
    close(fd--);          
  }
  return 0;
} 

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

/* NFS Mount implementation ..... */

#define NFSD   100003
#define NFSV  0

#define MOUNTD  100005
#define MOUNTV  0
#define MOUNTPORT 635

#define MOUNTP_NULL    0
#define MOUNTP_MNT    1
#define MOUNTP_DUMP    2
#define MOUNTP_UMNT    3
#define MOUNTP_UMNTALL  4
#define MOUNTP_EXPORT  5
#define MOUNTP_EXPALL  6
#define MOUNTP_PATHCONF 7


#define MOUNTP3_NULL  0
#define MOUNTP3_MNT    1
#define MOUNTP3_DUMP  2
#define MOUNTP3_UMNT  3
#define MOUNTP3_UMNTALL  4
#define MOUNTP3_EXPORT  5

#define MNTPATHLEN    1024
#define FHSIZE      32
#define FHSIZE3      64

typedef char fhandle[FHSIZE];

typedef struct {
  unsigned int fhandle3_len;
  char *fhandle3_val;
} fhandle3;

typedef enum mountstat3 {
  MNT_OK = 0,
  MNT3ERR_PERM = 1,
  MNT3ERR_NOENT = 2,
  MNT3ERR_IO = 5,
  MNT3ERR_ACCES = 13,
  MNT3ERR_NOTDIR = 20,
  MNT3ERR_INVAL = 22,
  MNT3ERR_NAMETOOLONG = 63,
  MNT3ERR_NOTSUPP = 10004,
  MNT3ERR_SERVERFAULT = 10006,
} mountstat3;

typedef struct fhstatus {
  unsigned int fhs_status;
  union {
    fhandle fhs_fhandle;
  } fhstatus_u;
} fhstatus;

typedef struct mountres3_ok {
  fhandle3 fhandle;
  struct {
    unsigned int auth_flavours_len;
    char *auth_flavours_val;
  } auth_flavours;
} mountres3_ok;

typedef struct mountres3 {
  mountstat3 fhs_status;
  union {
    mountres3_ok mountinfo;
  } mountres3_u;
} mountres3;

static bool_t decode_fhstatus(XDR *xdrs, fhstatus *objp)
{
  if (!xdr_u_int(xdrs, &objp->fhs_status))
    return FALSE;
  if (objp->fhs_status == 0)
    return xdr_opaque(xdrs, objp->fhstatus_u.fhs_fhandle, FHSIZE);
  return TRUE;
}

static bool_t encode_dirpath(XDR *xdrs, char **objp) 
{
  return xdr_string(xdrs, objp, MNTPATHLEN);
}

static bool_t decode_mountres(XDR *xdrs, mountres3 *objp) 
{
  if (!xdr_enum(xdrs, (enum_t *) &objp->fhs_status))
    return FALSE;
  if (objp->fhs_status == MNT_OK) {
    if (!xdr_bytes(xdrs,
        (char **) &objp->mountres3_u.mountinfo.fhandle.fhandle3_val,
        (unsigned int *) &objp->mountres3_u.mountinfo.fhandle.fhandle3_len,
        FHSIZE3))
      return FALSE;
    return xdr_array(xdrs,
        &(objp->mountres3_u.mountinfo.auth_flavours.auth_flavours_val),
        &(objp->mountres3_u.mountinfo.auth_flavours.auth_flavours_len),
        ~0, sizeof(int), (xdrproc_t) xdr_int);

  }
  return TRUE;
}

enum {
  NFS_SOFT      = (1 << 0),
  NFS_INTR      = (1 << 1),
  NFS_SECURE    = (1 << 2),
  NFS_POSIX     = (1 << 3),
  NFS_NOCTO     = (1 << 4),
  NFS_NOAC      = (1 << 5),
  NFS_TCP       = (1 << 6),
  NFS_VER3      = (1 << 7),
  NFS_KERBEROS  = (1 << 8),
  NFS_NONLM     = (1 << 9),
  NFS_NORDIRPLUS  = (1 << 14),
};

#define MAX_NFSV ((opt.version >= 4) ? 3 : 2)

static const char  __attribute__((aligned(1))) nfs_optvals[]=
/* 0 */ "rsize\0"
/* 1 */ "wsize\0"
/* 2 */ "timeo\0"
/* 3 */ "retrans\0"
/* 4 */ "acregmin\0"
/* 5 */ "acregmax\0"
/* 6 */ "acdirmin\0"
/* 7 */ "acdirmax\0"
/* 8 */ "actimeo\0"
/* 9 */ "retry\0"
/* 10 */ "port\0"
/* 11 */ "mountport\0"
/* 12 */ "mounthost\0"
/* 13 */ "mountprog\0"
/* 14 */ "mountvers\0"
/* 15 */ "nfsprog\0"
/* 16 */ "nfsvers\0"
/* 17 */ "vers\0"
/* 18 */ "proto\0"
/* 19 */ "namlen\0"
/* 20 */ "addr\0";

static const char __attribute__((aligned(1))) nfs_opts[]=
"bg\0"
"fg\0"
"soft\0"
"hard\0"
"intr\0"
"posix\0"
"cto\0"
"ac\0"
"tcp\0"
"udp\0"
"lock\0"
"rdirplus\0";


typedef struct optval_s {
  char* mountd;
  int daemonized;
  int port;
  int mountport;
  int proto;
  int bg;
  int retry;
  int mountprog;
  int mountvers;
  int nfsprog;
  int nfsvers;
  int tcp;
  int soft;
  int intr;
  int posix;
  int nocto;
  int noac;
  int nordirplus;
  int nolock;
  int version;
} optval;

typedef struct filehdrv2_s{
  char          data[32];
} filehdrv2_t;

typedef struct filehdrv3_s {
  unsigned short      size;
  unsigned char       data[64];
} filehdrv3_t;

typedef struct nfsdata_s {
  int      version;
  int      fd;
  filehdrv2_t  old_root;
  int      flags;
  int      rsize;
  int      wsize;
  int      timeo;
  int      retrans;
  int      acregmin;
  int      acregmax;
  int      acdirmin;
  int      acdirmax;
  struct sockaddr_in  addr;
  char    hostname[256];
  int      namlen;
  unsigned int  bsize;
  filehdrv3_t  root;
} nfsdata_t;

static void rpc_msg(const char *msg) 
{
  int len;
  while (msg[0] == ' ' || msg[0] == ':')
    msg++;
  len = strlen(msg);
  while (len && msg[len - 1] == '\n')
    len--;
  error_msg("%.*s", len, msg);
}

static int getindex(const char *search, char *key) 
{
  int idx =0;
  while (*search) {
    if (strcmp(search, key) == 0)
      return idx;
    search += strlen(search) + 1;
    idx++;
  }
  return -1;
}

static int parse_options(char* opts, optval* optv, nfsdata_t *nfsdata) 
{
  char *opt;
  if (!opts) return 0;

  for (opt = strtok(opts, ","); opt; opt = strtok(NULL, ",")) {
    char *opteq = strchr(opt, '=');
    if (opteq) {
      int val, idx = 0;
      *opteq++ = '\0';

      idx = getindex(nfs_optvals, opt);
      switch (idx) {
        case 12: // "mounthost"
          optv->mountd = xstrndup(opteq, strcspn(opteq, " \t\n\r,"));
          continue;
        case 18: // "proto"
          if (!strncmp(opteq, "tcp", 3))
            optv->tcp = 1;
          else if (!strncmp(opteq, "udp", 3))
            optv->tcp = 0;
          else
            error_msg("Unrecognized Protocol.");
          continue;
        case 20: // "addr" - ignore
          continue;
        case -1: // unknown
          continue;
        default: break;
      }

      val = atoi(opteq);
      switch (idx) {
        case 0: // "rsize"
          nfsdata->rsize = val;
          continue;
        case 1: // "wsize"
          nfsdata->wsize = val;
          continue;
        case 2: // "timeo"
          nfsdata->timeo = val;
          continue;
        case 3: // "retrans"
          nfsdata->retrans = val;
          continue;
        case 4: // "acregmin"
          nfsdata->acregmin = val;
          continue;
        case 5: // "acregmax"
          nfsdata->acregmax = val;
          continue;
        case 6: // "acdirmin"
          nfsdata->acdirmin = val;
          continue;
        case 7: // "acdirmax"
          nfsdata->acdirmax = val;
          continue;
        case 8: // "actimeo"
          nfsdata->acregmin = val;
          nfsdata->acregmax = val;
          nfsdata->acdirmin = val;
          nfsdata->acdirmax = val;
          continue;
        case 9: // "retry"
          optv->retry = val;
          continue;
        case 10: // "port"
          optv->port = val;
          continue;
        case 11: // "mountport"
          optv->mountport = val;
          continue;
        case 13: // "mountprog"
          optv->mountprog = val;
          continue;
        case 14: // "mountvers"
          optv->mountvers = val;
          continue;
        case 15: // "nfsprog"
          optv->nfsprog = val;
          continue;
        case 16: // "nfsvers" /*FALL_THROUGH*/
        case 17: // "vers"
          optv->nfsvers = val;
          continue;
        case 19: // "namlen"
          nfsdata->namlen = val;
          continue;
        default:
          error_exit("unknown nfs mount parameter: %s=%d", opt, val);
          break;
      }
    } else { /* not of the form opt=val */
      int val = 1;
      if (!strncmp(opt, "no", 2)) {
        val = 0;
        opt += 2;
      }

      switch (getindex(nfs_opts, opt)) {
        case 0: // "bg"
          optv->bg = val;
          break;
        case 1: // "fg"s
          optv->bg = !val;
          break;
        case 2: // "soft"
          optv->soft = val;
          break;
        case 3: // "hard"
          optv->soft = !val;
          break;
        case 4: // "intr"
          optv->intr = val;
          break;
        case 5: // "posix"
          optv->posix = val;
          break;
        case 6: // "cto"
          optv->nocto = !val;
          break;
        case 7: // "ac"
          optv->noac = !val;
          break;
        case 8: // "tcp"
          optv->tcp = val;
          break;
        case 9: // "udp"
          optv->tcp = !val;
          break;
        case 10: // "lock"
          if (optv->version >= 3) optv->nolock = !val;
          else error_msg("warning: option nolock is not supported");
          break;
        case 11: //rdirplus
          optv->nordirplus = !val;
          break;
        default:
          error_exit("unknown nfs mount option: %s%s", val ? "" : "no",
              opt);
          break;
      }
    }
  }

  return 0;
}

static int do_nfsmount(char *dev, char *dir, unsigned long rwflag, char *opts)
{
  union {
    fhstatus nfsv2;
    mountres3 nfsv3;
  } rpcstatus;

  int ret = -ETIMEDOUT;
  int msock = -1, fsock = -1;
  char *tmp;
  char *hostname, *pathname;
  CLIENT *rpcclient = NULL;
  nfsdata_t nfs_data;
  struct sockaddr_in nfsd;
  struct sockaddr_in mountd;
  struct hostent *hent;

  struct pmap pm_data;
  struct pmaplist *list;
  struct timeval to_total;
  struct timeval to_retry;
  time_t current;
  time_t previous;
  time_t timeout;
  optval opt;

  opt.daemonized = 0;
  opt.mountd = NULL;
  opt.bg = 0;
  opt.version = 4;

  hostname = xstrdup(dev);
  tmp = strchr(hostname, ':');
  pathname = tmp + 1;
  *tmp = '\0';

  memset(&nfsd, 0, sizeof(nfsd));
  nfsd.sin_family = AF_INET;

  if (!inet_aton(hostname, &nfsd.sin_addr)) {
    hent = gethostbyname(hostname);
    if (hent == NULL) perror_exit("Can't resolve Host.");
    if (hent->h_length != (int)sizeof(struct in_addr))
      perror_exit("Only IPv4 Supported.");
    memcpy(&nfsd.sin_addr, hent->h_addr_list[0], sizeof(struct in_addr));
  }

  memcpy(&mountd, &nfsd, sizeof(mountd));
  memset(&nfs_data, 0, sizeof(nfs_data));
  nfs_data.retrans  = 3;
  nfs_data.acregmin = 3;
  nfs_data.acregmax = 60;
  nfs_data.acdirmin = 30;
  nfs_data.acdirmax = 60;
  nfs_data.namlen   = NAME_MAX;

  opt.soft = 0;
  opt.intr = 0;
  opt.posix = 0;
  opt.nocto = 0;
  opt.nolock = 0;
  opt.noac = 0;
  opt.nordirplus = 0;
  opt.retry = 10000;
  opt.tcp = 1;
  opt.mountprog = MOUNTD;
  opt.mountvers = 0;
  opt.port = 0;
  opt.mountport = 0;
  opt.nfsprog = NFSD;
  opt.nfsvers = 0;

  parse_options(opts, &opt, &nfs_data);

  opt.proto = (opt.tcp)? IPPROTO_TCP : IPPROTO_UDP;

  nfs_data.flags = (opt.soft ? NFS_SOFT : 0)
      | (opt.intr ? NFS_INTR : 0)
      | (opt.posix ? NFS_POSIX : 0)
      | (opt.nocto ? NFS_NOCTO : 0)
      | (opt.noac ? NFS_NOAC : 0)
      | (opt.nordirplus ? NFS_NORDIRPLUS : 0);
    if (opt.version >= 2)
      nfs_data.flags |= (opt.tcp ? NFS_TCP : 0);
    if (opt.version >= 3)
      nfs_data.flags |= (opt.nolock ? NFS_NONLM : 0);
    if (opt.nfsvers > MAX_NFSV || opt.mountvers > MAX_NFSV)
      error_exit("NFSv%d not supported", opt.nfsvers);
    if (opt.nfsvers && !opt.mountvers)
      opt.mountvers = (opt.nfsvers < 3) ? 1 : opt.nfsvers;
    if (opt.nfsvers && opt.nfsvers < opt.mountvers)
      opt.mountvers = opt.nfsvers;

    /* Adjust options if none specified */
    if (!nfs_data.timeo) nfs_data.timeo = opt.tcp ? 70 : 7;
    nfs_data.version = opt.version;

    if (opt.mountd) {
    if (opt.mountd[0] >= '0' && opt.mountd[0] <= '9') {
      mountd.sin_family = AF_INET;
      mountd.sin_addr.s_addr = inet_addr(hostname);
    } else {
      hent = gethostbyname(opt.mountd);
      if (hent == NULL) perror_exit("Can't find %s", opt.mountd);
      if (hent->h_length != (int) sizeof(struct in_addr))
        perror_exit("only IPv4 is supported");

      mountd.sin_family = AF_INET;
      memcpy(&mountd.sin_addr, hent->h_addr_list[0],
          sizeof(struct in_addr));
    }
  }

  (void)memset(&pm_data, 0, sizeof(struct pmap));

  to_retry.tv_sec = 3;
  to_retry.tv_usec = 0;
  to_total.tv_sec = 20;
  to_total.tv_usec = 0;

  timeout = time(NULL) + (60 * opt.retry);
  previous = 0;
  current = 30;

recall:
  if (current - previous < 30)
    sleep(30);

  nfsd.sin_port = PMAPPORT;
  list = pmap_getmaps(&nfsd);

  while(list) {
    if (list->pml_map.pm_prog != MOUNTD) {
      list = list->pml_next;
      continue;
    }
    if (opt.mountvers > list->pml_map.pm_vers) {
      list = list->pml_next;
      continue;
    }
    if (opt.mountvers > 2 && list->pml_map.pm_vers != opt.mountvers) {
      list = list->pml_next;
      continue;
    }
    if (opt.mountvers && opt.mountvers < 2 && list->pml_map.pm_vers > 2) {
      list = list->pml_next;
      continue;
    }
    if ((list->pml_map.pm_vers > MAX_NFSV)
      ||( opt.proto && list->pml_map.pm_prot != opt.proto)
      ||( opt.port && list->pml_map.pm_port != opt.port)) {
      list = list->pml_next;
      continue;
    }

    memcpy(&pm_data, &list->pml_map, sizeof(pm_data));
    list = list->pml_next;
  }

  if (!pm_data.pm_vers) pm_data.pm_vers = MOUNTV;
  if (!pm_data.pm_port) pm_data.pm_port = MOUNTPORT;
  if (!pm_data.pm_prot) pm_data.pm_prot = IPPROTO_TCP;

  opt.nfsvers = (pm_data.pm_vers < 2)? 2 : pm_data.pm_vers;

  mountd.sin_port = htons(pm_data.pm_port);
  msock = RPC_ANYSOCK;

  switch (pm_data.pm_prot) {
  case IPPROTO_UDP:
    rpcclient = clntudp_create(&mountd, pm_data.pm_prog, pm_data.pm_vers,
        to_retry, &msock);
    if (rpcclient) break;
    mountd.sin_port = htons(pm_data.pm_port);
    msock = RPC_ANYSOCK;
  case IPPROTO_TCP:
    rpcclient = clnttcp_create(&mountd, pm_data.pm_prog, pm_data.pm_vers,
        &msock, 0, 0);
    if (rpcclient) break;
    mountd.sin_port = htons(pm_data.pm_port);
    msock = RPC_ANYSOCK;
  default:
    rpcclient = NULL;
  }

  if (!rpcclient) {
    if (!opt.daemonized && previous == 0)
      rpc_msg(clnt_spcreateerror(" "));
  } else {
    enum clnt_stat rpcstat;
    rpcclient->cl_auth = authunix_create_default();

    memset(&rpcstatus, 0, sizeof(rpcstatus));

    if (pm_data.pm_vers == 3)
      rpcstat = clnt_call(rpcclient, MOUNTP3_MNT,
          (xdrproc_t) encode_dirpath,
          (caddr_t) &pathname,
          (xdrproc_t) decode_mountres,
          (caddr_t) &rpcstatus,
          to_total);
    else rpcstat = clnt_call(rpcclient, MOUNTP_MNT,
          (xdrproc_t) encode_dirpath,
          (caddr_t) &pathname,
          (xdrproc_t) decode_fhstatus,
          (caddr_t) &rpcstatus,
          to_total);

    if (rpcstat == RPC_SUCCESS) goto kernel;

    if (errno != ECONNREFUSED) {
      rpc_msg(clnt_sperror(rpcclient, " "));
      goto die;
    }

    if (!opt.daemonized && previous == 0)
      rpc_msg(clnt_sperror(rpcclient, " "));
    auth_destroy(rpcclient->cl_auth);
    clnt_destroy(rpcclient);
    rpcclient = NULL;
    close(msock);
    msock = -1;
  }

  if (!opt.bg) goto die;
  if (!opt.daemonized) {
    opt.daemonized = (daemonize(0) == 0) ? 1 : 0;
    if (opt.daemonized <= 0) { /* parent or error */
      ret = -opt.daemonized;
      goto ret;
    }
  }
  previous = current;
  current = time(NULL);
  if (current >= timeout) goto die;
  goto recall;

kernel:
  if (opt.nfsvers == 2) {
    if (rpcstatus.nfsv2.fhs_status != 0) {
      error_msg("%s:%s failed, server error code: %d", hostname,
          pathname, rpcstatus.nfsv2.fhs_status);
      goto die;
    }
    memcpy(nfs_data.root.data, (char *) rpcstatus.nfsv2.fhstatus_u.fhs_fhandle,
        FHSIZE);
    nfs_data.root.size = FHSIZE;
    memcpy(nfs_data.old_root.data, (char *) rpcstatus.nfsv2.fhstatus_u.fhs_fhandle,
        FHSIZE);
  } else {
    fhandle3 *fhandle;
    if (rpcstatus.nfsv3.fhs_status != 0) {
      error_msg("%s:%s failed, server error code : %d", hostname,
          pathname, rpcstatus.nfsv3.fhs_status);
      goto die;
    }
    fhandle = &rpcstatus.nfsv3.mountres3_u.mountinfo.fhandle;
    memset(nfs_data.old_root.data, 0, FHSIZE);
    memset(&nfs_data.root, 0, sizeof(nfs_data.root));
    nfs_data.root.size = fhandle->fhandle3_len;
    memcpy(nfs_data.root.data, (char *) fhandle->fhandle3_val,
        fhandle->fhandle3_len);

    nfs_data.flags |= NFS_VER3;
  }

  /* Create nfs socket for kernel */
  if (opt.tcp) {
    if (opt.version < 3) {
      error_msg("NFS over TCP is not supported");
      goto die;
    }
    fsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  } else fsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fsock < 0) {
    perror_msg("nfs socket creation failed.");
    goto die;
  }
  if (bindresvport(fsock, 0) < 0) {
    perror_msg("nfs bindresvport failed.");
    goto die;
  }
  if (opt.port == 0) {
    nfsd.sin_port = PMAPPORT;
    opt.port = pmap_getport(&nfsd, opt.nfsprog, opt.nfsvers,
          opt.tcp ? IPPROTO_TCP : IPPROTO_UDP);
    if (opt.port == 0)
      opt.port = 2049;   /* NFS port */
  }
  nfsd.sin_port = htons(opt.port);

  nfs_data.fd = fsock;
  memcpy((char *) &nfs_data.addr, (char *) &nfsd, sizeof(nfs_data.addr));
  strncpy(nfs_data.hostname, hostname, sizeof(nfs_data.hostname));

  if (opt.bg) {
    /* We must wait until mount directory is available */
    struct stat statbuf;
    int delay = 1;
    while (stat(dir, &statbuf) == -1) {
      if (!opt.daemonized) {
        opt.daemonized = (daemonize(0) == 0) ? 1 : 0;
        if (opt.daemonized <= 0) {
          ret = -opt.daemonized;
          goto ret;
        }
      }
      sleep(delay);
      delay *= 2;
      if (delay > 30) delay = 30;
    }
  }

  if ((ret = mount(dev, dir, "nfs", rwflag, (void*)&nfs_data)) == -1) {
    perror_msg("%s on %s failed", dev, dir);
    return 255;
  }

  /* Abort */
die: 
  if (msock >= 0) {
    if (rpcclient) {
      auth_destroy(rpcclient->cl_auth);
      clnt_destroy(rpcclient);
    }
    close(msock);
  }
  if (fsock >= 0) close(fsock);

ret: 
   free(hostname);
   free(opt.mountd);
   free(opts);
   hostname = NULL;
   opt.mountd = NULL;
   opts = NULL;
   return ret;
}
/* End of nfs mount implementation */

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


  if ((strchr(dev, ':') != NULL)  && type == NULL)
      type = "nfs";

  if (type && strcmp(type ,"nfs") == 0)
    return do_nfsmount(dev, dir, rwflag, (char*)data);

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
  
  mnts = getmountlist(1);
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

