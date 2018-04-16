/*
* Copyright (C) 2015-2018 Frank Sapone
* Copyright (C) 2002-2003 r1ch.net
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include "dk_essentials.h"

typedef struct
{
	const char *gamename;
	const char *seckey;
	unsigned short	motdPort;
} game_table_t;

game_table_t gameTable[] =
{
	{"blood2", "jUOF0p", 0},
	{"daikatana", "fl8aY7", 27991},
	{"gspylite", "mgNUaC", 0},
	{"heretic2", "2iuCAS", 0},
	{"hexenworld", "6SeXQB", 0},
	{"kingpin", "QFWxY2", 0},
	{"nolf", "Jn3Ab4", 0},
	{"quake1", "7W7yZz", 0},
	{"quake2", "rtW0xg", 27901},
	{"quakeworld", "FU6Vqn", 27501},
	{"shogo", "MQMhRK", 0}, /* FS: Untested, but assumed to work */
	{"sin", "Ij1uAB", 0},
	{"southpark", "yoI7mE", 0},
	{"turok2", "RWd3BG", 0},
	{"unreal", "DAncRK", 0},
	{"ut", "Z5Nfb0", 0}, /* FS: Unreal Tournament 99 */
	{NULL, NULL}
};

const char *Gamespy_Get_Game_SecKey (char *gamename)
{
	int x = 0;

	if (!gamename || gamename[0] == 0)
	{
		return NULL;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly sin) stupidly send it partially uppercase */

	while (gameTable[x].gamename != NULL)
	{
		if(!strcmp(gamename, gameTable[x].gamename))
		{
			return gameTable[x].seckey;
		}

		x++;
	}
	return NULL;
}

unsigned short Gamespy_Get_MOTD_Port (char *gamename)
{
	int x = 0;

	if (!gamename || gamename[0] == 0)
	{
		return 0;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly sin) stupidly send it partially uppercase */

	while (gameTable[x].gamename != NULL)
	{
		if(!strcmp(gamename, gameTable[x].gamename))
		{
			return gameTable[x].motdPort;
		}

		x++;
	}
	return 0;
}
