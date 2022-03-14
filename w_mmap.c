/*****************************************************
****************************************************** 
** TBD:                                             **       
** PT-BR: Verrificar se é preciso mais alguma coisa **
** EN-US: See if needs more something.              **
******************************************************
******************************************************/
#include "include/z_zone.h"
#include "include/i_system.h"
#include "include/w_wad.h"
#include <string.h>

static struct
{
  void *cache;
#ifdef TIMEDIAG
  int locktic;
#endif
  int locks;
} *cachelump;


/*******************************************************************************
******************************************************************************** 
** const void* W_LockLumpNum(int lump);                                       **
**                                                                            **
** Description:                                                               **
** This copies the lump into a malloced memory region and returns its address **
** instead of returning a pointer into the memory mapped area                 **
********************************************************************************
********************************************************************************/

const void* W_LockLumpNum(int lump)
{
  size_t len = W_LumpLength(lump);
  int tag;
  //printf(" (len = W_LumpLength(lump)) : lump = %d, len = %d\n", lump, len);    // cosmito : debug
  const void *data = W_CacheLumpNum(lump, tag);
  //printf("data : %p\n\n", data);    // cosmito : debug
  
/*********************************************************************
********************************************************************** 
** EN-US: the cache it´s not gonna be allocated. in prboom do this: **
** PT-BR: o cache não está a ser alocado. no prboom se faz assim:   **
**   D_DoomMainSetup/W_Init/W_InitCache                             **
**                                                                  **
**   void W_InitCache(void)                                         **
**   {                                                              **
**     // set up caching                                            **
**     cachelump = calloc(numlumps, sizeof *cachelump);             **
**     if (!cachelump)                                              **
**     I_Error ("W_Init: Couldn't allocate lumpcache");             **
**     etc                                                          **
**   }                                                              **
**********************************************************************
**********************************************************************/

  if (!cachelump[lump].cache) 
  { 
    // read the lump in
    Z_Malloc(len, PU_CACHE, &cachelump[lump].cache);
    memcpy(cachelump[lump].cache, data, len);
  }

  /* cph - if wasn't locked but now is, tell z_zone to hold it */
  if (cachelump[lump].locks <= 0) {
    Z_ChangeTag(cachelump[lump].cache,PU_STATIC);
#ifdef TIMEDIAG
    cachelump[lump].locktic = gametic;
#endif
    // reset lock counter
    cachelump[lump].locks = 1;
  } else {
    // increment lock counter
    cachelump[lump].locks += 1;
  }

#ifdef SIMPLECHECKS
  if (!((cachelump[lump].locks+1) & 0xf))
    lprintf(LO_DEBUG, "W_CacheLumpNum: High lock on %8s (%d)\n",
      lumpinfo[lump].name, cachelump[lump].locks);
#endif

  return cachelump[lump].cache;
}

void W_UnlockLumpNum(int lump)
{
	if (cachelump[lump].locks == -1)
		return; // this lump is memory mapped

#ifdef SIMPLECHECKS
	if (cachelump[lump].locks == 0)
		fprintf(stderr, "W_UnlockLumpNum: Excess unlocks on %8s\n",
		lumpinfo[lump].name);
#endif
	cachelump[lump].locks -= 1;
	/* cph - Note: must only tell z_zone to make purgeable if currently locked,
	* else it might already have been purged
	*/
	if (cachelump[lump].locks == 0)
		Z_ChangeTag(cachelump[lump].cache, PU_CACHE);
}
