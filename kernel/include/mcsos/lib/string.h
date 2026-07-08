#ifndef MCSOS_LIB_STRING_H
#define MCSOS_LIB_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);

#ifdef __cplusplus
}
#endif

#endif
