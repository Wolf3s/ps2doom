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
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
// 
// static const char rcsid[] = "$Id: d_main.c,v 1.8 1997/02/03 22:45:09 b1 Exp $";
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include <SDL/SDL_timer.h>
#include <kernel.h>
#include <tamtypes.h>
#include <string.h>


#define	BGCOLOR		7
#define	FGCOLOR		8


extern int mixer_period;
extern int SAMPLERATE;
extern int use_hdd;
extern const char *hdd_wads_folder;
#include <libpad.h>
#include <debug.h>
/// cosmito
void checkForWadFile(char* wadname, char** foundwadfiles, char* foundfile, int* foundwadfiles_index, int* nWadsFound);
char currentWadName[20];

#define R_OK	4
// file access
static int access(char *file, int mode)
{
	FILE *test_fp;
	test_fp = fopen(file, "r");
	if ( test_fp != NULL ) {
		fclose(test_fp);
		return(0);
	}
	return(-1);
}

#include "include/doomdef.h"
#include "include/doomstat.h"
#include "include/dstrings.h"
#include "include/sounds.h"
#include "include/z_zone.h"
#include "include/w_wad.h"
#include "include/s_sound.h"
#include "include/v_video.h"
#include "include/f_finale.h"
#include "include/f_wipe.h"
#include "include/m_argv.h"
#include "include/m_misc.h"
#include "include/m_menu.h"
#include "include/i_system.h"
//#include "include/i_sound.h"
#include "include/l_sound_sdl.h"
#include "include/g_game.h"
#include "include/hu_stuff.h"
#include "include/wi_stuff.h"
#include "include/st_stuff.h"
#include "include/am_map.h"
#include "include/p_setup.h"
#include "include/r_local.h"
#include "include/d_main.h"
#include "include/pad_support.h"
char fullPath[256];

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop (void);


char*		wadfiles[MAXWADFILES];
char*           iwadfile;

boolean		devparm;	// started game with -devparm
boolean         nomonsters;	// checkparm of -nomonsters
boolean         respawnparm;	// checkparm of -respawn
boolean         fastparm;	// checkparm of -fast

boolean         drone;

boolean		singletics = false; // debug flag to cancel adaptiveness

extern  boolean	inhelpscreens;

skill_t		startskill;
int             startepisode;
int		startmap;
boolean		autostart;

FILE*		debugfile;

boolean		advancedemo;




char		wadfile[1024];		// primary wad file
char		mapdir[1024];           // directory of development maps
char		basedefault[1024];      // default file


void D_CheckNetGame (void);
void D_ProcessEvents (void);
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo (void);


//
// EVENT HANDLING
//
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//
event_t         events[MAXEVENTS];
int             eventhead;
int 		eventtail;


//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent (event_t* ev)
{
    events[eventhead] = *ev;
    eventhead = (++eventhead)&(MAXEVENTS-1);
}


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents (void)
{
    event_t*	ev;
	
    // IF STORE DEMO, DO NOT ACCEPT INPUT
    if ( ( gamemode == commercial )
	 && (W_CheckNumForName("map01")<0) )
      return;
	
    for ( ; eventtail != eventhead ; eventtail = (++eventtail)&(MAXEVENTS-1) )
    {
	ev = &events[eventtail];
	if (M_Responder (ev))
	    continue;               // menu ate the event
	G_Responder (ev);
    }
}




//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t     wipegamestate = GS_DEMOSCREEN;
extern  boolean setsizeneeded;
extern  int             showMessages;
void R_ExecuteSetViewSize (void);

