#include <rtthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <winbase.h>

HANDLE OutputHandle = NULL;
void rt_console_init()
{
	AllocConsole();
	OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
}

void rt_kprintf(const char* fmt, ...)
{
	va_list args;
	char buffer[255];

	if (OutputHandle == NULL) rt_console_init();

	va_start(args,fmt);
	vsprintf(buffer,fmt,args);
	WriteConsoleA(OutputHandle, buffer, strlen(buffer), NULL, NULL);
	va_end(args);
}

#if 0
void rt_kprintf(const char *fmt, ...)
{
	va_list p;

	va_start(p, fmt);
	vprintf(fmt, p);
	va_end(p);

#ifndef _MSC_VER
	fflush(stdout);
#endif

	return;
}
#endif

void* rt_memset(void *src, int c, rt_ubase_t n)
{
	return memset(src, c, n);
}

void* rt_memcpy(void *dst, const void *src, rt_ubase_t n)
{
 	return memcpy(dst, src, n);
}

int rt_memcmp(const void *dst, const void *src, rt_ubase_t n)
{
	return memcmp(dst, src, n);
}

void* rt_memmove(void *dst, const void *src, rt_ubase_t count)
{
	return memmove(dst, src, count);
}

rt_ubase_t rt_strncmp(const char * cs, const char * ct, rt_ubase_t count)
{
	return strncmp(cs, ct, count);
}

rt_ubase_t rt_strlen (const char *src)
{
	return strlen(src);
}

char* rt_strncpy(char *dest, const char *src, rt_ubase_t n)
{
	char *dscan;
	const char *sscan;

	dscan = dest;
	sscan = src;
	while (n > 0)
    {
		--n;
		if ((*dscan++ = *sscan++) == '\0') break;
	}

	while (n-- > 0) *dscan++ = '\0';

	return dest;
}

void* rt_malloc(rt_size_t nbytes)
{
	return malloc(nbytes);
}

void* rt_realloc(void *ptr, rt_size_t size)
{
	return realloc(ptr, size);
}

void rt_free (void *ptr)
{
	free(ptr);
}

char* rt_strdup(const char* str)
{
#ifndef __GCC__
	char *tmp=(char *)malloc(strlen(str)+1);
	if (!tmp) return 0;
	strcpy(tmp,str);
	return tmp;
#else
	return strdup(str);
#endif
}

char* strdup(const char* str)
{
	return rt_strdup(str);
}

#if 0
#include <sys/time.h>
#include <sys/timeb.h>
void gettimeofday(struct timeval *tv,void* time_zone)
{
	struct _timeb cur;
	_ftime(&cur);
	tv->tv_sec = cur.time;
	tv->tv_usec = cur.millitm * 1000;
}
#endif

void rt_assert(const char* str, int line)
{
	rt_kprintf("assert: %s:%d", str, line);

	while(1);
}

int strncasecmp ( const char* s1, const char* s2, size_t len )
{
    unsigned int  x2;
    unsigned int  x1;
    const char*   end = s1 + len;

    while (1)
	{
        if ((s1 >= end) )
            return 0;
        
		x2 = *s2 - 'A'; if ((x2 < 26u)) x2 += 32;
        x1 = *s1 - 'A'; if ((x1 < 26u)) x1 += 32;
		s1++; s2++;
		
        if ((x2 != x1))
            break;
			
        if ((x1 == (unsigned int)-'A'))
            break;
    }

    return x1 - x2;
}

int stat(const char *s, struct stat *t)
{
	return _stat(s, t);
}
