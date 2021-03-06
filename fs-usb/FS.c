/*

SNEEK - SD-NAND/ES emulation kit for Nintendo Wii

Copyright (C) 2009-2011  crediar
			  2011-2012  OverjoY

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "diskio.h"
#include "string.h"
#include "ehci.h"
#include "FS.h"

#define ISFS_OPEN_READ		1

static FIL fd_stack[MAX_FILE] ALIGNED(32);

char nandpath[0x60] ALIGNED(32);
char nandroot[0x40] ALIGNED(32);
char diroot[0x40] ALIGNED(32);

extern u32 s_size;
extern u32 s_cnt;

//#define USEATTR
//#undef DEBUG
//#define EDEBUG

typedef struct
{
	u32 data;
	u32 len;
} vector;

u32 HAXHandle;

bool DontFake = false;

void USB_Ioctlv(struct IPCMessage *msg)
{
	vector *v = (vector*)(msg->ioctlv.argv);
	s32 ret = -1017;

//#ifdef EDEBUG
//	dbgprintf("USB:IOS_Ioctlv( %d, 0x%x 0x%x 0x%x 0x%p )\n", msg->fd, msg->ioctl.command, InCount, OutCount, msg->ioctlv.argv );
//#endif
	switch(msg->ioctl.command)
	{
		case USB_IOCTL_UMS_INIT:
		{
			ret = FS_SUCCESS;
		} break;
		case USB_IOCTL_UMS_GET_CAPACITY:
		{
			u32 *ss = (u32*)(v[0].data);
			
			*ss = s_size;
			ret = s_cnt;

		} break;
		case USB_IOCTL_UMS_READ_SECTORS:
		{
			u8 *buf = (u8*)v[2].data;

			u32 sec = *(u32 *)v[0].data;
			u32 cnt  = *(u32 *)v[1].data;
			
			u32 t_read = 0;
			
			while(t_read < cnt)
			{
				u32 r_sec = (cnt - t_read);
				
				if(r_sec > 8)
					r_sec = 8;

				ret = USBStorage_Read_Sectors(sec+t_read, r_sec, &buf[t_read*s_size]);
				
				if (ret < 0)
					break;
					
				t_read += r_sec;
			}				
		
		} break;
		case USB_IOCTL_UMS_WRITE_SECTORS:
		{
			u8 *buf = (u8*)v[2].data;

			u32 sec = *(u32 *)v[0].data;
			u32 cnt  = *(u32 *)v[1].data;
			
			u32 t_write = 0;
			
			while(t_write < cnt)
			{
				u32 w_sec = (cnt - t_write);
				
				if (w_sec > 8)
					w_sec = 8;

				ret = USBStorage_Write_Sectors(sec+t_write, w_sec, &buf[t_write*s_size]);
				
				if (ret < 0)
					break;
					
				t_write += w_sec;
			}
		} break;
		case USB_IOCTL_UMS_READ_STRESS:
		{
			u8 *buf = (u8*)v[2].data;

			u32 sec = *(u32 *)v[0].data;
			u32 cnt  = *(u32 *)v[1].data;
			
			u32 t_read = 0;
			
			while(t_read < cnt)
			{
				u32 r_sec = (cnt - t_read);
				
				if (r_sec > 8)
					r_sec = 8;

				ret = USBStorage_Read_Stress(sec+t_read, r_sec, &buf[t_read*s_size]);
				
				if (ret < 0)
					break;
					
				t_read += r_sec;
			}
		} break;
	}

	mqueue_ack( (void *)msg, ret);
}


void FFS_Ioctlv(struct IPCMessage *msg)
{
	u32 InCount		= msg->ioctlv.argc_in;
	u32 OutCount	= msg->ioctlv.argc_io;
	vector *v		= (vector*)(msg->ioctlv.argv);
	s32 ret=0;

#ifdef EDEBUG
	dbgprintf("FFS:IOS_Ioctlv( %d, 0x%x 0x%x 0x%x 0x%p )\n", msg->fd, msg->ioctl.command, InCount, OutCount, msg->ioctlv.argv );
#endif

	//for( ret=0; ret<InCount+OutCount; ++ret)
	//{
	//	if( ((vu32)(v[ret].data)>>24) == 0 )
	//		dbgprintf("FFS:in:0x%08x\tout:0x%08x\n", v[ret].data, v[ret].data );
	//}
	switch(msg->ioctl.command)
	{
		case 0x60:
		{
			HAXHandle = FS_Open( (char*)(v[0].data), (u32)(v[1].data) );
#ifdef EDEBUG
			dbgprintf("FS_Open(%s, %02X):%d\n", (char*)(v[0].data), (u32)(v[1].data), HAXHandle );
#endif
			ret = HAXHandle;
		} break;
		case IOCTL_READDIR:
		{
			if( InCount == 1 && OutCount == 1 )
			{
				ret = FS_ReadDir( (char*)(v[0].data), (u32*)(v[1].data), NULL );

			} 
			else if( InCount == 2 && OutCount == 2 ) 
			{

				char *buf = heap_alloc_aligned( 0, v[2].len, 0x40 );
				memset32( buf, 0, v[2].len );

				ret = FS_ReadDir( (char*)(v[0].data), (u32*)(v[3].data), buf );

				memcpy( (char*)(v[2].data), buf, v[2].len );

				heap_free( 0, buf );

			} 
			else 
			{
				ret = FS_EFATAL;
			}

#ifdef DEBUG
			dbgprintf("FFS:ReadDir(\"%s\"):%d FileCount:%d\n", (char*)(v[0].data), ret, *(u32*)(v[1].data) );
#endif
		} break;
		
		case IOCTL_GETUSAGE:
		{
			char *path = (char *)v[0].data;
			u32 *blocks = (u32 *)v[1].data;
			u32 *inodes = (u32 *)v[2].data;
		
			*blocks = 0;
			*inodes = 1;
				
			ret = FS_GetUsage( path, inodes, blocks );
				
			if( ret >= 0 )
			{
				*blocks = *blocks / 0x4000;
					
				if( *blocks > 0x1000 )
					*blocks = 0x0 + ( *blocks & 0xFFF );
			}
			
			sync_after_write( blocks, sizeof(u32) );
			sync_after_write( inodes, sizeof(u32) );

#ifdef DEBUG
			dbgprintf("FFS:FS_GetUsage(\"%s\"):%d FileCount:%d FileSize:%d\n", (char*)(v[0].data), ret, *(u32*)(v[2].data), *(u32*)(v[1].data) );
#endif
		} break;
		default:
		{
			//dbgprintf("FFS:IOS_Ioctlv( %d, 0x%x 0x%x 0x%x 0x%p )\n", msg->fd, msg->ioctl.command, InCount, OutCount, msg->ioctlv.argv );
			ret = -1017;
		} break;
	}
#ifdef DEBUG
	//dbgprintf("FFS:IOS_Ioctlv():%d\n", ret);
#endif
	mqueue_ack( (void *)msg, ret);
}
void FFS_Ioctl(struct IPCMessage *msg)
{
	FIL fil;
	u8  *bufin  = (u8*)msg->ioctl.buffer_in;
	u32 lenin   = msg->ioctl.length_in;
	u8  *bufout = (u8*)msg->ioctl.buffer_io;
	u32 lenout  = msg->ioctl.length_io;
	s32 ret;

	//if( ((((vu32)bufin>>24)==0)&&lenin) || ((((vu32)bufout>>24)==0)&&lenout) )
	//{
	//	dbgprintf("FFS:in:0x%p\tout:0x%p\n", bufin, bufout );
	//}
#ifdef EDEBUG
	dbgprintf("FFS:IOS_Ioctl( %d 0x%x 0x%p 0x%x 0x%p 0x%x )\n", msg->fd, msg->ioctl.command, bufin, lenin, bufout, lenout);
#endif
	
	ret = FS_SUCCESS;
	switch(msg->ioctl.command)
	{
		case IOCTL_GET_DI_PATH:
		{	
			if(lenout < 0x20)
			{
				ret = -1017;
			}
			else 
			{
				memcpy(bufout, (void*)(diroot), 0x20);
				ret = FS_SUCCESS;
			}	
		 	
		} break;
		case IOCTL_DONT_FAKE:
		{
			DontFake = true;
			ret = FS_SUCCESS;
		} break;
		case IOCTL_IS_USB:
		{
			ret = FS_SUCCESS;
		} break;
		case IOCTL_NANDSTATS:
		{
			if( lenout < 0x1C )
				ret = -1017;
			else {
				NANDStat fs;
				
				//TODO: get real stats from SD CARD?
				fs.BlockSize	= 0x4000;
				fs.FreeBlocks	= 0x6DEC;
				fs.UsedBlocks	= 0x0DD4;
				fs.unk3			= 0x10;
				fs.unk4			= 0x02F0;
				fs.Free_INodes	= 0x146B;
				fs.unk5			= 0x0394;

				memcpy( bufout, &fs, sizeof(NANDStat) );
			}

			ret = FS_SUCCESS;
#ifdef DEBUG
			dbgprintf("FFS:GetNANDStats( %d, %p ):%d\n", msg->fd, msg->ioctl.buffer_io, ret );
#endif
		} break;

		case IOCTL_CREATEDIR:
		{
			ret = FS_CreateDir( (char*)(bufin+6) );
#ifdef USEATTR
			if( ret == FS_SUCCESS )
			{
				//create attribute file
				char *path = (char*)heap_alloc_aligned( 0, 0x40, 32 );
			
				FS_AdjustNpath(bufin+6);
//				_sprintf( path, "%s.attr", (char*)( bufin+6 ) );
				_sprintf( path, "%s.attr", (char*)( nandpath ) );

				if( f_open( &fil, path, FA_CREATE_ALWAYS | FA_WRITE ) == FR_OK )
				{
					u32 wrote;

					f_lseek( &fil, 6 );
					f_write( &fil, bufin+0x46, 4, &wrote);
					f_close( &fil );
				}

				heap_free( 0, path );
			}
#endif
#ifdef DEBUG
			dbgprintf("FFS:CreateDir(\"%s\", %02X, %02X, %02X, %02X ):%d\n", (char*)(bufin+6), *(u8*)(bufin+0x46), *(u8*)(bufin+0x47), *(u8*)(bufin+0x48), *(u8*)(bufin+0x49), ret );
#endif
		} break;

		case IOCTL_SETATTR:
		{
			if( lenin != 0x4A && lenin != 0x4C )
			{
				ret = FS_EFATAL;
			} else {
				ret = FS_SetAttr( (char*)(bufin+6) );
			}
#ifdef USEATTR
			if( ret == FS_SUCCESS )
			{
				//create attribute file
				char *path = (char*)heap_alloc_aligned( 0, 0x40, 32 );
			
				FS_AdjustNpath(bufin+6);
//				_sprintf( path, "%s.attr", (char*)(bufin+6) );
				_sprintf( path, "%s.attr", (char*)(nandpath) );

				if( f_open( &fil, (char*)path, FA_CREATE_ALWAYS | FA_WRITE ) == FR_OK )
				{
					u32 wrote;

					if( lenin == 0x4A )
						f_write( &fil, bufin, 4+2, &wrote);
					else
						f_lseek( &fil, 6 );

					f_write( &fil, bufin+0x46, 4, &wrote);
					f_close( &fil );
				}

				heap_free( 0, path );
			}
#endif
#ifdef EDEBUG
			dbgprintf("FFS:SetAttr(\"%s\", %08X, %04X, %02X, %02X, %02X, %02X):%d in:%X out:%X\n", (char*)(bufin+6), *(u32*)(bufin), *(u16*)(bufin+4), *(u8*)(bufin+0x46), *(u8*)(bufin+0x47), *(u8*)(bufin+0x48), *(u8*)(bufin+0x49), ret, lenin, lenout ); 
#endif

		} break;

		case IOCTL_GETATTR:
		{
			char *s=NULL;
			
			switch( lenin )
			{
				case 0x40:
					//s = (char*)(bufin);
					FS_AdjustNpath((char*)(bufin));
					s = nandpath;
					ret= FS_SUCCESS;
				break;
				case 0x4A:
					hexdump( bufin, lenin );
					//s = (char*)(bufin+6);
					FS_AdjustNpath((char*)(bufin+6));
					s = nandpath;
					ret= FS_SUCCESS;
				break;
				default:
					hexdump( bufin, lenin );
					ret = FS_EFATAL;
				break;
			}

			if( ret != FS_EFATAL )
			{
				if( f_open( &fil, s, FA_READ ) == FR_OK )
				{
					f_close( &fil );
					ret = FS_SUCCESS;
				} else {
					DIR d;
					if( f_opendir( &d, s ) == FR_OK )
					{
						ret = FS_SUCCESS;
					} 
					else 
					{
						ret = FS_ENOENT2;
					}
				}
#ifdef USEATTR
				//read attribute file
				char *path = (char*)heap_alloc_aligned( 0, 0x40, 32 );
			
				_sprintf( path, "%s.attr", s );

				if( f_open( &fil, path, FA_OPEN_EXISTING | FA_READ ) == FR_OK )
				{
					u32 read;
					f_read( &fil, bufout, 4+2, &read);
					f_read( &fil, bufout+0x46, 4, &read);
					f_close( &fil );
				}
				heap_free( 0, path );
#else
				*(u32*)(bufout) = 0x0000;
				*(u16*)(bufout) = 0x1000;
				*(u8*)(bufout+0x46) = 3;
				*(u8*)(bufout+0x47) = 3;
				*(u8*)(bufout+0x48) = 3;
				*(u8*)(bufout+0x49) = 3;

#endif
			}
#ifdef EDEBUG
			dbgprintf("FFS:GetAttr(\"%s\", %02X, %02X, %02X, %02X ):%d in:%X out:%X\n", s, *(u8*)(bufout+0x46), *(u8*)(bufout+0x47), *(u8*)(bufout+0x48), *(u8*)(bufout+0x49), ret, lenin, lenout );
#endif
		} break;

		case IOCTL_DELETE:
		{
			ret = FS_DeleteFile( (char*)bufin ) ;
#ifdef DEBUG
			dbgprintf("FFS:Delete(\"%s\"):%d\n", (char*)bufin, ret );
#endif
		} break;
		case IOCTL_RENAME:
		{
			ret = FS_Move( (char*)bufin, (char*)(bufin + 0x40) );
#ifdef USEATTR
			if( ret == FS_SUCCESS )
			{
				//move attribute file
				char *src = (char*)heap_alloc_aligned( 0, 0x80, 32 );
				char *dst = (char*)heap_alloc_aligned( 0, 0x80, 32 );
			
				_sprintf( src, "%s.attr", (char*)bufin );
				_sprintf( dst, "%s.attr", (char*)(bufin + 0x40) );

				ret = FS_Move( src, dst );
				//dbgprintf("FFS:Rename(\"%s\", \"%s\"):%d\n", src, dst, ret );

				heap_free( 0, src );
				heap_free( 0, dst );
			}
#endif
#ifdef DEBUG
			dbgprintf("FFS:Rename(\"%s\", \"%s\"):%d\n", (char*)bufin, (char*)(bufin + 0x40), ret );
#endif
		} break;
		case IOCTL_CREATEFILE:
		{
			ret = FS_CreateFile( (char*)(bufin+6) );
#ifdef USEATTR
			if( ret == FS_SUCCESS )
			{
				//create attribute file
				char *path = (char*)heap_alloc_aligned( 0, 0x40, 32 );
			
//				_sprintf( path, "%s.attr", (char*)(bufin+6) );
				FS_AdjustNpath(bufin+6);
				_sprintf( path, "%s.attr", (char*)(nandpath) );

				if( f_open( &fil, path, FA_CREATE_ALWAYS | FA_WRITE ) == FR_OK )
				{
					u32 wrote;

					if( lenin == 0x4A )
						f_write( &fil, bufin, 4+2, &wrote);
					else
						f_lseek( &fil, 6 );

					f_write( &fil, bufin+0x46, 4, &wrote);
					f_close( &fil );
				}

				heap_free( 0, path );
			}
#endif
#ifdef DEBUG
			//if( ret != -105 )
				dbgprintf("FFS:CreateFile(\"%s\", %02X, %02X, %02X, %02X):%d\n", (char*)(bufin+6), *(u8*)(bufin+0x46), *(u8*)(bufin+0x47), *(u8*)(bufin+0x48), *(u8*)(bufin+0x49), ret );
#endif
		} break;

		case IOCTL_GETSTATS:
		{
		
			ret = FS_GetStats( msg->fd, (FDStat*)bufout );

#ifdef EDEBUG
			dbgprintf("FFS:GetStats( %d, %d, %d ):%d\n", msg->fd, ((FDStat*)bufout)->file_length, ((FDStat*)bufout)->file_pos, ret );
#endif
		} break;

		case IOCTL_SHUTDOWN:
			//dbgprintf("FFS:Shutdown()\n");
			//Close all open FS handles
			//u32 i;
			//for( i=0; i < MAX_FILE; ++i)
			//{
			//	if( fd_stack[i].fs != NULL )
			//		f_close( &fd_stack[i] );
			//}
			ret = FS_SUCCESS;
		break;

		default:
#ifdef EDEBUG
			dbgprintf("FFS:Unknown IOS_Ioctl( %d 0x%x 0x%p 0x%x 0x%p 0x%x )\n", msg->fd, msg->ioctl.command, bufin, lenin, bufout, lenout);
#endif
			ret = FS_EFATAL;
		break;
	}

	mqueue_ack( (void *)msg, ret);
}

bool FS_IsNandFolder(char* whichpath)
{

	if(!strnccmp(whichpath, "/ticket", 7))
		return true;
	if(!strnccmp(whichpath, "/shared1", 8))
		return true;
	if(!strnccmp(whichpath, "/title", 6))
		return true;
	if(!strnccmp(whichpath, "/sys", 4))
		return true;
	if(!strnccmp(whichpath, "/wfs", 4))
		return true;
	if(!strnccmp(whichpath, "/shared2", 8))
		return true;
	if(!strnccmp(whichpath, "/import", 7))
		return true;
	if(!strnccmp(whichpath, "/meta", 5))
		return true;
	if(!strnccmp(whichpath, "/tmp",4))
		return true;
	if(!strnccmp(whichpath, "/sneekcache", 11))
		return true;

	return false;
}

void FS_AdjustNpath( char *path )
{			
	bool isnand = FS_IsNandFolder(path);
	if(!isnand)
	{
		strcpy(nandpath, path);
	}
	else
	{
		strcpy(nandpath, nandroot);
		strcat(nandpath, path);
		
	}
	
	if(*(vu32*)0x0 >> 8 == 0x52334f)
	{
		char *ptz = (char *)NULL;
		ptz = strstr(nandpath, "/00010000/");
		if(ptz != NULL)
		{
			if(FS_CreateDir("/title/20010000") != FS_ENOENT2)				
				strncpy(ptz, "/20010000/", 10);
		}
	}
}

u32 FS_CheckHandle( s32 fd )
{
	if( fd < 0 || fd >= MAX_FILE)
	{
#ifdef EDEBUG
		dbgprintf("FFS:Handle is invalid! %d < 0 || %d >= %d!\n", fd, fd, MAX_FILE);
#endif
		return 0;
	}
	if( fd_stack[fd].fs == NULL )
	{
#ifdef EDEBUG
		dbgprintf("FFS:Handle %d is invalid! FS == NULL!\n", fd);
#endif
		return 0;
	}

	return 1;
}

s32 FS_GetUsage(char *path, u32 *FileCount, u32 *TotalSize)
{
	if(*(u8*)0x0 != 'R' && *(u8*)0x0 != 'S' && DontFake != true)
	{
		if(*(vu32*)0x3180 >> 8 == 0x484142 && (strstr(path, "/ticket") != NULL || strstr(path, "/00010000/") != NULL || strstr(path, "/00010001/") != NULL))
		{				
#ifdef DEBUG
			dbgprintf("FFS:Don't fake values for: \"%s\" in Shop!\n", path);
#endif
		}
		else
		{
			*FileCount = 20;
			*TotalSize = 0x400000;
			return FS_SUCCESS;
		}
	}
	
	DontFake = false;
	
	if(*(vu32*)0x0 >> 8 == 0x535a41 || *(vu32*)0x0 >> 8 == 0x525050 || *(vu32*)0x0 >> 8 == 0x524f33)
	{
		*FileCount = 20;
		*TotalSize = 0x400000;
		return FS_SUCCESS;
	}
	
	char *file = heap_alloc_aligned(0, 0x40, 0x40);

	DIR d;
	FILINFO FInfo;
	s32 res=0;

	FS_AdjustNpath(path);
	
	switch(f_opendir(&d, nandpath ))
	{
		case FR_INVALID_NAME:
		case FR_NO_FILE:
		case FR_NO_PATH:			
			return FS_ENOENT2;
		default:
			return FS_EFATAL;
		case FR_OK:
			break;
	}

	while( (res = f_readdir(&d, &FInfo)) == FR_OK)
	{
#ifdef USEATTR
		if( FInfo.lfsize )
		{
			if( strstr( FInfo.lfname, ".attr" ) != NULL )
				continue;
		} else{
			if( strstr( FInfo.fname, ".attr" ) != NULL )
				continue;
		}
#endif
		if( FInfo.fattrib & AM_DIR )
		{
			memset32( file, 0, 0x40 );
			memcpy( file, path, strlen(path) );

			//insert slash
			file[strlen(path)] = '/';

			if( FInfo.lfsize )
			{
				memcpy( file+strlen(path)+1, FInfo.lfname, strlen(FInfo.lfname) );
			} else {
				memcpy( file+strlen(path)+1, FInfo.fname, strlen(FInfo.fname) );
			}


			res = FS_GetUsage( file, FileCount, TotalSize );
			if( res != FS_SUCCESS )
			{
				heap_free( 0, file );
				return res;
			}
		} 
		else 
		{
			*FileCount = *FileCount + 1;
			*TotalSize = *TotalSize + FInfo.fsize;
		}
	}

	heap_free( 0, file );
	return FS_SUCCESS;
}

/*
	Returns amount of items in a folder (folder+files)
	if the in/outcount are two it returns the amount of items and the item names of a folder

	FS_SUCCESS: on success
	FS_EFATAL : when the requested folder is an existing file
	FS_ENOENT2: when the folder doesn't exist
	FS_ENOENT2: for invalid names
*/
s32 FS_ReadDir( char *Path, u32 *FileCount, char *FileNames )
{
	DIR d;
	FIL f;
	FILINFO FInfo;
	s32 res = FS_EFATAL;

	FS_AdjustNpath(Path);

	if( f_open( &f, nandpath, FA_OPEN_EXISTING ) == FR_OK )
	{
		f_close( &f );
		return FS_EFATAL;
	}
	switch(f_opendir( &d, nandpath ))
	{
		case FR_INVALID_NAME:
		case FR_NO_PATH:
			return FS_ENOENT2;
		default:
			return FS_EFATAL;
		case FR_OK:
			break;
	}
	
	res = f_readdir( &d, &FInfo );

	*FileCount = 0;

	if( FileNames == NULL )
	{
		while( res == FR_OK )
		{
			*FileCount = (*FileCount) + 1;
#ifdef USEATTR
			if( FInfo.lfsize )
			{
				if( strstr( FInfo.lfname, ".attr" ) != NULL )
				{
					*FileCount -= 1;
				}
			} else {

				if( strstr( FInfo.fname, ".attr" ) != NULL )
				{
					*FileCount -= 1;
				}
			}
#endif
			res = f_readdir( &d, &FInfo );
		}

	} else {

		u32 off=0;
		while( res == FR_OK )
		{
			*FileCount = (*FileCount) + 1;
			if( FInfo.lfsize )
			{
#ifdef USEATTR
				if( strstr( FInfo.lfname, ".attr" ) != NULL )
				{
					*FileCount -= 1;
				} else {
#endif
					memcpy( (u8*)(FileNames + off), FInfo.lfname, strlen(FInfo.lfname) );
					off += strlen(FInfo.lfname) + 1;
#ifdef USEATTR
				}
#endif
			} else {

#ifdef USEATTR
				if( strstr( FInfo.fname, ".attr" ) != NULL )
				{
					*FileCount -= 1;
				} else {
#endif
					memcpy( (u8*)(FileNames + off), FInfo.fname, strlen(FInfo.fname) );
					off += strlen(FInfo.fname) + 1;
#ifdef USEATTR
				}
#endif
			}
			res = f_readdir( &d, &FInfo );
		}
	}

	if( res == FR_NO_FILE )
		return FS_SUCCESS;

	return FS_EFATAL;
}
s32 FS_CreateFile( char *Path )
{
	FIL fil;

	FS_AdjustNpath(Path);

	switch( f_open(&fil, nandpath, FA_CREATE_NEW ) )
	{
		case FR_EXIST:
			return FS_EEXIST2;

		case FR_OK:
			f_close(&fil);
			return FS_SUCCESS;

		case FR_NO_FILE:
		case FR_NO_PATH:
			return FS_ENOENT2;
		default:
			break;
	}

	return FS_EFATAL;
}
s32 FS_Delete( char *Path )
{
	dbgprintf("FS_Delete(\"%s\")\n", Path );

	FS_AdjustNpath(Path);

	char *FilePath = (char*)heap_alloc_aligned( 0, 0x80, 0x40 );
	char *LFilePath = (char*)heap_alloc_aligned( 0, 0x80, 0x40 );

	//normal clean up
	DIR d;
	if( f_opendir( &d, nandpath ) == FR_OK )
	{
		FILINFO FInfo;
		
		s32 res = f_readdir( &d, &FInfo );

		if( res != FR_OK )
		{
			heap_free( 0, FilePath );
			heap_free( 0, LFilePath );

			return FS_EFATAL;
		}

		while( res == FR_OK )
		{
			memset32( FilePath, 0, 0x40 );
			memset32( LFilePath, 0, 0x40 );
			
			strcpy( FilePath, Path );
			strcpy( LFilePath, nandpath );

			//insert slash
			FilePath[strlen(Path)] = '/';
			LFilePath[strlen(nandpath)] = '/';

			if( FInfo.lfsize )
			{
				//dbgprintf("\"%s\"\n", FInfo.lfname );
				memcpy( FilePath+strlen(Path)+1, FInfo.lfname, strlen(FInfo.lfname) );
				memcpy( LFilePath+strlen(nandpath)+1, FInfo.lfname, strlen(FInfo.lfname) );

			} else {
				//dbgprintf("\"%s\"\n", FInfo.fname );
				memcpy( FilePath+strlen(Path)+1, FInfo.fname, strlen(FInfo.fname) );
				memcpy( LFilePath+strlen(nandpath)+1, FInfo.fname, strlen(FInfo.fname) );

			}

			res = f_unlink( LFilePath );
			if( res  != FR_OK )
			{
				//dbgprintf("\"%s\":%d\n", FilePath, res );
				if( FInfo.fattrib&AM_DIR )
				{
					FS_Delete( FilePath );
					f_unlink( LFilePath );
				} else {
					heap_free( 0, FilePath );
					heap_free( 0, LFilePath );
					
					return FS_EFATAL;
				}
			}
			res = f_readdir( &d, &FInfo );
		}
	}

	heap_free( 0, FilePath );
	heap_free( 0, LFilePath );

	return FS_SUCCESS;
}
s32 FS_Close(s32 FileHandle)
{
	if(FileHandle == FS_FD || FileHandle == SD_FD)
		return FS_SUCCESS;
	
	if(FS_CheckHandle(FileHandle) == 0)
		return FS_EINVAL;
	
	if(f_close(&fd_stack[FileHandle]) != FR_OK)
	{
		memset32( &fd_stack[FileHandle], 0, sizeof(FIL) );
		return FS_EFATAL;
	} 
	else 
	{
		memset32( &fd_stack[FileHandle], 0, sizeof(FIL) );
		return FS_SUCCESS;
	}
	return FS_EFATAL;
}

