#include <tamtypes.h>
#include <stdio.h>
#include <string.h>
#include "d_main.h"

int gethostname(char *name, int len)
{
	return -1;
}

u32 inet_addr(const char *cp)
{
	return 0;
}

/*float pow(float a, float b)
{
	return powf(a,b);
}*/

//**********************************************************************
// ps2doom cheat support - code typing
//**********************************************************************

void psp_do_cheat(int cheat)
{
    event_t event;
    char *str;
    int i;

	switch (cheat)
	{
		case 1: // God Mode
		str = "iddqd";
		break;
		case 2: // Fucking Arsenal
		str = "idfa";
		break;
		case 3: // Key Full Ammo
		str = "idkfa";
		break;
		case 4: // No Clipping
		str = "idclip";
		break;
		case 5: // Toggle Map
		str = "iddt";
		break;
		case 6: // Invincible with Chainsaw
		str = "idchoppers";
		break;
		case 7: // Berserker Strength Power-up
		str = "idbeholds";
		break;
		case 8: // Invincibility Power-up
		str = "idbeholdv";
		break;
		case 9: // Invisibility Power-Up
		str = "idbeholdi";
		break;
		case 10: // Automap Power-up
		str = "idbeholda";
		break;
		case 11: // Anti-Radiation Suit Power-up
		str = "idbeholdr";
		break;
		case 12: // Light-Amplification Visor Power-up
		str = "idbeholdl";
		break;
		default:
		return;
	}

	for (i=0; i<strlen(str); i++)
	{
        event.type = ev_keydown;
        event.data1 = str[i];
        D_PostEvent (&event);
        event.type = ev_keyup;
        event.data1 = str[i];
        D_PostEvent (&event);
	}
}
