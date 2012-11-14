#ifndef _STRING_H_
#define _STRING_H_

#include "global.h"

char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
int strcmp(const char *, const char *);
int strcmpi(const char *, const char*);
int strncmp(const char *p, const char *q, size_t n);
int strncmpi(const char *p, const char *q, size_t n);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
char *strchr(const char *s, int c);
void *memset(void *, int, size_t);
char *strstr (const char *str1, const char *str2);
char *skipPastArticles(char* s);
extern void memcpy(void *dst, const void *src, u32 size);int memcmp(const void *s1, const void *s2, size_t n);
int sprintf( char *astr, const char *fmt, ...);
void Asciify(char *str);
void Asciify2(char *str);
void upperCase(char *str);
char *strcat(char *str1, const char *str2);

#endif
