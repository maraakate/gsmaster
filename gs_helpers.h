#ifndef _GS_HELPERS_H
#define _GS_HELPERS_H

#include "shared.h"

#define MAX_SUPPORTED_GAMETYPES 20

typedef struct game_table_s
{
	const char *gamename;
	const char *seckey;
	unsigned short	motdPort;
} game_table_t;

extern const game_table_t gameTable[MAX_SUPPORTED_GAMETYPES];

void GameSpy_Create_Challenge_Key (char *s, const size_t len);
const char *GameSpy_Get_Game_SecKey (const char *gamename);
unsigned short GameSpy_Get_MOTD_Port (const char *gamename);
unsigned short GameSpy_Get_Table_Number (const char *gamename);
unsigned short GameSpy_Get_GS3D_Port_Offset (const char *gamename, unsigned short port);

#endif // _GS_HELPERS_H
