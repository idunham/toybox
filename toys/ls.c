/* vi: set sw=4 ts=4:
 *
 * ls.c - list files
 *
 * Copyright 2012 Andre Renaud <andre@bluewatersys.com>
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ls.html

USE_LS(NEWTOY(ls, "ACFHLRSacdfiklmnpqrstux1", TOYFLAG_BIN))

config LS
	bool "ls"
	default y
	help
	  usage: ls [-ACFHLRSacdfiklmnpqrstux1] [directory...]
	  list files

          -1    list one file per line
          -a    list all files
	  -A	list all files except . and ..
          -F    append a character as a file type indicator
          -l    show full details for each file
*/

#include "toys.h"

#define FLAG_1 (1<<0)
//#define FLAG_x (1<<1)
//#define FLAG_u (1<<2)
//#define FLAG_t (1<<3)
//#define FLAG_s (1<<4)
//#define FLAG_r (1<<5)
//#define FLAG_q (1<<6)
#define FLAG_p (1<<7)
//#define FLAG_n (1<<8)
#define FLAG_m (1<<9)
#define FLAG_l (1<<10)
//#define FLAG_k (1<<11)
#define FLAG_i (1<<12)
#define FLAG_f (1<<13)
#define FLAG_d (1<<14)
//#define FLAG_c (1<<15)
#define FLAG_a (1<<16)
//#define FLAG_S (1<<17)
#define FLAG_R (1<<18)
//#define FLAG_L (1<<19)
//#define FLAG_H (1<<20)
#define FLAG_F (1<<21)
//#define FLAG_C (1<<21)
#define FLAG_A (1<<22)

// test sst output (suid/sticky in ls flaglist)

// ls -lR starts .: then ./subdir:

DEFINE_GLOBALS(
  struct dirtree *files;

  unsigned width;
  int again;
)

#define TT this.ls

void dlist_to_dirtree(struct dirtree *parent)
{
    // Turn double_list into dirtree
    struct dirtree *dt = parent->child;
    if (dt) {
        dt->parent->next = NULL;
        while (dt) {
            dt->parent = parent;
            dt = dt->next;
        }
    }
}

static char endtype(struct stat *st)
{
    mode_t mode = st->st_mode;
    if ((toys.optflags&(FLAG_F|FLAG_p)) && S_ISDIR(mode)) return '/';
    if (toys.optflags & FLAG_F) {
        if (S_ISLNK(mode) && !(toys.optflags & FLAG_F)) return '@';
        if (S_ISREG(mode) && (mode&0111)) return '*';
        if (S_ISFIFO(mode)) return '|';
        if (S_ISSOCK(mode)) return '=';
    }
    return 0;
}

static char *getusername(uid_t uid)
{
  struct passwd *pw = getpwuid(uid);
  return pw ? pw->pw_name : utoa(uid);
}

static char *getgroupname(gid_t gid)
{
  struct group *gr = getgrgid(gid);
  return gr ? gr->gr_name : utoa(gid);
}

// Figure out size of printable entry fields for display indent/wrap

static void entrylen(struct dirtree *dt, unsigned *len)
{
    struct stat *st = &(dt->st);
    unsigned flags = toys.optflags;

    *len = strlen(dt->name);
    if (endtype(st)) ++*len;
    if (flags & FLAG_m) ++*len;

    if (flags & FLAG_i) *len += (len[1] = numlen(st->st_ino));
    if (flags & FLAG_l) {
        len[2] = numlen(st->st_nlink);
        len[3] = strlen(getusername(st->st_uid));
        len[4] = strlen(getgroupname(st->st_gid));
        len[5] = numlen(st->st_size);
    }
}

static int compare(void *a, void *b)
{
    struct dirtree *dta = *(struct dirtree **)a;
    struct dirtree *dtb = *(struct dirtree **)b;

// TODO handle flags
    return strcmp(dta->name, dtb->name);
}

// callback from dirtree_recurse() determining how to handle this entry.

static int filter(struct dirtree *new)
{
    int flags = toys.optflags;

    // TODO should -1f print here to handle enormous dirs without runing
    // out of mem?

    if (flags & FLAG_a) return DIRTREE_NORECURSE;
    if (!(flags & FLAG_A) && new->name[0]=='.')
        return DIRTREE_NOSAVE|DIRTREE_NORECURSE;

    return dirtree_isdotdot(new)|DIRTREE_NORECURSE;
}

// Display a list of dirtree entries, according to current format
// Output types -1, -l, -C, or stream

