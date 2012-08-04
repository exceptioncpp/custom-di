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
#include "alloc.h"
#include "vsprintf.h"

int HeapID;

void HeapInit( void )
{
	HeapID = HeapCreate( (void*)0x13600000, 0x18000 );
}
void *halloc( u32 size )
{
	void *ptr = HeapAlloc( HeapID, size );
	if( ptr == NULL )
		while(1);

	return ptr;
}
void *halloca( u32 size, u32 align )
{
	void *ptr = HeapAllocAligned( HeapID, size, align );
	if( ptr == NULL )
		while(1);

	return ptr;
}
void hfree( void *ptr )
{
	if( ptr != NULL )
		HeapFree( HeapID, ptr );

	return;
}


void *malloc( u32 size )
{
	void *ptr = HeapAlloc( 0, size );
	if( ptr == NULL )
		while(1);

	return ptr;
}
void *malloca( u32 size, u32 align )
{
	void *ptr = HeapAllocAligned( 0, size, align );
	if( ptr == NULL )
		while(1);
		
	return ptr;
}
void free( void *ptr )
{
	if( ptr != NULL )
		HeapFree( 0, ptr );

	return;
}