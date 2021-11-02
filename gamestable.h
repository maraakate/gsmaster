#ifndef _GAMESTABLE_H
#define _GAMESTABLE_H

#include "shared.h"

#define MAX_SUPPORTED_GAMETYPES 20

typedef struct game_table_s
{
	const char *gamename;
	const char *seckey;
	unsigned short	motdPort;
} game_table_t;

extern game_table_t gameTable[MAX_SUPPORTED_GAMETYPES];

unsigned short GameSpy_Get_Table_Number (const char *gamename);
unsigned short GameSpy_Get_GS3D_Port_Offset (const char *gamename, unsigned short port);

#endif // _GAMESTABLE_H
