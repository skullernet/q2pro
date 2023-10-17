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

/* client.h is deliberately not included to make sure only functions from
 * the cgame_import struct are used */
#include "shared/shared.h"
#include "cgame_classic.h"

static cgame_import_t cgi;

static void CGC_Init(void) {}

static void CGC_Shutdown(void) {}

static void CGC_DrawHUD (int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps)
{
    // Note: isplit is ignored, due to missing split screen support
}

cgame_export_t cgame_classic = {
    .apiversion = CGAME_API_VERSION,

    .Init = CGC_Init,
    .Shutdown = CGC_Shutdown,

    .DrawHUD = CGC_DrawHUD,
};

cgame_export_t *GetClassicCGameAPI(cgame_import_t *import)
{
    cgi = *import;
    return &cgame_classic;
}