void D_Display (void)
{
    static  boolean		viewactivestate = false;
    static  boolean		menuactivestate = false;
    static  boolean		inhelpscreensstate = false;
    static  boolean		fullscreen = false;
    static  gamestate_t		oldgamestate = -1;
    static  int			borderdrawcount;
    int				nowtime;
    int				tics;
    int				wipestart;
    int				y;
    boolean			done;
    boolean			wipe;
    boolean			redrawsbar;

    if (nodrawers)
        return;                    // for comparative timing / profiling

    redrawsbar = false;

    // change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize ();
        oldgamestate = -1;                      // force background redraw
        borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate)
    {
        wipe = true;
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    }
    else
        wipe = false;

    if (gamestate == GS_LEVEL && gametic)
        HU_Erase();

    // do buffered drawing
    switch (gamestate)
    {
    case GS_LEVEL:
        if (!gametic)
            break;
        if (automapactive)
            AM_Drawer ();
        if (wipe || (viewheight != 200 && fullscreen) )
            redrawsbar = true;
        if (inhelpscreensstate && !inhelpscreens)
            redrawsbar = true;              // just put away the help screen
        ST_Drawer (viewheight == 200, redrawsbar );
        fullscreen = viewheight == 200;
        break;

    case GS_INTERMISSION:
        WI_Drawer ();
        break;

    case GS_FINALE:
        F_Drawer ();
        break;

    case GS_DEMOSCREEN:
        D_PageDrawer ();
        break;
    }

    // draw buffered stuff to screen
    I_UpdateNoBlit ();

    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
        R_RenderPlayerView (&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic)
        HU_Drawer ();

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
        I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
        viewactivestate = false;        // view was not active
        R_FillBackScreen ();    // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != 320)
    {
        if (menuactive || menuactivestate || !viewactivestate)
            borderdrawcount = 3;
        if (borderdrawcount)
        {
            R_DrawViewBorder ();    // erase old menu stuff
            borderdrawcount--;
        }

    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic
    if (paused)
    {
        if (automapactive)
            y = 4;
        else
            y = viewwindowy+4;
        V_DrawPatchDirect(viewwindowx+(scaledviewwidth-68)/2,
            y,0,W_CacheLumpName ("M_PAUSE", PU_CACHE));
    }


    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    NetUpdate ();         // send out any new accumulation


    // normal update
    if (!wipe)
    {
        I_FinishUpdate ();              // page flip or blit buffer
        return;
    }

    // wipe update
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
            nowtime = I_GetTime ();
            tics = nowtime - wipestart;
DOMULTITASK;
        } while (!tics);
        wipestart = nowtime;
        done = wipe_ScreenWipe(wipe_Melt
            , 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
        I_UpdateNoBlit ();
        M_Drawer ();                            // menu is drawn even on top of wipes
        I_FinishUpdate ();                      // page flip or blit buffer
DOMULTITASK;        
    } while (!done);
}

//
//  D_DoomLoop
//
extern  boolean         demorecording;

void D_DoomLoop (void)
{
    int debugparam;
    debugparam = M_CheckParm ("-debugfile");

    if (demorecording)
    {
        G_BeginRecording ();
    }
		
    if (debugparam)
    {
	  char    filename[20];
	  sprintf (filename,"debug%i.txt",consoleplayer);
	  printf ("debug output to: %s\n",filename);
	  debugfile = fopen (filename,"w");
    }
	
    I_InitGraphics ();

    while (1)
    {
	// frame syncronous IO operations
	I_StartFrame ();                
	
	// process one or more tics
	if (singletics)
	{
	    I_StartTic ();
DOMULTITASK;
	    D_ProcessEvents ();
DOMULTITASK;
        G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
DOMULTITASK;
        if (advancedemo)
		D_DoAdvanceDemo ();
DOMULTITASK;
        M_Ticker ();
DOMULTITASK;
        G_Ticker ();
DOMULTITASK;
        gametic++;
	    maketic++;
	}
	else
	{
	    TryRunTics (); // will run at least one tic
	}

	S_UpdateSounds (players[consoleplayer].mo);// move positional sounds
DOMULTITASK;
	// Update display, next frame, with current state.
	D_Display ();
    
    ///
	RotateThreadReadyQueue(42);
    ///

    }
}



//
//  DEMO LOOP
//
int             demosequence;
int             pagetic;
char                    *pagename;


//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
	D_AdvanceDemo ();
}



//
// D_PageDrawer
//
void D_PageDrawer (void)
{
    V_DrawPatch (0,0, 0, W_CacheLumpName(pagename, PU_CACHE));
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
 void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    if ( gamemode == retail )
      demosequence = (demosequence+1)%7;
    else
      demosequence = (demosequence+1)%6;
    
    switch (demosequence)
    {
      case 0:
	if ( gamemode == commercial )
	    pagetic = 35 * 11;
	else
	    pagetic = 170;
	gamestate = GS_DEMOSCREEN;
	pagename = "TITLEPIC";
	if ( gamemode == commercial )
	  S_StartMusic(mus_dm2ttl);
	else
	  S_StartMusic (mus_intro);
	break;
      case 1:
	G_DeferedPlayDemo ("demo1");
	break;
      case 2:
	pagetic = 200;
	gamestate = GS_DEMOSCREEN;
	pagename = "CREDIT";
	break;
      case 3:
	G_DeferedPlayDemo ("demo2");
	break;
      case 4:
	gamestate = GS_DEMOSCREEN;
	if ( gamemode == commercial)
	{
	    pagetic = 35 * 11;
	    pagename = "TITLEPIC";
	    S_StartMusic(mus_dm2ttl);
	}
	else
	{
	    pagetic = 200;

	    if ( gamemode == retail )
	      pagename = "CREDIT";
	    else
	      pagename = "HELP2";
	}
	break;
      case 5:
	G_DeferedPlayDemo ("demo3");
	break;
        // THE DEFINITIVE DOOM Special Edition demo
      case 6:
	G_DeferedPlayDemo ("demo4");
	break;
    }
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
}




