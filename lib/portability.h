// The tendency of gcc to produce stupid warnings continues with
// warn_unsed_result, which warns about things like ignoring the return code
// of nice(2) (which is completely useless since -1 is a legitimate return
// value on success and even the man page tells you to use errno instead).

// This makes it stop.

#undef _FORTIFY_SOURCE

#define _FILE_OFFSET_BITS 64

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 600
#define _BSD_SOURCE
#define _SVID_SOURCE

#include <stdio.h>
#define fdprintf(...) dprintf(__VA_ARGS__)

#ifdef __GNUC__
#define noreturn	__attribute__((noreturn))
#else
#define noreturn
#endif

#ifndef __APPLE__
#include <byteswap.h>
#include <endian.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

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

// Some versions of gcc produce spurious "may be uninitialized" warnings in
// cases where it provably can't happen.  Unfortunately, although this warning
// is calculated and produced separately from the "is definitely used
// uninitialized" warnings, there's no way to turn off the broken spurious "may
// be" warnings without also turning off the non-broken "is" warnings.

#if CFG_TOYBOX_DEBUG
#define GCC_BUG =0
#else
#define GCC_BUG
#endif
