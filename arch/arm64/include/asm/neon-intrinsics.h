#ifndef _NEON_INTRINSICS_H
#define _NEON_INTRINSICS_H

#include <asm-generic/int-ll64.h>

/*
 * For Aarch64, there is some ambiguity in the definition of the types below
 * between the kernel and GCC itself. This is usually not a big deal, but it
 * causes trouble when including GCC's version of 'stdint.h' (this is the file
 * that gets included when you #include <stdint.h> on a -ffreestanding build).
 * As this file also gets included implicitly when including 'arm_neon.h' (the
 * NEON intrinsics support header), we need the following to work around the
 * issue if we want to use NEON intrinsics in the kernel.
 */

#ifdef __INT64_TYPE__
#undef __INT64_TYPE__
#define __INT64_TYPE__		__signed__ long long
#endif

#ifdef __UINT64_TYPE__
#undef __UINT64_TYPE__
#define __UINT64_TYPE__		unsigned long long
#endif

#include <arm_neon.h>

#ifdef CONFIG_CC_IS_CLANG
#pragma clang diagnostic ignored "-Wincompatible-pointer-types"
#endif

#endif /* ! _NEON_INTRINSICS_H */
