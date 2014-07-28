#ifndef __ISA_AVAILABILITY__H__
#define __ISA_AVAILABILITY__H__

/*
 * These are defines for the extern "__isa_available" defined in the CRT,
 * which defines the latest instruction set available for use. The compiler
 * backend reads this file to emit vector code for specific microarchitectures.
 *
 * Additional  architectural features are defined for extern "__favor",
 * these defines identify performance features that are enabled in the processor.
 * The compiler backend can use these to enable processor specific optimizations.
 */

enum ISA_AVAILABILITY
{
    __ISA_AVAILABLE_X86   = 0,
    __ISA_AVAILABLE_SSE2  = 1,
    __ISA_AVAILABLE_SSE42 = 2,
    __ISA_AVAILABLE_AVX   = 3,
    __ISA_AVAILABLE_ENFSTRG = 4,
    __ISA_AVAILABLE_AVX2 = 5,

    __ISA_AVAILABLE_ARMNT   = 0,   // minimum Win8 ARM support (but w/o NEON)
    __ISA_AVAILABLE_NEON    = 1,   // support for 128-bit NEON instructions
};

#if defined(_M_IX86)

/* Defines for: "__favor" defined in the CRT */
#define __FAVOR_ATOM    0
#define __FAVOR_ENFSTRG 1 /* Enhanced Fast Strings rep movb/stob */
/* #define reserved     2 */

#elif defined(_M_AMD64)

/* Defines for: "__favor" defined in the CRT */
#define __FAVOR_ATOM    0
#define __FAVOR_ENFSTRG 1 /* Enhanced Fast Strings rep movb/stob */
#define __FAVOR_XMMLOOP 2 /* partially unrolled loop using xmm regs */

#endif

#endif // __ISA_AVAILABILITY__H__
