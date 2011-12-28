/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include "string.h"

size_t strnlen(const char *s, size_t count)
{
	const char *sc;

	for (sc = s; count-- && *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

size_t strlen(const char *s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

char *strncpy(char *dst, const char *src, size_t n)
{
	char *ret = dst;

	while (n && (*dst++ = *src++))
		n--;

	while (n--)
		*dst++ = 0;

	return ret;
}

char *strcpy(char *dst, const char *src)
{
	char *ret = dst;

	while ((*dst++ = *src++))
		;

	return ret;
}

int strcmp(const char *p, const char *q)
{
	for (;;) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a == 0 || a != b)
			return a - b;
	}
}

int strncmp(const char *p, const char *q, size_t n)
{
	while (n-- != 0) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a == 0 || a != b)
			return a - b;
	}
	return 0;
}

int strnccmp(const char *p, const char *q, size_t n)
{
	while (n-- != 0) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a == 0 || (((a - b) & 0xdf)!=0))
			return a - b;
	}
	return 0;
}

void *memset(void *dst, int x, size_t n)
{
	unsigned char *p;

	for (p = dst; n; n--)
		*p++ = x;

	return dst;
}

//void *memcpy(void *dst, const void *src, size_t n)
//{
//	unsigned char *p;
//	const unsigned char *q;
//
//	for (p = dst, q = src; n; n--)
//		*p++ = *q++;
//
//	return dst;
//}

int memcmp(const void *s1, const void *s2, size_t n)
{
	unsigned char *us1 = (unsigned char *) s1;
	unsigned char *us2 = (unsigned char *) s2;
	while (n-- != 0) {
		if (*us1 != *us2)
			return (*us1 < *us2) ? -1 : +1;
		us1++;
		us2++;
	}
	return 0;
}

char *strchr(const char *s, int c)
{
	do {
		if(*s == c)
			return (char *)s;
	} while(*s++ != 0);
	return NULL;
}

char *strcat( char *str1, const char *str2 )
{
	char *res = str1;
	
	while( *str1 )
		str1++;
		
	while( ( *str1++ = *str2++ ) )
		;
		
	return res;
}
