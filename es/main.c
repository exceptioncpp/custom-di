/*

SNEEK - SD-NAND/ES emulation kit for Nintendo Wii

Copyright (C) 2009-2011  crediar

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
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "common.h"
#include "sdhcreg.h"
#include "sdmmcreg.h"
#include "alloc.h"
#include "font.h"
#include "DI.h"
#include "ES.h"
#include "SDI.h"
#include "SMenu.h"
#include "utils.h"

int verbose = 0;
u32 base_offset=0;
void *queuespace=NULL;
int queueid = 0;
int heapid=0;
int FFSHandle=0;
u32 FSUSB=0;

#undef DEBUG

static u32 SkipContent ALIGNED(32);
static u64 TitleID ALIGNED(32);
static u32 KernelVersion ALIGNED(32);

static u32 KeyIDT ALIGNED(32) = 0;

char diroot[0x20] ALIGNED(32);


TitleMetaData *iTMD = (TitleMetaData *)NULL;			//used for information during title import
static u8 *iTIK=NULL;									//used for information during title import

u32 ignore_logfile;

extern u16 TitleVersion;
extern u32 *KeyID;
extern u8 *CNTMap;
extern u32 *HCR;
extern u32 *SDStatus;

void iCleanUpTikTMD( void )
{
	if( iTMD != NULL )
	{
		free( iTMD );
		iTMD = NULL;
	}
	if( iTIK != NULL )
	{
		free( iTIK );
		iTIK = NULL;
	}
}

char *path=NULL;
u32 *size=NULL;
u64 *iTitleID=NULL;

unsigned char sig_fwrite[32] =
{
	0x94, 0x21, 0xFF, 0xD0,
	0x7C, 0x08, 0x02, 0xA6,
	0x90, 0x01, 0x00, 0x34,
	0xBF, 0x21, 0x00, 0x14, 
	0x7C, 0x9B, 0x23, 0x78,
	0x7C, 0xDC, 0x33, 0x78,
	0x7C, 0x7A, 0x1B, 0x78,
	0x7C, 0xB9, 0x2B, 0x78, 
} ;

unsigned char patch_fwrite[144] =
{
	0x7C, 0x85, 0x21, 0xD7, 0x40, 0x81, 0x00, 0x84, 0x3C, 0xE0, 0xCD, 0x00, 0x3D, 0x40, 0xCD, 0x00, 
	0x3D, 0x60, 0xCD, 0x00, 0x60, 0xE7, 0x68, 0x14, 0x61, 0x4A, 0x68, 0x24, 0x61, 0x6B, 0x68, 0x20, 
	0x38, 0xC0, 0x00, 0x00, 0x7C, 0x06, 0x18, 0xAE, 0x54, 0x00, 0xA0, 0x16, 0x64, 0x08, 0xB0, 0x00, 
	0x38, 0x00, 0x00, 0xD0, 0x90, 0x07, 0x00, 0x00, 0x7C, 0x00, 0x06, 0xAC, 0x91, 0x0A, 0x00, 0x00, 
	0x7C, 0x00, 0x06, 0xAC, 0x38, 0x00, 0x00, 0x19, 0x90, 0x0B, 0x00, 0x00, 0x7C, 0x00, 0x06, 0xAC, 
	0x80, 0x0B, 0x00, 0x00, 0x7C, 0x00, 0x04, 0xAC, 0x70, 0x09, 0x00, 0x01, 0x40, 0x82, 0xFF, 0xF4, 
	0x80, 0x0A, 0x00, 0x00, 0x7C, 0x00, 0x04, 0xAC, 0x39, 0x20, 0x00, 0x00, 0x91, 0x27, 0x00, 0x00, 
	0x7C, 0x00, 0x06, 0xAC, 0x74, 0x09, 0x04, 0x00, 0x41, 0x82, 0xFF, 0xB8, 0x38, 0xC6, 0x00, 0x01, 
	0x7F, 0x86, 0x20, 0x00, 0x40, 0x9E, 0xFF, 0xA0, 0x7C, 0xA3, 0x2B, 0x78, 0x4E, 0x80, 0x00, 0x20, 
};

void ES_Ioctlv( struct ipcmessage *msg )
{
	u32 InCount		= msg->ioctlv.argc_in;
	u32 OutCount	= msg->ioctlv.argc_io;
	vector *v		= (vector*)(msg->ioctlv.argv);
	s32 ret			= ES_FATAL;
	u32 i;

	//dbgprintf("ES:IOS_Ioctlv( %d 0x%x %d %d 0x%p )\n", msg->fd, msg->ioctlv.command, msg->ioctlv.argc_in, msg->ioctlv.argc_io, msg->ioctlv.argv);
	////	
	//for( i=0; i<InCount+OutCount; ++i)
	//{
	//	dbgprintf("data:%p len:%d\n", v[i].data, v[i].len );
	//}

	switch(msg->ioctl.command)
	{
		case IOCTL_ES_VERIFYSIGN:
		{
			ret = ES_SUCCESS;
			////dbgprintf("ES:VerifySign():%d\n", ret );		
		} break;
		case IOCTL_ES_DECRYPT:
		{
			ret = aes_decrypt_( *(u32*)(v[0].data), (u8*)(v[1].data), (u8*)(v[2].data), v[2].len, (u8*)(v[4].data));
			//dbgprintf("ES:Decrypt( %d, %p, %p, %d, %p ):%d\n", *(u32*)(v[0].data), (u8*)(v[1].data), (u8*)(v[2].data), v[2].len, (u8*)(v[4].data), ret );	
		} break;
		case IOCTL_ES_ENCRYPT:
		{
			ret = aes_encrypt( *(u32*)(v[0].data), (u8*)(v[1].data), (u8*)(v[2].data), v[2].len, (u8*)(v[4].data));
			////dbgprintf("ES:Encrypt( %d, %p, %p, %d, %p ):%d\n", *(u32*)(v[0].data), (u8*)(v[1].data), (u8*)(v[2].data), v[2].len, (u8*)(v[4].data), ret );	
		} break;
		case IOCTL_ES_SIGN:
		{
			ret = ES_Sign( &TitleID, (u8*)(v[0].data), v[0].len, (u8*)(v[1].data), (u8*)(v[2].data) );
			////dbgprintf("ES:Sign():%d\n", ret );		
		} break;
		case IOCTL_ES_GETDEVICECERT:
		{
			_sprintf( path, "/sys/device.cert" );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				GetDeviceCert( (void*)(v[0].data) );
			} else {
				memcpy( (u8*)(v[0].data), data, 0x180 );
				free( data );
			}

			ret = ES_SUCCESS;
			////dbgprintf("ES:GetDeviceCert():%d\n", ret );			
		} break;
		case IOCTL_ES_DIGETSTOREDTMD:
		{
			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(TitleID>>32), (u32)(TitleID) );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				ret = *size;
			} else {
				memcpy( (u8*)(v[1].data), data, *size );
				
				ret = ES_SUCCESS;
				free( data );
			}

			////dbgprintf("ES:DIGetStoredTMD( %08x-%08x ):%d\n", (u32)(TitleID>>32), (u32)(TitleID), ret );
		} break;
		case IOCTL_ES_DIGETSTOREDTMDSIZE:
		{
			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(TitleID>>32), (u32)(TitleID) );

			s32 fd = IOS_Open( path, 1 );
			if( fd < 0 )
			{
				ret = fd;
			} else {

				fstats *status = (fstats*)malloc( sizeof(fstats) );

				ret = ISFS_GetFileStats( fd, status );
				if( ret < 0 )
				{
					dbgprintf("ES:ISFS_GetFileStats(%d, %p ):%d\n", fd, status, ret );
				} else
					*(u32*)(v[0].data) = status->Size;

				free( status );
			}

			////dbgprintf("ES:DIGetStoredTMDSize( %08x-%08x ):%d\n", (u32)(TitleID>>32), (u32)(TitleID), ret );
			
		} break;
		case IOCTL_ES_GETSHAREDCONTENTS:
		{
			_sprintf( path, "/shared1/content.map" );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				ret = ES_FATAL;
			} else {

				for( i=0; i < *(u32*)(v[0].data); ++i )
					memcpy( (u8*)(v[1].data+i*20), data+i*0x1C+8, 20 );

				free( data );
				ret = ES_SUCCESS;
			}

			////dbgprintf("ES:ES_GetSharedContents():%d\n", ret );
		} break;
		case IOCTL_ES_GETSHAREDCONTENTCNT:
		{
			_sprintf( path, "/shared1/content.map" );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				ret = ES_FATAL;
			} else {

				*(u32*)(v[0].data) = *size / 0x1C;

				free( data );
				ret = ES_SUCCESS;
			}

			////dbgprintf("ES:ES_GetSharedContentCount(%d):%d\n", *(vu32*)(v[0].data), ret );
		} break;
		case IOCTL_ES_GETTMDCONTENTCNT:
		{
			*(u32*)(v[1].data) = 0;
			TitleMetaData *tTMD = (TitleMetaData*)(v[1].data);
		
			for( i=0; i < tTMD->ContentCount; ++i )
			{	
				if( tTMD->Contents[i].Type & CONTENT_SHARED )
				{
					if( ES_CheckSharedContent( tTMD->Contents[i].SHA1 ) == 1 )
						(*(u32*)(v[1].data))++;
				} else {
					_sprintf( path, "/title/%08x/%08x/content/%08x.app", *(u32*)(v[0].data+0x18C), *(u32*)(v[0].data+0x190), tTMD->Contents[i].ID );
					s32 fd = IOS_Open( path, 1 );
					if( fd >= 0 )
					{
						(*(u32*)(v[1].data))++;
						IOS_Close( fd );
					}
				}
			}

			ret = ES_SUCCESS;
			////dbgprintf("ES:GetTmdContentsOnCardCount(%d):%d\n", *(u32*)(v[1].data), ret );
		} break;
		case IOCTL_ES_GETTMDCONTENTS:
		{
			u32 count=0;

			for( i=0; i < *(u16*)(v[0].data+0x1DE); ++i )
			{	
				if( (*(u16*)(v[0].data+0x1EA+i*0x24) & 0x8000) == 0x8000 )
				{
					if( ES_CheckSharedContent( (u8*)(v[0].data+0x1F4+i*0x24) ) == 1 )
					{
						*(u32*)(v[2].data+4*count) = *(u32*)(v[0].data+0x1E4+i*0x24);
						count++;
					}
				} else {
					_sprintf( path, "/title/%08x/%08x/content/%08x.app", *(u32*)(v[0].data+0x18C), *(u32*)(v[0].data+0x190), *(u32*)(v[0].data+0x1E4+i*0x24) );
					s32 fd = IOS_Open( path, 1 );
					if( fd >= 0 )
					{
						*(u32*)(v[2].data+4*count) = *(u32*)(v[0].data+0x1E4+i*0x24);
						count++;
						IOS_Close( fd );
					}
				}
			}

			ret = ES_SUCCESS;
			////dbgprintf("ES:ListTmdContentsOnCard():%d\n", ret );
		} break;
		case IOCTL_ES_GETDEVICEID:
		{
			_sprintf( path, "/sys/device.cert" );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				GetKey( 1, (u8*)(v[0].data) );
			} else {
			
				u32 value = 0;

				//Convert from string to value
				for( i=0; i < 8; ++i )
				{
					if( *(u8*)(data+i+0xC6) > '9' )
						value |= ((*(u8*)(data+i+0xC6))-'W')<<((7-i)<<2);	// 'a'-10 = 'W'
					 else 
						value |= ((*(u8*)(data+i+0xC6))-'0')<<((7-i)<<2);
				}

				*(u32*)(v[0].data) = value;
				free( data );
			}

			ret = ES_SUCCESS;
			////dbgprintf("ES:ES_GetDeviceID( 0x%08x ):%d\n", *(u32*)(v[0].data), ret );
		} break;
		case IOCTL_ES_GETCONSUMPTION:
		{
			*(u32*)(v[2].data) = 0;
			ret = ES_SUCCESS;

			////dbgprintf("ES:ES_GetConsumption():%d\n", ret );
		} break;
		case IOCTL_ES_ADDTITLEFINISH:
		{
			ret = ES_AddTitleFinish( iTMD );

			//Get TMD for the CID names and delete!
			_sprintf( path, "/tmp/title.tmd" );
			ISFS_Delete( path );

			for( i=0; i < iTMD->ContentCount; ++i )
			{
				_sprintf( path, "/tmp/%08x", iTMD->Contents[i].ID );
				ISFS_Delete( path );
				_sprintf( path, "/tmp/%08x.app", iTMD->Contents[i].ID );
				ISFS_Delete( path );
			}

			////TODO: Delete contents from old versions of this title
			//if( ret == ES_SUCCESS )
			//{
			//	//get dir list
			//	_sprintf( path, "/ticket/%08x/%08x/content", *(u32*)(iTMD+0x18C), *(u32*)(iTMD+0x190) );

			//	ISFS_ReadDir( path, NULL, FileCount

			//}

			iCleanUpTikTMD();

			////dbgprintf("ES:AddTitleFinish():%d\n", ret );
		} break;
		case IOCTL_ES_ADDTITLECANCEL:
		{
			//Get TMD for the CID names and delete!
			_sprintf( path, "/tmp/title.tmd" );
			ISFS_Delete( path );
			
			for( i=0; i < iTMD->ContentCount; ++i )
			{
				_sprintf( path, "/tmp/%08x", iTMD->Contents[i].ID );
				ISFS_Delete( path );
				_sprintf( path, "/tmp/%08x.app", iTMD->Contents[i].ID );
				ISFS_Delete( path );
			}

			iCleanUpTikTMD();

			ret = ES_SUCCESS;
			////dbgprintf("ES:AddTitleCancel():%d\n", ret );
		} break;
		case IOCTL_ES_ADDCONTENTFINISH:
		{
			if( SkipContent )
			{
				ret = ES_SUCCESS;
				////dbgprintf("ES:AddContentFinish():%d\n", ret );
				break;
			}

			if( iTMD == NULL )
				ret = ES_FATAL;
			else {
				//load Ticket to forge the decryption key
				_sprintf( path, "/ticket/%08x/%08x.tik", (u32)(iTMD->TitleID>>32), (u32)(iTMD->TitleID) );

				iTIK = NANDLoadFile( path, size );
				if( iTIK == NULL )
				{
					iCleanUpTikTMD();
					ret = ES_ETIKTMD;

				} else {

					ret = ES_CreateKey( iTIK );
					if( ret >= 0 )
					{
						ret = ES_AddContentFinish( *(vu32*)(v[0].data), iTIK, iTMD );
						DestroyKey( *KeyID );
					}

					free( iTIK );
				}
			}

			////dbgprintf("ES:AddContentFinish():%d\n", ret );
		} break;
		case IOCTL_ES_ADDCONTENTDATA:
		{
			if( SkipContent )
			{
				ret = ES_SUCCESS;
				////dbgprintf("ES:AddContentData(<fast>):%d\n", ret );
				break;
			}

			if( iTMD == NULL )
				ret = ES_FATAL;
			else {
				ret = ES_AddContentData( *(s32*)(v[0].data), (u8*)(v[1].data), v[1].len );
			}

			//if( ret < 0 )
				////dbgprintf("ES:AddContentData( %d, 0x%p, %d ):%d\n", *(s32*)(v[0].data), (u8*)(v[1].data), v[1].len, ret );
		} break;
		case IOCTL_ES_ADDCONTENTSTART:
		{
			SkipContent=0;

			if( iTMD == NULL )
				ret = ES_FATAL;
			else {
				//check if shared content and if it is already installed so we can skip this one
				for( i=0; i < iTMD->ContentCount; ++i )
				{
					if( iTMD->Contents[i].ID == *(u32*)(v[1].data) )
					{
						if( iTMD->Contents[i].Type & CONTENT_SHARED )
						{
							if( ES_CheckSharedContent( iTMD->Contents[i].SHA1 ) == 1 )
							{
								SkipContent=1;
								////dbgprintf("ES:Content already installed, using fast install!\n");
							}
						}
						break;
					}
				}

				ret = *(u32*)(v[1].data);
			}

			////dbgprintf("ES:AddContentStart():%d\n", ret );
		} break;
		case IOCTL_ES_ADDTITLESTART:
		{
			//Copy TMD to internal buffer for later use
			iTMD = (TitleMetaData*)malloca( v[0].len, 32 );
			memcpy( iTMD, (u8*)(v[0].data), v[0].len );

			_sprintf( path, "/tmp/title.tmd" );

			ES_TitleCreatePath( iTMD->TitleID );

			ret = ISFS_CreateFile( path, 0, 3, 3, 3 );
			if( ret < 0 )
			{
				dbgprintf("ISFS_CreateFile(\"%s\"):%d\n", path, ret );
			} else {

				s32 fd = IOS_Open( path, ISFS_OPEN_WRITE );
				if( fd < 0 )
				{
					//dbgprintf("IOS_Open(\"%s\"):%d\n", path, fd );
					ret = fd;
				} else {
					ret = IOS_Write( fd, (u8*)(v[0].data), v[0].len );
					if( ret < 0 || ret != v[0].len )
					{
						dbgprintf("IOS_Write( %d, %p, %d):%d\n", fd, v[0].data, v[0].len, ret );
					} else {
						ret = ES_SUCCESS;
					}

					IOS_Close( fd );
				}
			}

			if( ret == ES_SUCCESS )
			{
				//Add new TitleUID to uid.sys
				u16 UID = 0;
				ES_GetUID( &(iTMD->TitleID), &UID );
			}

			////dbgprintf("ES:AddTitleStart():%d\n", ret );
		} break;
		case IOCTL_ES_ADDTICKET:
		{
			//Copy ticket to local buffer
			Ticket *ticket = (Ticket*)malloca( v[0].len, 32 );
			memcpy( ticket, (u8*)(v[0].data), v[0].len );
			
			_sprintf( path, "/tmp/%08x.tik", (u32)(ticket->TitleID) );

			if( ticket->ConsoleID )
				doTicketMagic( ticket );

			ES_TitleCreatePath( ticket->TitleID );

			ret = ISFS_CreateFile( path, 0, 3, 3, 3 );
			if( ret < 0 )
			{
				dbgprintf("ES:ISFS_CreateFile(\"%s\"):%d\n", path, ret );
			} else {

				s32 fd = IOS_Open( path, ISFS_OPEN_WRITE );
				if( fd < 0 )
				{
					//dbgprintf("ES:IOS_Open(\"%s\"):%d\n", path, fd );
					ret = fd;
				} else {

					ret = IOS_Write( fd, ticket, v[0].len );
					if( ret < 0 || ret != v[0].len )
					{
						dbgprintf("ES:IOS_Write( %d, %p, %d):%d\n", fd, ticket, v[0].len, ret );

					} else {

						IOS_Close( fd );

						char *dstpath = (char*)malloca( 0x40, 32 );

						_sprintf( path, "/tmp/%08x.tik", (u32)(ticket->TitleID) );
						_sprintf( dstpath, "/ticket/%08x/%08x.tik", (u32)(ticket->TitleID>>32), (u32)(ticket->TitleID) );

						//this function moves the file, overwriting the target
						ret = ISFS_Rename( path, dstpath );
						//if( ret < 0 )
						//	dbgprintf("ES:ISFS_Rename( \"%s\", \"%s\" ):%d\n", path, dstpath, ret );

						free( dstpath );
					}
				}
			}

			////dbgprintf("ES:AddTicket(%08x-%08x):%d\n", (u32)(ticket->TitleID>>32), (u32)(ticket->TitleID), ret );
			free( ticket );
		} break;
		case IOCTL_ES_EXPORTTITLEINIT:
		{
			ret = ES_SUCCESS;
			////dbgprintf("ES:ExportTitleStart(%08x-%08x):%d\n", (u32)((*(u64*)(v[0].data))>>32), (u32)(*(u64*)(v[0].data)), ret );
		} break;
		case IOCTL_ES_EXPORTTITLEDONE:
		{
			ret = ES_SUCCESS;
			////dbgprintf("ES:ExportTitleDone():%d\n", ret );
		} break;
		case IOCTL_ES_DELETETITLECONTENT:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x/content", (u32)(*iTitleID>>32), (u32)(*iTitleID) );
			ret = ISFS_Delete( path );
			if( ret >= 0 )
				ISFS_CreateDir( path, 0, 3, 3, 3 );

			////dbgprintf("ES:DeleteTitleContent(%08x-%08x):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), ret );
		} break;
		case IOCTL_ES_DELETETICKET:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/ticket/%08x/%08x.tik", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			ret = ISFS_Delete( path );

			////dbgprintf("ES:DeleteTicket(%08x-%08x):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), ret );
		} break;
		case IOCTL_ES_DELETETITLE:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x", (u32)(*iTitleID>>32), (u32)(*iTitleID) );
			ret = ISFS_Delete( path );

			////dbgprintf("ES:DeleteTitle(%08x-%08x):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), ret );
		} break;
		case IOCTL_ES_GETTITLECONTENTS:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			TitleMetaData *tTMD = (TitleMetaData*)NANDLoadFile( path, size );
			if( tTMD != NULL )
			{
				u32 count=0;
				for( i=0; i < tTMD->ContentCount; ++i )
				{	
					if( tTMD->Contents[i].Type & CONTENT_SHARED )
					{
						if( ES_CheckSharedContent( tTMD->Contents[i].SHA1 ) == 1 )
						{
							*(u32*)(v[2].data+0x4*count) = tTMD->Contents[i].ID;
							count++;
						}
					} else {
						_sprintf( path, "/title/%08x/%08x/content/%08x.app", (u32)(tTMD->TitleID>>32), (u32)(tTMD->TitleID), tTMD->Contents[i].ID );
						s32 fd = IOS_Open( path, 1 );
						if( fd >= 0 )
						{
							*(u32*)(v[2].data+0x4*count) = tTMD->Contents[i].ID;
							count++;
							IOS_Close( fd );
						}
					}
				}

				free( tTMD );

				ret = ES_SUCCESS;

			} else {
				ret = *size;
			}

			////dbgprintf("ES:GetTitleContentsOnCard( %08x-%08x ):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), ret );
		} break;
		case IOCTL_ES_GETTITLECONTENTSCNT:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			TitleMetaData *TMD = (TitleMetaData *)NANDLoadFile( path, size );
			if( TMD != NULL )
			{
				*(u32*)(v[1].data) = 0;
			
				for( i=0; i < TMD->ContentCount; ++i )
				{	
					if( TMD->Contents[i].Type & CONTENT_SHARED )
					{
						if( ES_CheckSharedContent( TMD->Contents[i].SHA1 ) == 1 )
							(*(u32*)(v[1].data))++;
					} else {
						_sprintf( path, "/title/%08x/%08x/content/%08x.app", (u32)((TMD->TitleID)>>32), (u32)(TMD->TitleID), TMD->Contents[i].ID );
						s32 fd = IOS_Open( path, 1 );
						if( fd >= 0 )
						{
							(*(u32*)(v[1].data))++;
							IOS_Close( fd );
						}
					}
				}
				
				ret = ES_SUCCESS;
				free( TMD );

			} else {
				ret = *size;
			}

			////dbgprintf("ES:GetTitleContentCount( %08x-%08x, %d):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), *(u32*)(v[1].data), ret );
		} break;
		case IOCTL_ES_GETTMDVIEWS:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			ret = ES_GetTMDView( iTitleID, (u8*)(v[2].data) );

			////dbgprintf("ES:GetTMDView( %08x-%08x ):%d\n", (u32)(*iTitleID>>32), (u32)*iTitleID, ret );
		} break;
		case IOCTL_ES_DIGETTICKETVIEW:
		{
			if( v[0].len )
			{
				hexdump( (u8*)(v[0].data), v[1].len );
				while(1);
			}

			_sprintf( path, "/ticket/%08x/%08x.tik", (u32)(TitleID>>32), (u32)TitleID );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				ret = ES_DIGetTicketView( &TitleID, (u8*)(v[1].data) );
			} else  {

				iES_GetTicketView( data, (u8*)(v[1].data) );
				
				free( data );
				ret = ES_SUCCESS;
			}

			//dbgprintf("ES:GetDITicketViews( %08x-%08x ):%d\n", (u32)(TitleID>>32), (u32)TitleID, ret );
		} break;
		case IOCTL_ES_GETVIEWS:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/ticket/%08x/%08x.tik", (u32)(*iTitleID>>32), (u32)*iTitleID );

			u8 *data = NANDLoadFile( path, size );
			if( data == NULL )
			{
				ret = *size;
			} else  {

				for( i=0; i < *(u32*)(v[1].data); ++i )
					iES_GetTicketView( data + i * TICKET_SIZE, (u8*)(v[2].data) + i * 0xD8 );
				
				free( data );
				ret = ES_SUCCESS;
			}

			//dbgprintf("ES:GetTicketViews( %08x-%08x ):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), ret );
		} break;
		case IOCTL_ES_GETTMDVIEWSIZE:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			TitleMetaData *TMD = (TitleMetaData *)NANDLoadFile( path, size );
			if( TMD == NULL )
			{
				ret = *size;
			} else {
				*(u32*)(v[1].data) = TMD->ContentCount*16+0x5C;

				free( TMD );
				ret = ES_SUCCESS;
			}

			//dbgprintf("ES:GetTMDViewSize( %08x-%08x, %d ):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), *(u32*)(v[1].data), ret );
		} break;
		case IOCTL_ES_DIGETTMDVIEW:
		{
			TitleMetaData *data = (TitleMetaData*)malloca( v[0].len, 0x40 );
			memcpy( data, (u8*)(v[0].data), v[0].len );

			iES_GetTMDView( data, (u8*)(v[2].data) );

			free( data );

			ret = ES_SUCCESS;
			//dbgprintf("ES:DIGetTMDView():%d\n", ret );
		} break;
		case IOCTL_ES_DIGETTMDVIEWSIZE:
		{
			TitleMetaData *TMD = (TitleMetaData*)malloca( v[0].len, 0x40 );
			memcpy( TMD, (u8*)(v[0].data), v[0].len );

			*(u32*)(v[1].data) = TMD->ContentCount*16+0x5C;

			free( TMD );

			ret = ES_SUCCESS;
			//dbgprintf("ES:DIGetTMDViewSize( %d ):%d\n", *(u32*)(v[1].data), ret );
		} break;
		case IOCTL_ES_CLOSECONTENT:
		{
			IOS_Close( *(u32*)(v[0].data) );

			ret = ES_SUCCESS;
			//dbgprintf("ES:CloseContent(%d):%d\n", *(u32*)(v[0].data), ret );
		} break;
		case IOCTL_ES_SEEKCONTENT:
		{
			ret = IOS_Seek( *(u32*)(v[0].data), *(u32*)(v[1].data), *(u32*)(v[2].data) );
			//dbgprintf("ES:SeekContent( %d, %d, %d ):%d\n", *(u32*)(v[0].data), *(u32*)(v[1].data), *(u32*)(v[2].data), ret );
		} break;
		case IOCTL_ES_READCONTENT:
		{
			ret = IOS_Read( *(u32*)(v[0].data), (u8*)(v[1].data), v[1].len );

			int i;
			for( i=0; i < v[1].len; i+=4 )
			{
				if( memcmp( (void*)((u8*)(v[1].data)+i), sig_fwrite, sizeof(sig_fwrite) ) == 0 )
				{
					//dbgprintf("ES:[patcher] Found __fwrite pattern:%08X\n",  (u32)((u8*)(v[1].data)+i) | 0x80000000 );
					memcpy( (void*)((u8*)(v[1].data)+i), patch_fwrite, sizeof(patch_fwrite) );
				}
				//if( *(vu32*)((u8*)(v[1].data)+i) == 0x3C608000 )
				//{
				//	if( ((*(vu32*)((u8*)(v[1].data)+i+4) & 0xFC1FFFFF ) == 0x800300CC) && ((*(vu32*)((u8*)(v[1].data)+i+8) >> 24) == 0x54 ) )
				//	{
				//		//dbgprintf("ES:[patcher] Found VI pattern:%08X\n", (u32)((u8*)(v[1].data)+i) | 0x80000000 );
				//		*(vu32*)0xCC = 1;
				//		dbgprintf("ES:VideoMode:%d\n", *(vu32*)0xCC );
				//		//*(vu32*)((u8*)(v[1].data)+i+4) = 0x5400F0BE | ((*(vu32*)((u8*)(v[1].data)+i+4) & 0x3E00000) >> 5 );
				//	}
				//}
			}

			//dbgprintf("ES:ReadContent( %d, %p, %d ):%d\n", *(u32*)(v[0].data), v[1].data, v[1].len, ret );
		} break;
		case IOCTL_ES_OPENCONTENT:
		{
			ret = ES_OpenContent( TitleID, *(u32*)(v[0].data) );
			//dbgprintf("ES:OpenContent(%d):%d\n", *(u32*)(v[0].data), ret );
		} break;
		case IOCTL_ES_OPENTITLECONTENT:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			ret = ES_OpenContent( *iTitleID, *(u32*)(v[2].data) );

			//dbgprintf("ES:OpenTitleContent( %08x-%08x, %d):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), *(u32*)(v[2].data), ret );
		} break;
		case IOCTL_ES_GETTITLEDIR:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x/data", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			memcpy( (u8*)(v[1].data), path, 32 );

			ret = ES_SUCCESS;
			//dbgprintf("ES:GetTitleDataDir(%s):%d\n", v[1].data, ret );
		} break;
		case IOCTL_ES_LAUNCH:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			//dbgprintf("ES:LaunchTitle( %08x-%08x )\n", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			ret = ES_LaunchTitle( (u64*)(v[0].data), (u8*)(v[1].data) );

			//dbgprintf("ES_LaunchTitle Failed with:%d\n", ret );

		} break;
		case IOCTL_ES_SETUID:
		{
			memcpy( &TitleID, (u8*)(v[0].data), sizeof(u64) );

			u16 UID = 0;
			ret = ES_GetUID( &TitleID, &UID );
			if( ret >= 0 )
			{
				ret = SetUID( 0xF, UID );

				if( ret >= 0 )
				{
					_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(TitleID>>32), (u32)TitleID );
					TitleMetaData *TMD = (TitleMetaData *)NANDLoadFile( path, size );
					if( TMD == NULL )
					{
						ret = *size;
					} else {
						ret = _cc_ahbMemFlush( 0xF, TMD->GroupID );
						//if( ret < 0 )
						//	dbgprintf("_cc_ahbMemFlush( %d, %04X ):%d\n", 0xF, TMD->GroupID, ret );
						free( TMD );
					}
				} else {
					dbgprintf("ES:SetUID( 0xF, %04X ):%d\n", UID, ret );
				}
			}

			//dbgprintf("ES:SetUID(%08x-%08x):%d\n", (u32)(TitleID>>32), (u32)TitleID, ret );
		} break;
		case IOCTL_ES_GETTITLEID:
		{
			memcpy( (u8*)(v[0].data), &TitleID, sizeof(u64) );

			ret = ES_SUCCESS;
			//dbgprintf("ES:GetTitleID(%08x-%08x):%d\n", (u32)(*(u64*)(v[0].data)>>32), (u32)*(u64*)(v[0].data), ret );
		} break;
		case IOCTL_ES_GETOWNEDTITLECNT:
		{
			ret = ES_GetNumOwnedTitles( (u32*)(v[0].data) );
			//dbgprintf("ES:GetOwnedTitleCount(%d):%d\n", *(u32*)(v[0].data), ret );
		} break;
		case IOCTL_ES_GETTITLECNT:
		{
			ret = ES_GetNumTitles( (u32*)(v[0].data) );
			//dbgprintf("ES:GetTitleCount(%d):%d\n", *(u32*)(v[0].data), ret );
		} break;
		case IOCTL_ES_GETOWNEDTITLES:
		{
			ret = ES_GetOwnedTitles( (u64*)(v[1].data) );
			//dbgprintf("ES:GetOwnedTitles():%d\n", ret );
		} break;
		case IOCTL_ES_GETTITLES:
		{
			ret = ES_GetTitles( (u64*)(v[1].data) );
			//dbgprintf("ES:GetTitles():%d\n", ret );
		} break;
		/*
			Returns 0 views if title is not installed but always ES_SUCCESS as return
		*/
		case IOCTL_ES_GETVIEWCNT:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/ticket/%08x/%08x.tik", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			s32 fd = IOS_Open( path, 1 );
			if( fd < 0 )
			{
				*(u32*)(v[1].data) = 0;
			} else {
				u32 size = IOS_Seek( fd, 0, SEEK_END );
				*(u32*)(v[1].data) = size / 0x2A4;
				IOS_Close( fd );
			}

			ret = ES_SUCCESS;
			//dbgprintf("ES:GetTicketViewCount( %08x-%08x, %d):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), *(u32*)(v[1].data), ret );
		} break;
		case IOCTL_ES_GETSTOREDTMDSIZE:
		{
			memcpy( iTitleID, (u8*)(v[0].data), sizeof(u64) );

			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(*iTitleID>>32), (u32)(*iTitleID) );

			s32 fd = IOS_Open( path, 1 );
			if( fd < 0 )
			{
				ret = fd;
				*(u32*)(v[1].data) = 0;

			} else {

				fstats *status = (fstats*)malloc( sizeof(fstats) );

				ret = ISFS_GetFileStats( fd, status );
				if( ret < 0 )
				{
					*(u32*)(v[1].data) = 0;
				} else {
					*(u32*)(v[1].data) = status->Size;
				}

				IOS_Close( fd );
				free( status );
			}

			//dbgprintf("ES:GetStoredTMDSize( %08x-%08x, %d ):%d\n", (u32)(*iTitleID>>32), (u32)(*iTitleID), *(u32*)(v[1].data), ret );
		} break;
		case IOCTL_ES_GETSTOREDTMD:
		{
			_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)((*(u64*)(v[0].data))>>32), (u32)((*(u64*)(v[0].data))) );

			s32 fd = IOS_Open( path, 1 );
			if( fd < 0 )
			{
				ret = fd;
			} else {

				ret = IOS_Read( fd, (u8*)(v[2].data), v[2].len );
				if( ret == v[2].len )
					ret = ES_SUCCESS;

				IOS_Close( fd );
			}

			//dbgprintf("ES:GetStoredTMD(%08x-%08x):%d\n", (u32)((*(u64*)(v[0].data))>>32), (u32)((*(u64*)(v[0].data))), ret );
		} break;
		case 0x3B:
		{
			/*
				data:138f0300 len:2560(0xA00)
				data:00000000 len:0(0x0)
				data:138f0fc0 len:216(0xD8)
				data:138f0d40 len:520(0x208)
				data:138f1720 len:4(0x4)
				data:138f0f80 len:20(0x14)
				ES:IOS_Ioctlv( 154 0x3b 4 2 0x138f17f0 )
			*/
			ret = ES_FATAL;
			//dbgprintf("ES:DIVerfiyTikView():%d\n", ret );
		} break;
		case IOCTL_ES_DIVERIFY:
		{
			ret = ES_SUCCESS;

			if( (u32*)(v[4].data) == NULL )		// key
			{
				//dbgprintf("key ptr == NULL\n");
				ret = ES_FATAL;
			} else if( v[4].len != 4 ) {
				//dbgprintf("key len invalid, %d != 4\n", v[4].len );
				ret = ES_FATAL;
			}

			if( (u8*)(v[3].data) == NULL )		// TMD
			{
				//dbgprintf("TMD ptr == NULL\n");
				ret = ES_FATAL;
			} else if( ((*(u16*)(v[3].data+0x1DE))*36+0x1E4) != v[3].len ) {
				//dbgprintf("TMD len invalid, %d != %d\n", ((*(u16*)(v[3].data+0x1DE))*36+0x1E4), v[3].len );
				ret = ES_FATAL;
			}

			if( (u8*)(v[2].data) == NULL )		// tik
			{
				//dbgprintf("tik ptr == NULL\n");
				ret = ES_FATAL;
			} else if( v[2].len != 0x2A4 ) {
				//dbgprintf("tik len invalid, %d != 0x2A4\n", v[2].len );
				ret = ES_FATAL;
			}

			if( (u8*)(v[5].data) == NULL )		//hashes
			{
				////dbgprintf("hashes ptr == NULL\n");
				ret = ES_FATAL;
			 }

			if( ret == ES_SUCCESS )
				ret = ES_DIVerify( &TitleID, (u32*)(v[4].data), (TitleMetaData*)(v[3].data), v[3].len, (char*)(v[2].data), (char*)(v[5].data) );

			////dbgprintf("ES:DIVerfiy():%d\n", ret );
		} break;
		case IOCTL_ES_GETBOOT2VERSION:
		{
			*(u32*)(v[0].data) = 5;
			ret = ES_SUCCESS;
			////dbgprintf("ES:GetBoot2Version(5):%d\n", ret );
		} break;
		case 0x45:
		{
			ret = ES_FATAL;
			////dbgprintf("ES:KoreanKeyCheck():%d\n", ret );
		} break;
		case IOCTL_ES_IMPORTBOOT:
		{
			ret = ES_SUCCESS;
			////dbgprintf("ES:ImportBoot():%d\n", ret );
		} break;

		case 0x50:
		{
//			u32 KeyIDT ALIGNED(32) = 0;
			u32* pKeyIDT ALIGNED(32);

			pKeyIDT = &KeyIDT;

			ret = CreateKey( pKeyIDT, 0, 0 );
			//if( ret < 0 )
			//{
				//dbgprintf("CreateKey( %p, %d, %d ):%d\n",&KeyIDT, 0, 0, ret );
				//dbgprintf("KeyID:%d\n", KeyIDT );
			//}
			//else
			//{

				s32 r = syscall_71( KeyIDT, 8 );
				//if( r < 0 )
				//{
				//	dbgprintf("ES:keyid:%p:%08x\n",&KeyIDT, KeyIDT );
				//	dbgprintf("ES:syscall_71():%d\n", r);
				//}
				
				//dbgprintf(" ES key setting parameters \n");
				hexdump (v[0].data,0x10);
				hexdump (v[1].data,0x10); 
				
				
				ret = syscall_5d( KeyIDT, 0, 4, 1, 0, (void *)(v[0].data), (void *)(v[1].data) );
				//if( ret < 0 )
				//	dbgprintf("syscall_5d( %d, %d, %d, %d, %d, %p, %p ):%d\n", KeyIDT, 0, 4, 1, ret, (void *)(v[0].data), (void *)(v[1].data), ret );
				
			//}
//we will recycle pKeyIDT and use it to transfer the KeyIDT to DI
			pKeyIDT = (u32*)(v[2].data);
			*pKeyIDT = KeyIDT;				
//			memcpy(v[2].data,&KeyIDT,4);

		} break;

		default:
		{
			for( i=0; i<InCount+OutCount; ++i)
			{
				//dbgprintf("data:%p len:%d(0x%X)\n", v[i].data, v[i].len, v[i].len );
				hexdump( (u8*)(v[i].data), v[i].len );
			}
			//dbgprintf("ES:IOS_Ioctlv( %d 0x%x %d %d 0x%p )\n", msg->fd, msg->ioctlv.command, msg->ioctlv.argc_in, msg->ioctlv.argc_io, msg->ioctlv.argv);
			ret = ES_FATAL;
		} break;
	}

	mqueue_ack( (void *)msg, ret);
}
int _main( int argc, char *argv[] )
{
	s32 ret=0;
	struct ipcmessage *message=NULL;
	u8 MessageHeap[0x10];
	u32 MessageQueue=0xFFFFFFFF;

	ignore_logfile = 1;
	
	thread_set_priority( 0, 0x79 );
	thread_set_priority( 0, 0x50 );
	thread_set_priority( 0, 0x10 );

#ifdef DEBUG
	dbgprintf("$IOSVersion: ES: %s %s 64M DEBUG$\n", __DATE__, __TIME__ );
#else
	dbgprintf("$IOSVersion: ES: %s %s 64M Release$\n", __DATE__, __TIME__ );
#endif

	KernelVersion = *(vu32*)0x00003140;
	dbgprintf("ES:KernelVersion:%d\n", KernelVersion );

	dbgprintf("ES:Heap Init...");
	MessageQueue = mqueue_create( MessageHeap, 1 );
	dbgprintf("ok\n");

	device_register( "/dev/es", MessageQueue );

	s32 Timer = TimerCreate( 0, 0, MessageQueue, 0xDEADDEAD );
	dbgprintf("ES:Timer:%d\n", Timer );

	u32 pid = GetPID();
	dbgprintf("ES:GetPID():%d\n", pid );
	ret = SetUID( pid, 0 );
	dbgprintf("ES:SetUID():%d\n", ret );
	ret = _cc_ahbMemFlush( pid, 0 );
	dbgprintf("ES:_cc_ahbMemFlush():%d\n", ret );

	u32 Flag1;
	u16	Flag2;
	GetFlags( &Flag1, &Flag2 );
	dbgprintf("ES:Flag1:%d Flag2:%d\n", Flag1, Flag2 );

	u32 version = GetKernelVersion();
	dbgprintf("ES:KernelVersion:%08X, %d\n", version, (version<<8)>>0x18 );
	
	ret = ISFS_Init();

	dbgprintf("ES:ISFS_Init():%d\n", ret );

	if( ISFS_IsUSB() == FS_ENOENT2 )
	{
		dbgprintf("ES:Found FS-SD\n");
		FSUSB = 0;

		ret = device_register("/dev/sdio", MessageQueue );
#ifdef DEBUG
		dbgprintf("ES:DeviceRegister(\"/dev/sdio\"):%d QueueID:%d\n", ret, queueid );
#endif
	} else {
		dbgprintf("ES:Found FS-USB\n");
		FSUSB = 1;
		// if we start logging 2 fast
		// we will delay the es startup
		// and di won't be able to grab the harddisk
		ignore_logfile = 0;
	}
	if(ISFS_Get_Di_Path( diroot ) == FS_SUCCESS)
	{
		dbgprintf("ES:ISFS_Get_Di_Path = %s\n",diroot);
		diroot[0x1f] = 0;
	}
	else
	{
		dbgprintf("ES:ISFS_Get_Di_Path failure\n");
		strcpy(diroot,"sneek");
	}

	SDStatus = (u32*)malloca( sizeof(u32), 0x40 );

	*SDStatus = 0x00000001;
	HCR = (u32*)malloca( sizeof(u32)*0x30, 0x40 );
	memset32( HCR, 0, sizeof(u32)*0x30 );
	
//Used in Ioctlvs
	path		= (char*)malloca(		0x40,  32 );
	size		= (u32*) malloca( sizeof(u32), 32 );
	iTitleID	= (u64*) malloca( sizeof(u64), 32 );
	
	ES_BootSystem( &TitleID, &KernelVersion );

	dbgprintf("ES:TitleID:%08x-%08x version:%d\n", (u32)((TitleID)>>32), (u32)(TitleID), TitleVersion );

	ret = 0;
	u32 MenuType = 0;
	u32 StartTimer = 1;
	u32 Prii_Setup = 0;

	SMenuInit( TitleID, TitleVersion );
	ignore_logfile = 0;

	if( LoadFont( "/sneek/font.bin" ) )	// without a font no point in displaying the menu
	{
		TimerRestart( Timer, 0, 10000000 );
	}
	
	if( TitleID == 0x0000000100000002LL )
	{
		//Disable SD access for system menu, as it breaks channel/game loading
		//if( *SDStatus == 1 )
		//	*SDStatus = 1;

		MenuType = 1;
		
	} /*else if ( (TitleID >> 32) ==  0x00010000LL ) {
		MenuType = 2;
	}*/

	LoadAndRebuildChannelCache();
	Force_Internet_Test();

	dbgprintf("ES:looping!\n");
		
	while (1)
	{
		ret = mqueue_recv( MessageQueue, (void *)&message, 0);
		if( ret != 0 )
		{
			dbgprintf("ES:mqueue_recv(%d) FAILED:%d\n", MessageQueue, ret);
			return 0;
		}
	
		if( (u32)message == 0xDEADDEAD )
		{
			TimerStop( Timer );

			//proper priiloader will set to obcd
			//on adress 0x8132FFFB which is not 32 bit aligned
			//u32 luc = 0x0;
			if(((*(u32*)(0x0132FFF8) & 0xff) != 0x4f)||((*(u32*)(0x0132FFFC) & 0xffffff00) != 0x62636400))
			//if (luc != 0x4f626364)
			{
				//if we are coming from a prii_setup, the menu still needs to load
				//so we will wait 10 seconds like normal to ensure it's there
				if (Prii_Setup == 1)
				{
					Prii_Setup = 0;
					TimerRestart( Timer, 0, 10000000);	
				}
				else
				{
					if( StartTimer )
					{
						dbgprintf("ES:Normal StartTimer\n"); 
						StartTimer = 0;
						if( MenuType == 1 )
						{
							if( !SMenuFindOffsets( (void*)0x01330000, 0x003D0000 ) )
							{
								dbgprintf("ES:Failed to find all menu patches!\n");
								continue;
							}
						} else if( MenuType == 2 ) {
							if( !SMenuFindOffsets( (void*)0x00000000, 0x01200000 ) )
							{
								dbgprintf("ES:Failed to find all menu patches!\n");
								continue;
							}
						} else {
							continue;
						}
					}
			
					SMenuAddFramebuffer();
					if( MenuType == 1 )
					{
						SMenuDraw();
						SMenuReadPad();
					} else if( MenuType == 2 ) {

						SCheatDraw();
						SCheatReadPad();
					}
					TimerRestart( Timer, 0, 2500 );
				}
			}
			else
			{
				dbgprintf("ES:prii Obcd detected: %x\n",*(u32*)(0x0132FFFC));
				TimerRestart( Timer, 0, 1000000);	
				Prii_Setup = 1;
			}
			//TimerRestart( Timer, 0, 2500 );
			continue;
		}

		//dbgprintf("ES:Command:%02X\n", message->command );
		//dbgprintf("ES:mqueue_recv(%d):%d cmd:%d ioctlv:\"%X\"\n", queueid, ret, message->command, message->ioctlv.command );

		switch( message->command )
		{
			case IOS_OPEN:
			{
				//dbgprintf("ES:mqueue_recv(%d):%d cmd:%d device:\"%s\":%d\n", queueid, ret, message->command, message->open.device, message->open.mode );
				// Is it our device?
				if( strncmp( message->open.device, "/dev/es", 7 ) == 0 )
				{
					ret = ES_FD;
				} else if( strncmp( message->open.device, "/dev/sdio/slot", 14 ) == 0 ) {
					ret = SD_FD;
				} else  {
					ret = FS_ENOENT;
				}
				
				mqueue_ack( (void *)message, ret );
				
			} break;
			
			case IOS_CLOSE:
			{
#ifdef DEBUG
				dbgprintf("ES:IOS_Close(%d)\n", message->fd );
#endif
				if( message->fd == ES_FD || message->fd == SD_FD )
				{
					mqueue_ack( (void *)message, ES_SUCCESS );
					break;
				} else 
					mqueue_ack( (void *)message, FS_EINVAL );

			} break;

			case IOS_READ:
			case IOS_WRITE:
			case IOS_SEEK:
			{
				dbgprintf("ES:Error Read|Write|Seek called!\n");
				mqueue_ack( (void *)message, FS_EINVAL );
			} break;
			case IOS_IOCTL:
			{
				if( message->fd == SD_FD ) 
					SD_Ioctl( message );
				else
					mqueue_ack( (void *)message, FS_EINVAL );

			} break;

			case IOS_IOCTLV:
				if( message->fd == ES_FD )
					ES_Ioctlv( message );
				else if( message->fd == SD_FD ) 
					SD_Ioctlv( message );
				else
					mqueue_ack( (void *)message, FS_EINVAL );
			break;
			
			default:
				dbgprintf("ES:unimplemented/invalid msg: %08x argv[0]:%08x\n", message->command, message->args[0] );
				mqueue_ack( (void *)message, FS_EINVAL );
			break;
		}
	}

	return 0;
}

