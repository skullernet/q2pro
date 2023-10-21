/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.h"
#include "common/gamedll.h"
#include "system/system.h"

extern cvar_t   *fs_game;

static void *LoadGameLibraryFrom(const char *path)
{
    void *gamelib;

    Sys_LoadLibrary(path, NULL, &gamelib);
    if (!gamelib)
        Com_EPrintf("Failed to load game library: %s\n", Com_GetLastError());
    else
        Com_Printf("Loaded game library from %s\n", path);

    return gamelib;
}

static void *LoadGameLibrary(const char *libdir, const char *gamedir)
{
    char path[MAX_OSPATH];

    if (Q_concat(path, sizeof(path), libdir,
                 PATH_SEP_STRING, gamedir, PATH_SEP_STRING,
                 "game" CPUSTRING LIBSUFFIX) >= sizeof(path)) {
        Com_EPrintf("Game library path length exceeded\n");
        return NULL;
    }

    if (os_access(path, X_OK)) {
        Com_Printf("Can't access %s: %s\n", path, strerror(errno));
        return NULL;
    }

    return LoadGameLibraryFrom(path);
}


void *GameDll_Load(void)
{
    void *gamelib = NULL;

    // for debugging or `proxy' mods
    if (sys_forcegamelib->string[0])
        gamelib = LoadGameLibraryFrom(sys_forcegamelib->string);

    // try game first
    if (!gamelib && fs_game->string[0]) {
        if (sys_homedir->string[0])
            gamelib = LoadGameLibrary(sys_homedir->string, fs_game->string);
        if (!gamelib)
            gamelib = LoadGameLibrary(sys_libdir->string, fs_game->string);
    }

    // then try baseq2
    if (!gamelib) {
        if (sys_homedir->string[0])
            gamelib = LoadGameLibrary(sys_homedir->string, BASEGAME);
        if (!gamelib)
            gamelib = LoadGameLibrary(sys_libdir->string, BASEGAME);
    }

    return gamelib;
}
