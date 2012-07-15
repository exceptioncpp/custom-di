/*

SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

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
#include "alloc.h"

#ifndef MEM2_BSS
#define MEM2_BSS
#endif

u32 s_size;
u32 s_cnt;

DSTATUS disk_initialize(BYTE drv, WORD *ss)
{
	s32 r;
	int i;

	while(1)
	{	
		udelay(50000);
		
		dbgprintf("FS: Initializing TinyEHCI...\n");
		tiny_ehci_init();

		dbgprintf("FS: Discovering EHCI devices...\n");
		
		i = 1;

		while(ehci_discover() == -ENODEV)
		{
			dbgprintf("FS: Waiting for device to become ready (%d)\n", i);
			udelay(4000);
			i++;
		}

		dbgprintf("done!\n");
	
		r = USBStorage_Init();
		
		if(r == 0)
			break;
	}		
	
	s_cnt = USBStorage_Get_Capacity(&s_size);
	
	*ss = s_size;
	
	dbgprintf("FS: Drive size: %dMB SectorSize:%d\n", s_cnt / 1024 * s_size / 1024, s_size);

	return r;
}

DSTATUS disk_status (BYTE drv)
{
	(void)drv;
	return 0;
}

DRESULT disk_read (BYTE drv, BYTE *buff, DWORD sector, BYTE count)
{
	u32 *buffer = malloca( count*s_size, 0x40 );

	if( USBStorage_Read_Sectors( sector, count, buffer ) != 1 )
	{
		//dbgprintf("FS: Failed to read disc: Sector:%d Count:%d dst:%p\n", sector, count, buff );
		return RES_ERROR;
	}

	memcpy( buff, buffer, count*s_size );
	free( buffer );

	return RES_OK;
}

DRESULT disk_write (BYTE drv, const BYTE *buff, DWORD sector, BYTE count)
{
	int i;
	u32 *buffer = malloca( count*s_size, 0x40 );
	memcpy( buffer, buff, count*s_size );

	if( USBStorage_Write_Sectors( sector, count, buffer ) != 1 )
	{
		//dbgprintf("FS: Failed to write disc: Sector:%d Count:%d dst:%p\n", sector, count, buff );
		return RES_ERROR;
	}
	free( buffer );

	return RES_OK;
}
