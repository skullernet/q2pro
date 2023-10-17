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

#include "client.h"
#include "cgame_classic.h"

void CG_Init(void) {}

static void CG_Print(const char *msg)
{
    Com_Printf("%s", msg);
}

const cgame_export_t *cgame = NULL;
static char *current_game = NULL;

void CG_Load(const char* new_game)
{
    if (!current_game || strcmp(current_game, new_game) != 0) {
        cgame_import_t cgame_imports = {
            .tick_rate = 1000 / CL_FRAMETIME,
            .frame_time_s = CL_FRAMETIME * 0.001f,
            .frame_time_ms = CL_FRAMETIME,

            .Com_Print = CG_Print,
        };

        cgame = GetClassicCGameAPI(&cgame_imports);
        current_game = Z_CopyString(new_game);
    }
}

void CG_Unload(void)
{
    cgame = NULL;
    Z_Freep(&current_game);
}
