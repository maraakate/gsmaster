#ifndef _GAMESTABLE_H
#define _GAMESTABLE_H

#include "shared.h"

#define MAX_SUPPORTED_GAMETYPES 19

typedef struct game_table_s
{
	const char *gamename;
	const char *seckey;
	unsigned short	motdPort;
} game_table_t;

extern game_table_t gameTable[MAX_SUPPORTED_GAMETYPES];

#endif // _GAMESTABLE_H
