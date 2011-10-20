/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include "vsprintf.h"

#define LOG_DEBUG	1
//#define AUTO_CREATE_LOGFILE 1

#ifdef LOG_DEBUG	
#include "fs.h"

typedef enum {
	FR_OK = 0,			/* 0 */
	FR_DISK_ERR,		/* 1 */
	FR_INT_ERR,			/* 2 */
	FR_NOT_READY,		/* 3 */
	FR_NO_FILE,			/* 4 */
	FR_NO_PATH,			/* 5 */
	FR_INVALID_NAME,	/* 6 */
	FR_DENIED,			/* 7 */
	FR_EXIST,			/* 8 */
	FR_INVALID_OBJECT,	/* 9 */
	FR_WRITE_PROTECTED,	/* 10 */
	FR_INVALID_DRIVE,	/* 11 */
	FR_NOT_ENABLED,		/* 12 */
	FR_NO_FILESYSTEM,	/* 13 */
	FR_MKFS_ABORTED,	/* 14 */
	FR_TIMEOUT			/* 15 */
} FRESULT;

static char Path[32];
static u32 buflen ALIGNED(32);
extern u32 ignore_logfile;
#endif

static inline int isdigit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int isxdigit(int c)
{
	return (c >= '0' && c <= '9')
	    || (c >= 'a' && c <= 'f')
	    || (c >= 'A' && c <= 'F');
}

static inline int islower(int c)
{
	return c >= 'a' && c <= 'z';
}

static inline int toupper(int c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}

static int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

static char * number(char * str, long num, int base, int size, int precision
	,int type)
{
	char c,sign,tmp[66];
	const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
	int i;

	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0)
		tmp[i++] = digits[do_div(num,base)];
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0)
			*str++ = ' ';
	if (sign)
		*str++ = sign;
	if (type & SPECIAL) {
		if (base==8)
			*str++ = '0';
		else if (base==16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			*str++ = c;
	while (i < precision--)
		*str++ = '0';
	while (i-- > 0)
		*str++ = tmp[i];
	while (size-- > 0)
		*str++ = ' ';
	return str;
}

int vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	unsigned long num;
	int i, base;
	char * str;
	const char *s;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			continue;

		case 's':
			s = va_arg(args, char *);
			//if (!s)
			//	s = "<NULL>";

			len = strnlen(s, precision);

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			continue;

		case 'p':
			if (field_width == -1) {
				field_width = 2*sizeof(void *);
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			continue;


		case 'n':
			if (qualifier == 'l') {
				long * ip = va_arg(args, long *);
				*ip = (str - buf);
			} else {
				int * ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;

		case '%':
			*str++ = '%';
			continue;

		/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			else
				--fmt;
			continue;
		}
		if (qualifier == 'l')
			num = va_arg(args, unsigned long);
		else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & SIGN)
				num = (short) num;
		} else if (flags & SIGN)
			num = va_arg(args, int);
		else
			num = va_arg(args, unsigned int);
		str = number(str, num, base, field_width, precision, flags);
	}
	*str = '\0';
	return str-buf;
}

/*
static char buffer[1024] ALIGNED(32);

int dbgprintf( const char *fmt, ...)
{
	if ( (*(vu32*)(0xd800070) & 1) == 0)
		return 0;

	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);

	//FIL f;
	//if( f_open( &f, "log.txt", FA_WRITE|FA_OPEN_ALWAYS ) == FR_OK )
	//{
	//	f_lseek( &f, f.fsize );
	//	u32 read=0;
	//	f_write( &f, buffer, strlen(buffer), &read );  
	//	f_close( &f);
	//}
	//GeckoSendBuffer( buffer );

	svc_write(buffer);

	return i;
}
*/
int dbgprintf( const char *fmt, ...)
{

	if ( (*(vu32*)(0xd800070) & 1) == 0)
		return 0;

	va_list args;
	int i;

	char *buffer = (char*)malloca( 0x400, 0x40 );

	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);


#ifdef LOG_DEBUG	

	if (ignore_logfile == 0)
	{
		sprintf(Path,"/sneek/eslog.txt");
		s32 fd = IOS_Open( Path, 2 );
		if( fd < 0 )
		{
			if(fd == FS_ENOENT2)
			{
#ifdef	AUTO_CREATE_LOGFILE		
				ISFS_CreateFile(Path,0,0,0,0);
				fd = IOS_Open( Path, 2 );
				if( fd < 0 )
				{
					//sprintf(buffer,"CDI:dbgprintf->IOS_Open(\"%s\", 2 ):%d\n", Path, fd );
					//OSReport(buffer);
					free(buffer);
					return i;
				}
#else
				ignore_logfile = 1;
#endif			
			}
			else
			{
				//sprintf(buffer,"CDI:dbgprintf->IOS_Open(\"%s\", 2 ):%d\n", Path, fd );
				//OSReport(buffer);
				free(buffer);
				return i;
			}
		}

		s32 r = IOS_Seek(fd,0,2);
	
		if( r < 0 )
		{
			//sprintf(buffer,"CDI:dbgprintf->Seek(%d):%d\n", fd, r );
			//OSReport(buffer);
			IOS_Close(fd);
			free(buffer);
			return i;
		}
	
//		sprintf(buffer,"CDI:Made it after seek\r\n");
//		OSReport(buffer);
//		IOS_Close(fd);
	
	
	//dbgprintf("ES:NANDLoadFile->Size:%d\n", *Size );
		buflen = strlen(buffer);
		r = IOS_Write( fd, buffer,buflen);
		if( r < 0 )
		{
			//sprintf(buffer,"CDI:dbgprintf->Write(%d) len = %d :%d\n", fd, buflen, r );
			//OSReport(buffer);
//			hfree( status );
			IOS_Close(fd);
//			*Size = r;
			free(buffer);
			return i;
		}
	
//		sprintf(buffer,"CDI:Made it after write len = %d \r\n",buflen);
		IOS_Close(fd);
	}
	
// to see if things work as expected	
	svc_write(buffer);
	


#else
	//GeckoSendBuffer( buffer );
	svc_write( buffer );
#endif
	free(buffer);
	return i;
}


/*

void fatal(const char *fmt, ...)
{
	//va_list args;
	//char buffer[1024];
	//int i;

	//va_start(args, fmt);
	//i = vsprintf(buffer, fmt, args);
	//va_end(args);
	//svc_write(buffer);
	for (;;);
}

*/