//      print title for every printed line
char            title[128];



//
// D_AddFile
//
void D_AddFile (char *file)
{
    int     numwadfiles;
    char    *newfile;
    for (numwadfiles = 0 ; wadfiles[numwadfiles] ; numwadfiles++)
	;

    newfile = malloc (strlen(file)+1);
    strcpy (newfile, file);
	
    wadfiles[numwadfiles] = newfile;
    //printf("%s\n", newfile);
}

//check if file exists
static int FileExists(char *filename)
{
   //c++ std::fstream
   FILE *fstream;
   
   fstream = fopen(filename, "r");

   if (fstream != NULL) 
   { 
     fclose(fstream); return 1; 
   } 
   
   else 
   { 
	 return 0; 
   }
}

struct
{
    char *name;
    GameMission_t mission;
} iwads[] = 
{
    {"doom.wad",     doom},
    {"doom2.wad",    doom2},
    {"tnt.wad",      pack_tnt},
    {"plutonia.wad", pack_plut},
    {"doom1.wad",    doom},
};

/*
 Search a directory to try to find an IWAD
 Returns non-zero if successful
*/
static int Find_IWADS_Dir(char *dir)
{
   int i; 
   int result;

   result = 0; 
   
   for(i=0; i<sizeof(iwads) / sizeof(*iwads); i++)
   {
    //todo:Zalloc here
    char *filename = malloc(strlen(dir) + strlen(iwads[i].name) + 3);


      sprintf(filename, "%s/%s", dir, iwads[i].name);

      if (FileExists(filename)) 
      { 
	iwadfile = filename; 
	gamemission = iwads[i].mission; 
	D_AddFile(filename); 
	result = 1; 
	break; 
      }
      
      else 
      { 
	free(filename); 
      }
   
   }
    return result;
}

/*
  When given an IWAD with the '-iwad' parameter, this void declaration will try to indentify
by his name
*/
static void IdentifyIWADByName(char *name)
{
   int i;
   
   for (i=0; i<sizeof(iwads) / sizeof(*iwads);++i) 
   { 
      if(strlen(name) < strlen(iwads[i].name))    continue;

      // Check if it ends in this IWAD name.
      if (!strcasecmp(name + strlen(name) - strlen(iwads[i].name), iwads[i].name)) 
      { 
	    gamemission = iwads[i].mission; 
	    break; 
      }
   }
  gamemission = none;
}

void checkForWadFile(char* wadname, char** foundwadfiles, char* foundfile, int* foundwadfiles_index, int* nWadsFound)
{
    if ( !access (wadname, R_OK) )
    {
        scr_printf("    %s\n", wadname);
        *nWadsFound = *nWadsFound + 1;

        foundfile = malloc( strlen(wadname)+1 );
        sprintf(foundfile, "%s", wadname);
        foundwadfiles[*foundwadfiles_index] = foundfile;
        *foundwadfiles_index = *foundwadfiles_index + 1;
    }
}

