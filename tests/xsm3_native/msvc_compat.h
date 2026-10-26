#pragma once
#include <stdlib.h>
#include <stdint.h>
#ifdef _MSC_VER
#define __attribute__(x)
#define __BYTE_ORDER__ 1234
#define __ORDER_BIG_ENDIAN__ 4321
#define __GNUC__ 1
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define __builtin_bswap64 _byteswap_uint64
#endif
