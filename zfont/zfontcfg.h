/*
 * Copyright (c) 2025 Jiapeng Li <mail@jiapeng.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __ZFCFG_H__
#define __ZFCFG_H__

#ifdef ZF_USR_CFG
#include "zf-usr.h"
#endif

#ifndef ZF_DEBUG
#   define ZF_DEBUG                    0
#endif

#if ZF_DEBUG
#   include <stdio.h>

#   ifndef ZF_LOG
#       define ZF_LOG(x...)                 do { \
            printf(x); printf("\n"); \
        } while(0)
#   endif

#   ifndef ZF_HEXDUMP
#       define ZF_HEXDUMP(x, y)             do { \
            for (int i = 0; i < y; i++) { \
                if (i && i % 16 == 0) printf("\n"); \
                printf("%02X ", ((uint8_t*)x)[i]); \
            } \
            printf("\n"); \
        } while(0)
#   endif

#   ifndef ZF_INTDUMP
#       define ZF_INTDUMP(x, y)             do { \
            for (int i = 0; i < y; i++) { \
                if (i && i % 8 == 0) printf("\n"); \
                printf("%5d ", x[i]); \
            } \
            printf("\n"); \
        } while(0)
#endif

#   ifndef ZF_RGBDUMP
#       define ZF_RGBDUMP(x, y)             do { \
            for (int i = 0; i < y / 3; i++) { \
                if (i && i % 8 == 0) printf("\n"); \
                printf("(%3d,%3d,%3d) ", x[i * 3], x[i * 3 + 1], x[i * 3 + 2]); \
            } \
            printf("\n"); \
        } while(0)
#   endif
#else
#   define ZF_LOG(x...)                do {} while(0)
#   define ZF_HEXDUMP(x, y)            do {} while(0)
#   define ZF_INTDUMP(x, y)            do {} while(0)
#   define ZF_RGBDUMP(x, y)            do {} while(0)
#endif

#   ifndef ZF_PRINT
#       define ZF_PRINT(x...)                 do { \
            printf(x); printf("\n"); \
        } while(0)
#   endif

#endif /* __ZFCFG_H__ */