static void listfiles(struct dirtree *indir)
{
    struct dirtree *dt, **sort = 0;
    unsigned long dtlen = 0, ul = 0;
    unsigned width, flags = toys.optflags, totals[6], len[6];
    int showdirs = 1;

    // Figure out if we should show directories and current directory name
    if (indir == TT.files) showdirs = (flags & (FLAG_d|FLAG_R));
    if (indir != TT.files || (indir->parent && (flags & FLAG_R))) {
        char *path = dirtree_path(indir, 0);

        if (TT.again++) xputc('\n');
        xprintf("%s:\n", path);
        free(path);
    }

    // Copy linked list to array and sort it. Directories go in array because
    // we visit them in sorted order.

    for (;;) {
        for (dt = indir->child; dt; dt = dt->next) {
            if (sort) sort[dtlen] = dt;
            dtlen++;
        }
        if (sort) break;
        sort = xmalloc(dtlen * sizeof(void *));
        dtlen = 0;
        continue;
    }

    // This is wrong, should be blocks used not file count.
    if (indir->parent && (flags & FLAG_l)) xprintf("total %lu\n", dtlen);

    if (!(flags & FLAG_f)) qsort(sort, dtlen, sizeof(void *), (void *)compare);

    // Find largest entry in each field for everything but -1

    memset(totals, 0, 6*sizeof(unsigned));
    if ((flags & (FLAG_1|FLAG_l)) != FLAG_1) {
        for (ul = 0; ul<dtlen; ul++) {
            entrylen(sort[ul], len);
            if (flags & FLAG_l) {
                for (width=0; width<6; width++)
                    if (len[width] > totals[width]) totals[width] = len[width];
//TODO            } else if (flags & FLAG_C) {
            } else if (*len > *totals) *totals = *len;
        }
    }

    // Loop through again to produce output.
    width = 0;
    memset(toybuf, ' ', 256);
    for (ul = 0; ul<dtlen; ul++) {
        struct stat *st = &(sort[ul]->st);
        mode_t mode = st->st_mode;
        char et = endtype(st);

        if (S_ISDIR(mode) && !showdirs) continue;
        entrylen(sort[ul], len);

        if (ul) {
            if (toys.optflags & FLAG_m) xputc(',');
            if ((flags & FLAG_1) || width+1+*len > TT.width) {
                xputc('\n');
                width = 0;
            } else {
                xputc(' ');
                width++;
            }
        }
        width += *len;

        if (flags & FLAG_i)
            xprintf("% *lu ", len[1], (unsigned long)st->st_ino);

        if (flags & FLAG_l) {
            struct tm *tm;
            char perm[11], thyme[64], c, d;
            int i, bit;

            perm[10]=0;
            for (i=0; i<9; i++) {
                bit = mode & (1<<i);
                c = i%3;
                if (!c && (mode & (1<<((d=i/3)+9)))) {
                    c = "tss"[d];
                    if (!bit) c &= 0x20;
                } else c = bit ? "xwr"[c] : '-';
                perm[9-i] = c;
            }

            if (S_ISDIR(mode)) c = 'd';
            else if (S_ISBLK(mode)) c = 'b';
            else if (S_ISCHR(mode)) c = 'c';
            else if (S_ISLNK(mode)) c = 'l';
            else if (S_ISFIFO(mode)) c = 'p';
            else if (S_ISSOCK(mode)) c = 's';
            else c = '-';
            *perm = c;

            tm = localtime(&(st->st_mtime));
            strftime(thyme, sizeof(thyme), "%F %H:%M", tm);

            xprintf("%s% *d %s%s%s%s% *d %s ", perm, totals[2]+1, st->st_nlink,
                    getusername(st->st_uid), toybuf+255-(totals[3]-len[3]),
                    getgroupname(st->st_gid), toybuf+256-(totals[4]-len[4]),
                    totals[5]+1, st->st_size, thyme);
        }

        xprintf("%s", sort[ul]->name);
        if ((flags & FLAG_l) && S_ISLNK(mode))
            xprintf(" -> %s", sort[ul]->symlink);

        if (et) xputc(et);
    }

    if (width) xputc('\n');

    for (ul = 0; ul<dtlen; free(sort[ul++])) {
// TODO follow symlinks when?
        if (!S_ISDIR(sort[ul]->st.st_mode) || dirtree_isdotdot(sort[ul]))
            continue;
        if (indir == TT.files || (flags & FLAG_R)) {
            int fd = openat(indir->data, sort[ul]->name, 0);

            sort[ul]->data = dup(fd);
            dirtree_recurse(sort[ul], filter);
            sort[ul]->data = fd;
            listfiles(sort[ul]);
        }
    }
    free(sort);
    if (indir->data != AT_FDCWD) close(indir->data);
}

void ls_main(void)
{
    char **s, *noargs[] = {".", 0};
    struct dirtree *dt;

    // Do we have an implied -1
    if (!isatty(1) || (toys.optflags&FLAG_l)) toys.optflags |= FLAG_1;
    else {
        TT.width = 80;
        terminal_size(&TT.width, NULL);
    }

    // Iterate through command line arguments, collecting directories and files.
    // Non-absolute paths are relative to current directory.
    TT.files = dirtree_add_node(0, 0);
    for (s = *toys.optargs ? toys.optargs : noargs; *s; s++) {
        dt = dirtree_add_node(AT_FDCWD, *s);

        if (!dt) {
            toys.exitval = 1;
            continue;
        }

        // Typecast means double_list->prev temporarirly goes in dirtree->parent
        dlist_add_nomalloc((struct double_list **)&TT.files->child,
                           (struct double_list *)dt);
    }

    if (!TT.files->child) return;

    // Turn double_list into dirtree
    dlist_to_dirtree(TT.files);

    // Special case a single directory argument: silently descend into it.
    dt = TT.files->child;

    if (S_ISDIR(dt->st.st_mode) && !dt->next && !(toys.optflags&FLAG_d)) {
        int fd = open(dt->name, 0);
        TT.files = dt;
        dt->data = dup(fd);
        dirtree_recurse(dt, filter);
        dt->data = fd;
    } else TT.files->data = AT_FDCWD;

    // Display the files we collected
    listfiles(TT.files);

    if (CFG_TOYBOX_FREE) {
        free(TT.files->parent);
        free(TT.files);
    }
}