void IdentifyVersionAndSelect(void) // cosmito
{
    char*	doom1wad;
    char*	doomwad;
    char*	doomuwad;
    char*	doom2wad;

    char*	doom2fwad;
    char*	plutoniawad;
    char*	tntwad;

    char *home;
    char *doomwaddir;
	
    char *doomiwaddir; 
    int result; 
     
    /******************************************************************************************** 
    **   parameter commands in this case it will be just the name of parmeter like int comdev; **
    **   instead of a command like M_Checkparam = ("-comdev")                                  **
    *********************************************************************************************/
    int shdev;
    int regdev;
    int comdev;
    int iwadparm;

    doomiwaddir = getenv("DOOMIWADDIR"); 
    iwadparm = M_CheckParm("-iwad");
    shdev = M_CheckParm("-shdev");
    regdev = M_CheckParm("-regdev");
    comdev = M_CheckParm("-comdev");

#ifdef _EE
    extern char fullPath[256];
#endif

    doomwaddir = getenv("DOOMWADDIR");

	#ifdef _EE
        doomwaddir = fullPath;
	#else
    if (!doomwaddir)
		doomwaddir = "./";
	#endif

    // Commercial.
    doom2wad = malloc(strlen(doomwaddir)+1+9+1+5);
    sprintf(doom2wad, "%sdoom2.wad", doomwaddir);

    // Retail.
    doomuwad = malloc(strlen(doomwaddir)+1+8+1+5);
    sprintf(doomuwad, "%sdoomu.wad", doomwaddir);
    
    // Registered.
    doomwad = malloc(strlen(doomwaddir)+1+8+1+5);
    sprintf(doomwad, "%sdoom.wad", doomwaddir);
    
    // Shareware.
    doom1wad = malloc(strlen(doomwaddir)+1+9+1+5);
    sprintf(doom1wad, "%sdoom1.wad", doomwaddir);

     // Bug, dear Shawn.
    // Insufficient malloc, caused spurious realloc errors.
    plutoniawad = malloc(strlen(doomwaddir)+1+/*9*/12+1+5);
    sprintf(plutoniawad, "%splutonia.wad", doomwaddir);

    tntwad = malloc(strlen(doomwaddir)+1+9+1+5);
    sprintf(tntwad, "%stnt.wad", doomwaddir);


    // French stuff.
    doom2fwad = malloc(strlen(doomwaddir)+1+10+1+5);
    sprintf(doom2fwad, "%sdoom2f.wad", doomwaddir);

    home = getenv("HOME");
    if (!home)
      home = ".";
    sprintf(basedefault, "%s.doomrc", home);
 
    if (shdev)
    {
        gamemode = shareware;
        devparm = true;
        D_AddFile (DEVDATA"doom1.wad");
        D_AddFile (DEVMAPS"data_se/texture1.lmp");
        D_AddFile (DEVMAPS"data_se/pnames.lmp");
        strcpy (basedefault,DEVDATA"default.cfg");
        return;
    }

    if (regdev)
    {
        gamemode = registered;
        devparm = true;
        D_AddFile (DEVDATA"doom.wad");
        D_AddFile (DEVMAPS"data_se/texture1.lmp");
        D_AddFile (DEVMAPS"data_se/texture2.lmp");
        D_AddFile (DEVMAPS"data_se/pnames.lmp");
        strcpy (basedefault,DEVDATA"default.cfg");
        return;
    }

    if (comdev)
    {
        gamemode = commercial;
        devparm = true;
 
        D_AddFile (DEVDATA"plutonia.wad");
        D_AddFile (DEVDATA"tnt.wad");
        D_AddFile (DEVDATA"doom2.wad");
        D_AddFile (DEVDATA"doom.wad");
        D_AddFile (DEVMAPS"cdata/texture1.lmp");
        D_AddFile (DEVMAPS"cdata/pnames.lmp");
        strcpy (basedefault,DEVDATA"default.cfg");
        return;
    }


    if(iwadparm)
    {
     iwadfile = myargv[iwadparm + 1]; 
     if (gamemission == none)
     {
      int i;

      for (i=0; i<numlumps; ++i) 
      { 
        if (!strncasecmp(lumpinfo[i].name, "MAP01", 8)) 
        { 
	     gamemission = doom2; 
	     break; 
        } 
      
        else if (!strncasecmp(lumpinfo[i].name, "E1M1", 8)) 
        { 
	       gamemission = doom; break; 
        } 
      }  
    }

    if (gamemission == none) 
    { 
     // Still no idea. I don't think this is going to work.  
     I_Error("Unknown or invalid IWAD file."); 
    }

    if (gamemission == doom) 
    {
      // Doom 1. But which version?
      if (W_CheckNumForName("E4M1") > 0) 
      { 
        // Ultimate Doom
        gamedescription = "The Ultimate DOOM"; 
        gamemode = retail; 
      }  

    else if(W_CheckNumForName("E3M1") > 0) 
    { 
	   gamedescription = "DOOM Registered"; 
	   gamemode = registered; 
    }

      else 
      { 
        gamedescription = "DOOM Shareware"; 
        gamemode = shareware; 
      }
    }
  
    else 
    {
      //Doom 2 of some kind.  But which mission?
      gamemode = commercial;
      if (gamemission == doom2) 
	  gamedescription = "DOOM 2: Hell on Earth"; 
      else if (gamemission == pack_plut) 
      gamedescription = "DOOM 2: Plutonia Experiment"; 
      else if (gamemission == pack_tnt) 
      gamedescription = "DOOM 2: TNT - Evilution"; 
    }
  
    printf(gamedescription, "%s\n");
    D_AddFile(iwadfile); 
    IdentifyIWADByName(iwadfile); 
    result = 1;
         
     if (doomiwaddir != NULL) 
     {
      result = Find_IWADS_Dir(doomiwaddir);
     }
  
     else if (result == 0)
     {
       result = Find_IWADS_Dir(".") || Find_IWADS_Dir("mass0:/ps2doom") || Find_IWADS_Dir("pfs0:/ps2doom");
     }
     
     else
     {
       I_Error("Game mode indeterminate. No IWAD file was found. Try\n" "specifying one with the '-iwad' command line parameter.\n");
     }    
    }

    /// pad init
    int ret;
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) 
    {
      printf("sifLoadModule sio failed: %d\n", ret);
      SleepThread();
    }    

    ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
    if (ret < 0) 
    {
        printf("sifLoadModule pad failed: %d\n", ret);
        SleepThread();
    }

    printf("padInit(0);\n");
    padInit(0);

    int port = 0; // 0 -> Connector 1, 1 -> Connector 2
    printf("int port = 0;\n");

    int slot = 0; // Always zero if not using multitap

    printf("PortMax: %d\n", padGetPortMax());
    printf("SlotMax: %d\n", padGetSlotMax(port));

    if((ret = padPortOpen(port, slot, padBuf)) == 0) {
        printf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }
    
    if(!initializePad(port, slot)) {
        printf("pad initalization failed!\n");
        SleepThread();
    }

    int ymin = 6, nWadsFound = 0;
    int y = ymin;

    char* foundwadfiles[MAXWADFILES];
    char* foundfile = NULL;
    int foundwadfiles_index = 0;

    checkForWadFile(doom2fwad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);
    checkForWadFile(doom2wad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);
    checkForWadFile(plutoniawad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);
    checkForWadFile(tntwad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);
    checkForWadFile(doomuwad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);
    checkForWadFile(doomwad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);
    checkForWadFile(doom1wad, foundwadfiles, foundfile, &foundwadfiles_index, &nWadsFound);

    int index = 0;
    int ymax = y + nWadsFound - 1;

    if (nWadsFound > 0)
    {

        int butres = 0;
        u32 old_pad = 0;
        u32 new_pad = 0;

        int timeout = 100;     // TBD : read from config?
        if (nWadsFound == 1)
            timeout = 0;

        int wait_for_user = 0;
        while(timeout != 0 || wait_for_user == 1)
        {
            butres = padUtils_ReadButtonWait(port, slot, old_pad, new_pad);
            if(butres != 0)
            {
                wait_for_user = 1;
            }

            if(butres == PAD_UP)
            {
                y--;
                if(y < ymin)
                    y = ymin;
              
	    }
            else if(butres == PAD_DOWN)
            {
                y++;
                if(y > ymax)
                    y = ymax;
	    }
            else if(butres == PAD_CROSS || butres == PAD_CIRCLE)
            {
                break;
            }

           SDL_SYS_TimerInit();
            if(timeout != 0)
                timeout--;
        }

        padPortClose(port, slot);

        // find out which wad selected
        index = y - ymin;
        char* selected_wad = foundwadfiles[index];


        if ( strcmp(doom1wad, selected_wad) == 0 )
        {
            gamemode = shareware;
            sprintf(currentWadName, "doom1sav");
            D_AddFile (doom1wad);
            return;
        }

        if ( strcmp(doom2fwad, selected_wad) == 0 )
        {
            gamemode = commercial;
            // C'est ridicule!
            // Let's handle languages in config files, okay?
            language = french;
            printf("French version\n");

            //// halves sample playing frequency to avoid lack of memory for the upsampled sounds
            //mixer_period = 2;
            //SAMPLERATE = SAMPLERATE	/ 2;
            //printf("doom2f detected : halving sample playing frequency to %dHz **************** \n", SAMPLERATE);

            D_AddFile (doom2fwad);
            sprintf(currentWadName, "doom2fsav");
            return;
        }

        if ( strcmp(doom2wad, selected_wad) == 0 )
        {
            gamemode = commercial;

            // halves sample playing frequency to avoid lack of memory for the upsampled sounds
            //mixer_period = 2;
            //SAMPLERATE = SAMPLERATE	/ 2;
            //printf("doom2 detected : halving sample playing frequency to %dHz **************** \n", SAMPLERATE);

            sprintf(currentWadName, "doom2sav");
            //printf("\n\n           ->> %s\n\n", currentWadName);

            D_AddFile (doom2wad);
            return;
        }

        if ( strcmp(plutoniawad, selected_wad) == 0 )
        {
            gamemode = commercial;
            sprintf(currentWadName, "plutoniasav");
            D_AddFile (plutoniawad);
            return;
        }

        if ( strcmp(tntwad, selected_wad) == 0 )
        {
            gamemode = commercial;
            sprintf(currentWadName, "tntsav");

            D_AddFile (tntwad);
            return;
        }

        if ( strcmp(doomuwad, selected_wad) == 0 )
        {
            gamemode = retail;
            sprintf(currentWadName, "doomusav");
            D_AddFile (doomuwad);
            return;
        }

        if ( strcmp(doomwad, selected_wad) == 0 )
        {
            gamemode = registered;
            sprintf(currentWadName, "doomsav");
            D_AddFile (doomwad);
            return;
        }
    }

    if(nWadsFound == 0)
    {
        printf("Game mode indeterminate.\n");
        gamemode = indetermined;
        sprintf(currentWadName, "indetersav");
        padPortClose(port, slot);

        // We don't abort. Let's see what the PWAD contains.
        //exit(1);
        //I_Error ("Game mode indeterminate\n");
    }
}

