/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include "string.h"
#include "gecko.h"
#include "vsprintf.h"

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
char * strstr ( const char *str1, const char *str2)
{
	char *cp = (char *) str1;
	char *s1, *s2;

	if ( !*str2 )
		return((char *)str1);

	while (*cp)
	{
		s1 = cp;
		s2 = (char *) str2;
		while ( *s1 && *s2 && !(*s1-*s2) )
			s1++, s2++;

		if (!*s2)
			return(cp);
		cp++;

	}

	return(NULL);
}
size_t strlcpy(char *dest, const char *src, size_t maxlen)
{
	size_t len,needed;

	len = needed = strnlen(src, maxlen-1) + 1;
	if (len >= maxlen)
		len = maxlen-1;

	memcpy(dest, src, len);
	dest[len]='\0';

	return needed-1;
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
#ifdef DEBUG
static char ascii(char s) {
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}

void hexdump(void *d, int len)
{
  u8 *data;
  int i, off;
  data = (u8*)d;
  for (off=0; off<len; off += 16)
  {
    dbgprintf("%08x  ",off);
    for(i=0; i<16; i++)
      if((i+off)>=len)
		  dbgprintf("   ");
      else
		  dbgprintf("%02x ",data[off+i]);

    dbgprintf(" ");
    for(i=0; i<16; i++)
      if((i+off)>=len) dbgprintf(" ");
      else dbgprintf("%c",ascii(data[off+i]));
    dbgprintf("\n");
  }
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
#endif
