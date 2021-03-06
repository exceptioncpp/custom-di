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

#ifndef _SMENU_
#define _SMENU_

#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "gecko.h"
#include "alloc.h"
#include "vsprintf.h"
#include "GCPad.h"
#include "WPad.h"
#include "DI.h"
#include "font.h"
#include "NAND.h"
#include "ES.h"
#include "utils.h"

#define MAX_HITS			64
#define MAX_FB				3
//#define MAX_APPS            50
#define MAX_PATH_SIZE		128

#define MENU_POS_X			20
#define MENU_POS_Y			54

#define VI_INTERLACE		0
#define VI_NON_INTERLACE	1
#define VI_PROGRESSIVE		2

#define VI_NTSC				0
#define VI_PAL				1
#define VI_MPAL				2
#define VI_DEBUG			3
#define VI_DEBUG_PAL		4
#define VI_EUR60			5

#define NANDCFG_SIZE 	0x10
#define NANDDESC_OFF	0x80
#define NANDDI_OFF		0xC0
#define NANDINFO_SIZE	0x100

#define SCROLLTIMER		60
#define MAXSHIFT		64

enum 
{
	AREA_JPN = 0,
	AREA_USA,
	AREA_EUR,
	AREA_KOR = 6,
};

enum 
{
	MAME = 0,
	GC,
	WBFS,
	FST,
	INV,
};

enum SMConfig
{
	CONFIG_PRESS_A				= (1<<0),
	CONFIG_NO_BG_MUSIC			= (1<<1),
	CONFIG_NO_SOUND				= (1<<2),
	CONFIG_MOVE_DISC_CHANNEL	= (1<<3),
	CONFIG_REM_NOCOPY			= (1<<4),
	CONFIG_REGION_FREE			= (1<<5),
	CONFIG_REGION_CHANGE		= (1<<6),
	CONFIG_FORCE_INET			= (1<<7),
	CONFIG_FORCE_EuRGB60		= (1<<8),
	CONFIG_BLOCK_DISC_UPDATE	= (1<<9),
};

typedef struct
{	
	u16 PALVid;
	u16 EULang;
	u16 NTSCVid;
	u16 USLang;
	u32 Config;
	u32 Autoboot;	
	u32 ChNbr;
	u16 Shop1;
	u16 DolNr;
	u64 TitleID;
	u64 RtrnID;
	u8 bootapp[256];
	u8 DOLName[40];
} HacksConfig; 

void SMenuInit( u64 TitleID, u16 TitleVersion );
u32 SMenuFindOffsets( void *ptr, u32 SearchSize );
void SMenuAddFramebuffer( void );
void SMenuDraw( void );
void SMenuReadPad( void );

void SCheatDraw( void );
void SCheatReadPad( void );

void LoadAndRebuildChannelCache();
void __configloadcfg( void );

typedef struct
{
	u64 titleID;
	u8 name[41];
} __attribute__((packed)) ChannelInfo;

typedef struct
{
	u32 numChannels;
	ChannelInfo channels[];
} __attribute__((packed)) ChannelCache;


#endif
