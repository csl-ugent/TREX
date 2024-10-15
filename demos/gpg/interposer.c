#define _GNU_SOURCE

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>

void *memset(void *s, int c, size_t n)
{
	//printf("In interposed memset!");
    unsigned char* p=s;
    while(n--)
    {
        *p++ = (unsigned char)c;
    }
    return s;
}

void
__explicit_bzero_chk (void *dst, size_t len, size_t dstlen)
{
    memset (dst, '\0', len);
}