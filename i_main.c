// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_main.c,v 1.4 1997/02/03 22:45:10 b1 Exp $";


#include "SDL.h"

#include "doomdef.h"

#include "m_argv.h"
#include "d_main.h"

#ifdef _EE

#include <libmc.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <kernel.h>
#include <sys/stat.h>
#include <ps2_all_drivers.h>
#include <iopcontrol.h>

static void reset_IOP() {
	SifInitRpc(0);
	#if !defined(DEBUG) || defined(BUILD_FOR_PCSX2)
	/* Comment this line if you don't wanna debug the output */
	while(!SifIopReset("", 0)){};
	#endif
	while(!SifIopSync()){};
	SifInitRpc(0);
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
}

static void initMC(void)
{
   int ret;
   // mc variables
   int mc_Type, mc_Free, mc_Format;


   printf("initMC: Initializing Memory Card\n");

   ret = mcInit(MC_TYPE_XMC);

   if( ret < 0 ) {
	printf("initMC: failed to initialize memcard server.\n");
   } else {
       printf("initMC: memcard server started successfully.\n");
   }

   // Since this is the first call, -1 should be returned.
   // makes me sure that next ones will work !
   mcGetInfo(0, 0, &mc_Type, &mc_Free, &mc_Format); 
   mcSync(MC_WAIT, NULL, &ret);
}

static void waitUntilUsbDeviceIsReady() {
  struct stat buffer;
  int ret = -1;
  int retries = 50;

  while (ret != 0 && retries > 0) {
    ret = stat("mass:/", &buffer);
    /* Wait until the device is ready */
    nopdelay();

    retries--;
  }
}

#endif

int
main
( int		argc,
  char**	argv ) 
{ 
    myargc = argc; 
    myargv = argv; 

#ifdef _EE
    reset_IOP();
    init_fileXio_driver();
    init_memcard_driver(true);
    initMC();
    init_usb_driver(true);
    waitUntilUsbDeviceIsReady();
#endif

    D_DoomMain (); 

    return 0;
} 
