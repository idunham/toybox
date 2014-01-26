#include "toys.h"

// 0 if we end up with the path existing as a directory; 1 otherwise
// flags is a bitwise OR of 1 for "recurse", 2 for "verbose"
int r_mkdir(char *dir, mode_t mode, int flags)
{
  struct stat buf;
  char *s;

  // mkdir -p one/two/three is not an error if the path already exists,
  // but is if "three" is a file.  The others we dereference and catch
  // not-a-directory along the way, but the last one we must explicitly
  // test for. Might as well do it up front.

  if (!stat(dir, &buf) && !S_ISDIR(buf.st_mode)) {
    errno = EEXIST;
    return 1;
  }

  for (s=dir; ; s++) {
    char save=0;
    mode_t lmode = mode;
    //mode_t mode = 0777&~toys.old_umask;

    // Skip leading / of absolute paths.
    if (s!=dir && *s == '/' && (flags & 1)) {
      save = *s;
      *s = 0;
    } else if (*s) continue;

    // Use the mode from the -m option only for the last directory.
    if (save == '/') lmode |= 0300;

    if (mkdir(dir, lmode)) {
      if (!(flags & 1) || errno != EEXIST) return 1;
    } else if (flags & 2)
      fprintf(stderr, "%s: created directory '%s'\n", toys.which->name, dir);
    
    if (!(*s = save)) break;
  }

  return 0;
}