//
// Find a Response File
//
void FindResponseFile (void)
{
    int             i;
#define MAXARGVS        100
	
    for (i = 1;i < myargc;i++)
	if (myargv[i][0] == '@')
	{
	    FILE *          handle;
	    int             size;
	    int             k;
	    int             index;
	    int             indexinfile;
	    char    *infile;
	    char    *file;
	    char    *moreargs[20];
	    char    *firstargv;
			
	    // READ THE RESPONSE FILE INTO MEMORY
	    handle = fopen (&myargv[i][1],"rb");
	    if (!handle)
	    {
		printf ("\nNo such response file!");
		exit(1);
	    }
	    printf("Found response file %s!\n",&myargv[i][1]);
	    fseek (handle,0,SEEK_END);
	    size = ftell(handle);
	    fseek (handle,0,SEEK_SET);
	    file = malloc (size);
	    fread (file,size,1,handle);
	    fclose (handle);
			
	    // KEEP ALL CMDLINE ARGS FOLLOWING @RESPONSEFILE ARG
	    for (index = 0,k = i+1; k < myargc; k++)
		moreargs[index++] = myargv[k];
			
	    firstargv = myargv[0];
	    myargv = malloc(sizeof(char *)*MAXARGVS);
	    memset(myargv,0,sizeof(char *)*MAXARGVS);
	    myargv[0] = firstargv;
			
	    infile = file;
	    indexinfile = k = 0;
	    indexinfile++;  // SKIP PAST ARGV[0] (KEEP IT)
	    do
	    {
		myargv[indexinfile++] = infile+k;
		while(k < size &&
		      ((*(infile+k)>= ' '+1) && (*(infile+k)<='z')))
		    k++;
		*(infile+k) = 0;
		while(k < size &&
		      ((*(infile+k)<= ' ') || (*(infile+k)>'z')))
		    k++;
	    } while(k < size);
			
	    for (k = 0;k < index;k++)
		myargv[indexinfile++] = moreargs[k];
	    myargc = indexinfile;
	
	    // DISPLAY ARGS
	    printf("%d command-line args:\n",myargc);
	    for (k=1;k<myargc;k++)
		printf("%s\n",myargv[k]);

	    break;
	}
}

