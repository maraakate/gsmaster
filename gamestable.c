/*
Copyright (c) 2015-2018 Frank Sapone <fhsapone@gmail.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "dk_essentials.h"
#include "gamestable.h"

game_table_t gameTable[MAX_SUPPORTED_GAMETYPES] =
{
	{"blood2", "jUOF0p", 0},
	{"daikatana", "fl8aY7", 27991},
	{"gamespy2", "d4kZca", 0},
	{"gspylite", "mgNUaC", 0},
	{"heretic2", "2iuCAS", 0},
	{"hexen2", "FAKEKEY", 0},
	{"hexenworld", "6SeXQB", 0},
	{"kingpin", "QFWxY2", 0},
	{"nolf", "Jn3Ab4", 0},
	{"nolf2", "g3Fo6x", 0},
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

const char *GameSpy_Get_Game_SecKey (char *gamename)
{
	int x = 0;

	if (!gamename || gamename[0] == 0)
	{
		return NULL;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly sin) send it partially uppercase */

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

unsigned short GameSpy_Get_MOTD_Port (char *gamename)
{
	int x = 0;

	if (!gamename || gamename[0] == 0)
	{
		return 0;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly sin) send it partially uppercase */

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
