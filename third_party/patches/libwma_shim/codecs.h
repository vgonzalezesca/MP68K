/* mp368k: sustituto mínimo de codecs.h de Rockbox para compilar libwma sin el resto del entorno de códecs. */
#ifndef MP368K_SHIM_CODECS_H
#define MP368K_SHIM_CODECS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define ROCKBOX_BIG_ENDIAN 1
#else
#  define ROCKBOX_LITTLE_ENDIAN 1
#endif
#define IBSS_ATTR
#define ICODE_ATTR
#define ICONST_ATTR
#define MEM_ALIGN_ATTR
#define DEBUGF(...) ((void)0)
#define LOGF(...) ((void)0)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define swap32(x) __builtin_bswap32(x)
#define swap16(x) __builtin_bswap16(x)
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#endif