//TBD: Put all iwad functions here

//
// D_DoomMain
//
void D_DoomMain (void)
{
    
    int p;
    
    //command line parameters
    int nomonsters;
    int respawn;
    int fastparam;
    int devparam; 
    int altdeath;
    int deathmatch;
    int turbo;
    int wart;
    int playdemo;
    int timedemo;    
    int skill;	
    int episode;
    int timer;    
    int time;
    int avg;
    int warp;
    int statcopy;
    int record;
    int loadgame;
    int cdrom;
    char file[256];

    //todo: add the switch case of pspdoom here 
    nomonsters = M_CheckParm ("-nomonsters");
    respawn = M_CheckParm ("-respawn");
    fastparm = M_CheckParm ("-fast");
    devparm = M_CheckParm ("-devparm");
    altdeath = M_CheckParm ("-altdeath");
    deathmatch = M_CheckParm ("-deathmatch");
    wart = M_CheckParm ("-wart");
    turbo = M_CheckParm ("-turbo");
    file[256] = M_CheckParm ("-file");
    playdemo = M_CheckParm ("-playdemo");
    skill = M_CheckParm ("-skill");
    episode = M_CheckParm ("-episode");
    timer = M_CheckParm ("-timer");
    avg = M_CheckParm ("-avg");
    warp = M_CheckParm ("-warp");
    // start the apropriate game based on parms
    record = M_CheckParm ("-record");
    playdemo = M_CheckParm ("-playdemo");
    // check for a driver that wants intermission stats
    statcopy = M_CheckParm("-statcopy");
    timedemo = M_CheckParm ("-timedemo");
    loadgame = M_CheckParm ("-loadgame");
    cdrom = M_CheckParm ("-cdrom");
    FindResponseFile();
	
    IdentifyVersionAndSelect();

    setbuf (stdout, NULL);
    modifiedgame = false;


    if (altdeath)
	deathmatch = 2;
    else if (deathmatch)
	deathmatch = 1;

    switch ( gamemode )
    {
      case retail:
	sprintf (title,
		 "                         "
		 "The Ultimate DOOM Startup v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
      case shareware:
	sprintf (title,
		 "                            "
		 "DOOM Shareware Startup v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
      case registered:
	sprintf (title,
		 "                            "
		 "DOOM Registered Startup v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
      case commercial:
	sprintf (title,
		 "                         "
		 "DOOM 2: Hell on Earth v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
/*FIXME
       case pack_plut:
	sprintf (title,
		 "                   "
		 "DOOM 2: Plutonia Experiment v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
      case pack_tnt:
	sprintf (title,
		 "                     "
		 "DOOM 2: TNT - Evilution v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
*/
      default:
	sprintf (title,
		 "                     "
		 "Public DOOM - v%i.%i"
		 "                           ",
		 VERSION_NUM/100,VERSION_NUM%100);
	break;
    }
    
    scr_printf ("%s\n",title);

    if (devparm)
	printf(D_DEVSTR);
    
    
    // turbo option
    if (turbo)
    {
	 int     scale = 200;
	 extern int forwardmove[2];
	 extern int sidemove[2];
	
	if (p<myargc-1)
	    scale = atoi (myargv[p+1]);
	if (scale < 10)
	    scale = 10;
	if (scale > 400)
	    scale = 400;
	scr_printf ("turbo scale: %i%%\n",scale);
	forwardmove[0] = forwardmove[0]*scale/100;
	forwardmove[1] = forwardmove[1]*scale/100;
	sidemove[0] = sidemove[0]*scale/100;
	sidemove[1] = sidemove[1]*scale/100;
    }
    
    // add any files specified on the command line with -file wadfile
    // to the wad list
    //
    // convenience hack to allow -wart e m to add a wad file
    // prepend a tilde to the filename so wadfile will be reloadable
    
    if (wart)
    {
	myargv[p][4] = 'p';     // big hack, change to -warp

	// Map name handling.
	switch (gamemode )
	{
	  case shareware:
	  case retail:
	  case registered:
	    sprintf (file,"~"DEVMAPS"E%cM%c.wad",
		     myargv[p+1][0], myargv[p+2][0]);
	    printf("Warping to Episode %s, Map %s.\n",
		   myargv[p+1],myargv[p+2]);
	    break;
	    
	  case commercial:
	  default:
	    p = atoi (myargv[p+1]);
	    if (p<10)
	      sprintf (file,"~"DEVMAPS"cdata/map0%i.wad", p);
	    else
	      sprintf (file,"~"DEVMAPS"cdata/map%i.wad", p);
	    break;
	}
	D_AddFile (file);
    }

    if (file)
    {
	// the parms after p are wadfile/lump names,
	// until end of parms or another - preceded parm
	modifiedgame = true;            // homebrew levels
	while (++p != myargc && myargv[p][0] != '-')
	    D_AddFile (myargv[p]);
    }
    



    
    if (!playdemo)
	timedemo = M_CheckParm ("-timedemo");

    if (timedemo && timedemo < myargc-1)
    {
	  sprintf (file,"%s.lmp", myargv[p+1]);
	  D_AddFile (file);
	  printf("Playing demo %s.lmp.\n",myargv[p+1]);
    }
    
    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;


    
    if (skill && skill < myargc-1)
    {
	startskill = myargv[p+1][0]-'1';
	autostart = true;
    }


    
    if (episode && episode < myargc-1)
    {
	 startepisode = myargv[p+1][0]-'0';
	 startmap = 1;
	 autostart = true;
    }
	
 
    
    if (timer && timer < myargc-1 && deathmatch)
    {
	
	time = atoi(myargv[p+1]);
	printf("Levels will end after %d minute",time);
	if (time>1)
	    printf("s");
	printf(".\n");
    }
    

    
    if (avg && avg < myargc-1 && deathmatch)
	printf("Austin Virtual Gaming: Levels will end after 20 minutes\n");
    
    if (p && p < myargc-1)
    {
	if (gamemode == commercial)
	    startmap = atoi (myargv[p+1]);
	else
	{
	    startepisode = myargv[p+1][0]-'0';
	    startmap = myargv[p+2][0]-'0';
	}
	autostart = true;
    }
    
    // init subsystems
    printf ("V_Init: allocate screens.\n");
    V_Init ();

	#ifdef _EE
		SDL_SYS_TimerInit();
	#endif


    printf ("M_LoadDefaults: Load system defaults.\n");
    M_LoadDefaults ();              // load before initing other systems

    printf ("Z_Init: Init zone memory allocation daemon. \n");
    Z_Init ();

    printf ("W_Init: Init WADfiles.\n");


    W_InitMultipleFiles (wadfiles);
   

    // Check for -file in shareware
    if (modifiedgame)
    {
	// These are the lumps that will be checked in IWAD,
	// if any one is not present, execution will be aborted.
	char name[23][8]=
	{
	    "e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
	    "e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
	    "dphoof","bfgga0","heada1","cybra1","spida1d1"
	};
	int i;
	
	if ( gamemode == shareware)
	    I_Error("\nYou cannot -file with the shareware "
		    "version. Register!");

	// Check for fake IWAD with right name,
	// but w/o all the lumps of the registered version. 
	if (gamemode == registered)
	    for (i = 0;i < 23; i++)
		if (W_CheckNumForName(name[i])<0)
		    I_Error("\nThis is not the registered version.");
    }
    // If additonal PWAD files are used, print modified banner
    if (modifiedgame)
    {
	/*m*/printf 
    (
	"===========================================================================\n"
	"     PS2DOOM is free software, covered by the DOOM Source Code\n               "
  "     License. However WITHOUT ANY WARRANTY even the implied warranty of      "
  "     FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License for more details.\n"
  "     You are welcome to change and distribute but you can´t sell comercially.\n"
  "     See the License for more information.\n                                    "                                                                 
	"=============================================================================\n");
	
    getchar ();
    }
	
    // Check and print which version is executed.
    switch ( gamemode )
    {
      case shareware:
      case indetermined:
	printf (
	    "===========================================================================\n"
	    "                                Open source!\n"
	    "===========================================================================\n"
    );
	break;
      case registered:
      case retail:
      case commercial:
	printf (
	    "===========================================================================\n"
	    "         Open source product - you can do distributions or modify the code!\n"
	    "         However you cannot comercialy sell the product                      "
        "         as in the terms of the DOOM Source Code License\n                   "
	    "===========================================================================\n");
	break;
	
      default:
	// Ouch.
	break;
    }
    
    //this useless for the newgui
    printf ("M_Init: Init miscellaneous info.\n");
    M_Init ();

    printf ("R_Init: Init DOOM refresh daemon - ");
    R_Init ();

    printf ("\nP_Init: Init Playloop state.\n");
    P_Init ();

    printf ("I_Init: Setting up machine state.\n");
    I_Init ();

    // cosmito : SDL_Init is inited here. scr_printf calls results in strange characters then. 

    printf ("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame ();

    printf ("S_Init: Setting up sound.\n");
    S_Init (snd_SfxVolume /* *8 */, snd_MusicVolume /* *8*/ );

    printf ("HU_Init: Setting up heads up display.\n");
    HU_Init ();

    printf ("ST_Init: Init status bar.\n");
    ST_Init ();
    
    

    if (statcopy && statcopy<myargc-1)
    {
	// for statistics driver
	extern  void*	statcopy;                            

	statcopy = (void*)atoi(myargv[p+1]);
	printf ("External statistics registered.\n");
    }
    

    if (record && record < myargc-1)
    {
	  G_RecordDemo (myargv[p+1]);
	  autostart = true;
    }
	
    
    if (playdemo && playdemo < myargc-1)
    {
 	    singledemo = true;              // quit after one demo
    	G_DeferedPlayDemo (myargv[p+1]);
	    D_DoomLoop ();  // never returns
    }
	
    
    if (timedemo && timedemo < myargc-1)
    {
	 G_TimeDemo (myargv[p+1]);
	 D_DoomLoop ();  // never returns
    }
	
    

    if (loadgame && loadgame < myargc-1)
    {
	if (cdrom)
	    //sprintf(file, "c:\\doomdata\\"SAVEGAMENAME"%c.dsg",myargv[p+1][0]);
        sprintf(file, "c:\\doomdata\\%s%c.dsg", currentWadName, myargv[p+1][0]);
	else
        //sprintf(file, "mc0:PS2DOOM/"SAVEGAMENAME"%c.dsg",myargv[p+1][0]);
        sprintf(file, "mc0:PS2DOOM/%s%c.dsg", currentWadName, myargv[p+1][0]);
	G_LoadGame (file);
    }
	

    if ( gameaction != ga_loadgame )
    {
	if (autostart || netgame)
	    G_InitNew (startskill, startepisode, startmap);
	else
	    D_StartTitle ();                // start up intro loop

    }

    D_DoomLoop ();  // never returns
}
