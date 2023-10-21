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
#include "common/loc.h"

static struct {
    qhandle_t font_pic;
} scr;

static cvar_t   *scr_alpha;
static cvar_t   *scr_font;

static void scr_font_changed(cvar_t *self)
{
    scr.font_pic = R_RegisterFont(self->string);
}

static bool CGX_IsExtendedServer(void)
{
    return cl.csr.extended;
}

static void CGX_ClearColor(void)
{
    color_t clear_color;
    clear_color.u8[0] = clear_color.u8[1] = clear_color.u8[2] = 255;
    clear_color.u8[3] = 255 * Cvar_ClampValue(scr_alpha, 0, 1);
    R_SetColor(clear_color.u32);
}

static void CGX_SetAlpha(float alpha)
{
    R_SetAlpha(alpha * Cvar_ClampValue(scr_alpha, 0, 1));
}

static void CGX_SetColor(uint32_t color)
{
    color_t new_color;
    new_color.u32 = color;
    new_color.u8[3] *= Cvar_ClampValue(scr_alpha, 0, 1);
    R_SetColor(new_color.u32);
}

static cgame_q2pro_extended_support_ext_t cgame_q2pro_extended_support = {
    .api_version = 1,

    .IsExtendedServer = CGX_IsExtendedServer,
    .ClearColor = CGX_ClearColor,
    .SetAlpha = CGX_SetAlpha,
    .SetColor = CGX_SetColor,
};

void CG_Init(void)
{
    scr_alpha = Cvar_Get("scr_alpha", "1", 0);
    scr_font = Cvar_Get("scr_font", "conchars", 0);
    scr_font->changed = scr_font_changed;
    scr_font_changed(scr_font);
}

static void CG_Print(const char *msg)
{
    Com_Printf("%s", msg);
}

static const char *CG_get_configstring(int index)
{
    if (index < 0 || index >= cs_remap_rerelease.end)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);

    return cl.configstrings[remap_cs_index(index, &cs_remap_rerelease, &cl.csr)];
}

static cvar_t *CG_cvar(const char *var_name, const char *value, cvar_flags_t flags)
{
    if (flags & CVAR_EXTENDED_MASK) {
        Com_WPrintf("CGame attemped to set extended flags on '%s', masked out.\n", var_name);
        flags &= ~CVAR_EXTENDED_MASK;
    }

    return Cvar_Get(var_name, value, flags | CVAR_GAME);
}

static void * CG_GetExtension(const char *name)
{
    if (strcmp(name, cgame_q2pro_extended_support_ext) == 0) {
        return &cgame_q2pro_extended_support;
    }
    return NULL;
}

static uint64_t CG_CL_ClientTime(void)
{
    return cl.time;
}

static void CG_Draw_GetPicSize (int *w, int *h, const char *name)
{
    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (img == 0) {
        *w = *h = 0;
        return;
    }
    R_GetPicSize(w, h, img);
}

static void CG_SCR_DrawChar(int x, int y, int scale, int num, bool shadow)
{
    int draw_flags = shadow ? UI_DROPSHADOW : 0;
    R_DrawChar(x, y, draw_flags, num, scr.font_pic);
}

static void CG_SCR_DrawPic (int x, int y, int w, int h, const char *name)
{
    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (img == 0)
        return;

    R_DrawStretchPic(x, y, w, h, img);
}

#define NUM_LOC_STRINGS     8
#define LOC_STRING_LENGTH   MAX_STRING_CHARS
static char cg_loc_strings[NUM_LOC_STRINGS][LOC_STRING_LENGTH];
static int cg_loc_string_num = 0;

static const char* CG_Localize (const char *base, const char **args, size_t num_args)
{
    char *out_str = cg_loc_strings[cg_loc_string_num];
    cg_loc_string_num = (cg_loc_string_num + 1) % NUM_LOC_STRINGS;
    Loc_Localize(base, true, args, num_args, out_str, LOC_STRING_LENGTH);
    return out_str;
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

            .get_configstring = CG_get_configstring,

            .cvar = CG_cvar,

            .GetExtension = CG_GetExtension,

            .CL_ClientTime = CG_CL_ClientTime,
            .Draw_GetPicSize = CG_Draw_GetPicSize,
            .SCR_DrawChar = CG_SCR_DrawChar,
            .SCR_DrawPic = CG_SCR_DrawPic,

            .Localize = CG_Localize,
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
