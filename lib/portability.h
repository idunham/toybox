// Workarounds for horrible build environment idiosyncrasies.

// Instead of polluting the code with strange #ifdefs to work around bugs
// in specific compiler, library, or OS versions, localize all that here
// and in portability.c

// The tendency of gcc to produce stupid warnings continues with
// warn_unused_result, which warns about things like ignoring the return code
// of nice(2) (which is completely useless since -1 is a legitimate return
// value on success and even the man page tells you to use errno instead).

// This makes it stop.

#undef _FORTIFY_SOURCE

// For musl
#define _ALL_SOURCE

// Test for gcc (using compiler builtin #define)

#ifdef __GNUC__
#define noreturn	__attribute__((noreturn))
#else
#define noreturn
#endif

// Always use long file support.
#define _FILE_OFFSET_BITS 64

// This isn't in the spec, but it's how we determine what libc we're using.

#include <features.h>

// Various constants old build environments might not have even if kernel does

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif

#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif

#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x200
#endif

// We don't define GNU_dammit because we're not part of the gnu project, and
// don't want to get any FSF on us. Unfortunately glibc (gnu libc)
// won't give us Linux syscall wrappers without claiming to be part of the
// gnu project (because Stallman's "GNU owns Linux" revisionist history
// crusade includes the kernel, even though Linux was inspired by Minix).

// We use most non-posix Linux syscalls directly through the syscall() wrapper,
// but even many posix-2008 functions aren't provided by glibc unless you
// claim it's in the name of Gnu.

#if defined(__GLIBC__)
// "Function prototypes shall be provided." but aren't.
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html
char *crypt(const char *key, const char *salt);

// see http://pubs.opengroup.org/onlinepubs/9699919799/functions/strptime.html
#include <time.h>
char *strptime(const char *buf, const char *format, struct tm *tm);

// uClibc pretends to be glibc and copied a lot of its bugs, but has a few more
#if defined(__UCLIBC__)
#include <unistd.h>
#include <stdio.h>
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream);

// When building under obsolete glibc (Ubuntu 8.04-ish), hold its hand a bit.
#elif __GLIBC__ == 2 && __GLIBC_MINOR__ < 10
#define fstatat fstatat64
int fstatat64(int dirfd, const char *pathname, void *buf, int flags);
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
char *stpcpy(char *dest, const char *src);
#include <sys/stat.h>
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);
int openat(int dirfd, const char *pathname, int flags, ...);
#include <dirent.h>
DIR *fdopendir(int fd);
#include <unistd.h>
int fchownat(int dirfd, const char *pathname,
                    uid_t owner, gid_t group, int flags);
int isblank(int c);
int unlinkat(int dirfd, const char *pathname, int flags);
#include <stdio.h>
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream);

// Straight from posix-2008, things old glibc had but didn't prototype

int faccessat(int fd, const char *path, int amode, int flag);
int linkat(int fd1, const char *path1, int fd2, const char *path2, int flag);
int mkdirat(int fd, const char *path, mode_t mode);
int symlinkat(const char *path1, int fd, const char *path2);
int mknodat(int fd, const char *path, mode_t mode, dev_t dev);
#include <sys/time.h>
int futimens(int fd, const struct timespec times[2]);
int utimensat(int fd, const char *path, const struct timespec times[2], int flag);

#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif
#endif

#endif

// Work out how to do endianness

#ifndef __APPLE__
#include <byteswap.h>
#include <endian.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

int clearenv(void);
#else

#ifdef __BIG_ENDIAN__
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

#endif

#if IS_BIG_ENDIAN
#define IS_LITTLE_ENDIAN 0
#define SWAP_BE16(x) (x)
#define SWAP_BE32(x) (x)
#define SWAP_BE64(x) (x)
#define SWAP_LE16(x) bswap_16(x)
#define SWAP_LE32(x) bswap_32(x)
#define SWAP_LE64(x) bswap_64(x)
#else
#define IS_LITTLE_ENDIAN 1
#define SWAP_BE16(x) bswap_16(x)
#define SWAP_BE32(x) bswap_32(x)
#define SWAP_BE64(x) bswap_64(x)
#define SWAP_LE16(x) (x)
#define SWAP_LE32(x) (x)
#define SWAP_LE64(x) (x)
#endif

#if defined(__APPLE__) || defined(__ANDROID__) || (defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ < 10)
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

// Linux headers not listed by POSIX or LSB
#include <shadow.h>
#include <sys/mount.h>
#include <sys/swap.h>

// Some systems don't define O_NOFOLLOW, and it varies by architecture, so...
#include <fcntl.h>
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