s32 FS_Open(char *Path, u8 Mode)
{	
	if(strncmp(Path, "/AX", 3) == 0)
		return HAXHandle;

	// Is it a device?
	if(strncmp( Path, "/dev/", 5) == 0)
	{
		if(strncmp(Path+5, "fs", 2) == 0)
		{
//			dbgprintf("/dev/fs trigger detected\n");
			return FS_FD;
		}
		else if(strncmp(Path+5, "usb2", 4) == 0) 
		{
			if(*(u8*)0x0 != 'R' && *(u8*)0x0 != 'S')
			{
				dbgprintf("/dev/usb2 trigger detected\n");
				return USB_FD;
			}
			else
			{
				dbgprintf("UBISOFT AP detection found\n");
				return FS_ENOENT;
			}
		}
		/*else if(strncmp(Path+5, "usb/ven", 7) == 0) 
		{
			if(*(u8*)0x0 != 'R' && *(u8*)0x0 != 'S')
			{
				dbgprintf("/dev/usb/ven trigger detected\n");
				return VEN_FD;
			}
			return FS_ENOENT;
		}
		else if(strncmp(Path+5, "usb/hid", 7) == 0) 
		{
			if(*(u8*)0x0 != 'R' && *(u8*)0x0 != 'S')
			{
				dbgprintf("/dev/usb/hid trigger detected\n");
				return HID_FD;
			}
			return FS_ENOENT;
		}*/
		/* else if( strncmp( Path+5, "flash", 5 ) == 0 ) {
			return FS_ENOENT;		
		} else if( strncmp( Path+5, "boot2", 5 ) == 0) {
			return B2_FD;
		}*/ 
		else 
		{
			// Not a devicepath of ours, dispatch it to the syscall again..
			return FS_ENOENT;
		}
	} 
	else 
	{ // Or is it a filepath ?
		u32  i = 0;
		while(i < MAX_FILE)
		{
			if(fd_stack[i].fs == NULL)
				break;
			i++;
		}

		if(i == MAX_FILE)
			return FS_ENFILE;

		FS_AdjustNpath(Path);

		if( f_open( &fd_stack[i], nandpath, Mode ) != FR_OK )
		{
			switch(*(vu32*)0x0 >> 8)
			{							
				/*** Add games that need this hack for game saves here ***/
				case 0x524950:
				case 0x525559:
				{
					if((strstr(nandpath, "/data/") != NULL))
					{
						if(f_open( &fd_stack[i], nandpath, Mode | FA_CREATE_ALWAYS ) != FR_OK)
						{
							memset32(&fd_stack[i], 0, sizeof(FIL));
							return FS_ENOENT2;
						}
					}
					else
					{
						memset32(&fd_stack[i], 0, sizeof(FIL));
						return FS_ENOENT2;
					}
				} break;
				default:
				{
					/*** All other requests should return NO ENTRY ***/
					memset32(&fd_stack[i], 0, sizeof(FIL));
					return FS_ENOENT2;
				} break;					
			}
		}
		return i;		
	}
	return FS_EFATAL;
}

s32 FS_Write(s32 FileHandle, u8 *Data, u32 Length)
{
	u32 wrote = 0;
	
	if( FS_CheckHandle(FileHandle) == 0)
		return FS_EINVAL;

	s32 ret = f_write(&fd_stack[FileHandle], Data, Length, &wrote);	
	
	switch(ret)
	{
		case FR_OK:
			return wrote;
		case FR_DENIED:
			return FS_EACCESS;
		default:
			dbgprintf("f_write( %p, %p, %d, %p):%d\n", &fd_stack[FileHandle], Data, Length, &wrote, ret);
		break;
	}
	return FS_EFATAL;
}

s32 FS_Read(s32 FileHandle, u8 *Data, u32 Length)
{
	u32 read = 0;

	if(FS_CheckHandle(FileHandle) == 0)
		return FS_EINVAL;

	s32 ret = f_read(&fd_stack[FileHandle], Data, Length, &read);
	
	switch(ret)
	{
		case FR_OK:
			return read;
		case FR_DENIED:
			return FS_EACCESS;
		default:
			//dbgprintf("f_read( %p, %p, %d, %p):%d\n", &fd_stack[FileHandle], Data, Length, &read, ret);
			break;
	}

	return FS_EFATAL;
}
/*
	SEEK_SET returns the seeked amount
	SEEK_END returns the new offset, but returns -101 when you seek out of the file
	SEEK_CUR "
*/
s32 FS_Seek( s32 FileHandle, s32 Where, u32 Whence )
{
	if( FS_CheckHandle(FileHandle) == 0)
		return FS_EINVAL;

	switch( Whence )
	{
		case SEEK_SET:
		{
			if(Where > fd_stack[FileHandle].fsize)
				break;

			if(f_lseek( &fd_stack[FileHandle], Where) == FR_OK)
			{
				if(Where == 0)
					return 0;

				return fd_stack[FileHandle].fptr;
			}
		} break;
		case SEEK_CUR:
		{
			if(f_lseek(&fd_stack[FileHandle], Where + fd_stack[FileHandle].fptr) == FR_OK)
				return fd_stack[FileHandle].fptr;
		} break;

		case SEEK_END:
		{
			if(f_lseek(&fd_stack[FileHandle], Where + fd_stack[FileHandle].fsize) == FR_OK)
					return fd_stack[FileHandle].fptr;
		} break;

		default:
			break;
	}
	return FS_EFATAL;
}
s32 FS_GetStats(s32 FileHandle, FDStat *Stats)
{
	if(FS_CheckHandle(FileHandle) == 0)
		return FS_EINVAL;

	Stats->file_length  = fd_stack[FileHandle].fsize;
	Stats->file_pos		= fd_stack[FileHandle].fptr;

	return FS_SUCCESS;
}
s32 FS_CreateDir(char *Path)
{
	FS_AdjustNpath(Path);
	switch(f_mkdir(nandpath))
	{
		case FR_OK:
			return FS_SUCCESS;

		case FR_DENIED:
			return FS_EACCESS;

		case FR_EXIST:
			return FS_EEXIST2;

		case FR_NO_FILE:
		case FR_NO_PATH:
			return FS_ENOENT2;
		default:
			break;
	}

	return FS_EFATAL;
}
s32 FS_SetAttr(char *Path)
{		
	FIL fil;	
	FS_AdjustNpath(Path);
	
	if( f_open(&fil, nandpath, FA_OPEN_EXISTING) == FR_OK)
	{
		f_close(&fil);
		return FS_SUCCESS;
	} 
	else 
	{
		DIR d;
		if(f_opendir(&d, nandpath) == FR_OK)
			return FS_SUCCESS;
	}

	return FS_ENOENT2;
}
s32 FS_DeleteFile(char *Path)
{
	FS_AdjustNpath(Path);

#ifdef USEATTR
	//delete attribute file
	char *AttrFile = (char*)heap_alloc_aligned(0, 0x80, 0x40);

	_sprintf(AttrFile, "%s.attr", nandpath);
	f_unlink(AttrFile);
	heap_free(0, AttrFile);
#endif
	switch(f_unlink(nandpath))
	{
		case FR_DENIED:	//Folder is not empty and can't be removed without deleting the content first	
			return FS_Delete(Path);

		case FR_OK:
			return FS_SUCCESS;

		case FR_EXIST:
			return FS_EEXIST2;

		case FR_NO_FILE:
		case FR_NO_PATH:
			return FS_ENOENT2;
		default:
			break;
	}

	return FS_EFATAL;
}
s32 FS_Move(char *sPath, char *dPath)
{
	char *LSPath = (char *)heap_alloc_aligned(0, 0x80, 0x40);
	
	FS_AdjustNpath(sPath);
	strcpy(LSPath, nandpath);
	FS_AdjustNpath(dPath);
	
	switch(f_rename(LSPath, nandpath))
	{
		case FR_OK:
		{
			heap_free(0, LSPath);
			return FS_SUCCESS;
		}
		case FR_NO_PATH:
		case FR_NO_FILE:
		{	
			heap_free(0, LSPath);
			return FS_ENOENT2;
		}
		case FR_EXIST:	//On normal IOS Rename overwrites the target!
			if(f_unlink(nandpath) == FR_OK)
			{
				if(f_rename(LSPath, nandpath) == FR_OK)
				{
					heap_free(0, LSPath);
					return FS_SUCCESS;
				}
			}
		default:
			break;
	}
	heap_free(0, LSPath);
	return FS_EFATAL;
}
