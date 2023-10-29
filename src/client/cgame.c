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
#include "common/gamedll.h"

static struct {
    qhandle_t font_pic;
    
    kfont_t kfont;
} scr;

static cvar_t   *scr_alpha;
static cvar_t   *scr_font;

static void scr_font_changed(cvar_t *self)
{
    scr.font_pic = R_RegisterFont(self->string);
}

static inline uint32_t color_to_u32(const rgba_t* color)
{
    return MakeColor(color->r, color->g, color->b, color->a);
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

static const pmoveParams_t* CGX_GetPmoveParams(void)
{
    return &cl.pmp;
}

static cgame_q2pro_extended_support_ext_t cgame_q2pro_extended_support = {
    .api_version = 1,

    .IsExtendedServer = CGX_IsExtendedServer,
    .ClearColor = CGX_ClearColor,
    .SetAlpha = CGX_SetAlpha,
    .SetColor = CGX_SetColor,
    .GetPmoveParams = CGX_GetPmoveParams,
};

void CG_Init(void)
{
    scr_alpha = Cvar_Get("scr_alpha", "1", 0);
    scr_font = Cvar_Get("scr_font", "conchars", 0);
    scr_font->changed = scr_font_changed;
    scr_font_changed(scr_font);

    SCR_LoadKFont(&scr.kfont, "fonts/qconfont.kfont");
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

static void CG_Com_Error(const char *message)
{
    Com_Error(ERR_DROP, "%s", message);
}

static void *CG_TagMalloc(size_t size, int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    return Z_TagMallocz(size, tag + TAG_MAX);
}

static void CG_FreeTags(int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    Z_FreeTags(tag + TAG_MAX);
}

static cvar_t *CG_cvar(const char *var_name, const char *value, cvar_flags_t flags)
{
    if (flags & CVAR_EXTENDED_MASK) {
        Com_WPrintf("CGame attemped to set extended flags on '%s', masked out.\n", var_name);
        flags &= ~CVAR_EXTENDED_MASK;
    }

    return Cvar_Get(var_name, value, flags | CVAR_GAME);
}

static void CG_AddCommandString(const char *string)
{
    if (!strcmp(string, "menu_loadgame\n"))
        string = "pushmenu loadgame\n";
    Cbuf_AddText(&cmd_buffer, string);
}

static void * CG_GetExtension(const char *name)
{
    if (strcmp(name, cgame_q2pro_extended_support_ext) == 0) {
        return &cgame_q2pro_extended_support;
    }
    return NULL;
}

static bool CG_CL_FrameValid(void)
{
    return cl.frame.valid;
}

static float CG_CL_FrameTime(void)
{
    return cls.frametime;
}

static uint64_t CG_CL_ClientTime(void)
{
    return cl.time;
}

static uint64_t CG_CL_ClientRealTime(void)
{
    return cls.realtime;
}

static int32_t CG_CL_ServerFrame(void)
{
    return cl.frame.number;
}

static int32_t CG_CL_ServerProtocol(void)
{
    return cls.serverProtocol;
}

static const char* CG_CL_GetClientName(int32_t index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        Com_Error(ERR_DROP, "%s: invalid client index", __func__);
    }
    return cl.clientinfo[index].name;
}

static const char* CG_CL_GetClientPic(int32_t index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        Com_Error(ERR_DROP, "%s: invalid client index", __func__);
    }
    return cl.clientinfo[index].icon_name;
}

static const char * CG_CL_GetClientDogtag (int32_t index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        Com_Error(ERR_DROP, "%s: invalid client index", __func__);
    }
    return cl.clientinfo[index].dogtag_name;
}

static const char* CG_CL_GetKeyBinding(const char *binding)
{
    return Key_GetBinding(binding);
}

static bool CG_Draw_RegisterPic(const char *name)
{
    qhandle_t img = R_RegisterPic(name);
    return img != 0;
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

static void CG_SCR_DrawColorPic(int x, int y, int w, int h, const char* name, const rgba_t *color)
{
    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (img == 0)
        return;

    CGX_SetColor(color_to_u32(color));
    R_DrawStretchPic(x, y, w, h, img);
    CGX_ClearColor();
}

static void CG_SCR_SetAltTypeface(bool enabled)
{
    // We don't support alternate type faces.
}

static float CG_SCR_FontLineHeight(int scale)
{
    if (!scr.kfont.pic)
        return CHAR_HEIGHT * scale;

    return scr.kfont.line_height;
}

static cg_vec2_t CG_SCR_MeasureFontString(const char *str, int scale)
{
    // TODO: 'str' may contain UTF-8, handle that.
    // FIXME: can contain line breaks
    int x, y = CG_SCR_FontLineHeight(scale);

    if (!scr.kfont.pic)
        x = strlen(str) * CHAR_WIDTH * scale;
    else {
        x = 0;

        for (const char *r = str; *r; r++) {
            const kfont_char_t *ch = SCR_KFontLookup(&scr.kfont, *r);

            if (ch)
                x += ch->w;
        }
    }

    return (cg_vec2_t) { x, y };
}

static void CG_SCR_DrawFontString(const char *str, int x, int y, int scale, const rgba_t *color, bool shadow, text_align_t align)
{
    CGX_SetColor(color_to_u32(color));

    int draw_x = x;
    if (align != LEFT) {
        int text_width = CG_SCR_MeasureFontString(str, scale).x;
        if (align == CENTER)
            draw_x -= text_width / 2;
        else if (align == RIGHT)
            draw_x -= text_width;
    }

    int draw_flags = shadow ? UI_DROPSHADOW : 0;

    // TODO: 'str' may contain UTF-8, handle that.
    // FIXME: can contain line breaks
    if (!scr.kfont.pic) {
        while (*str) {
            R_DrawStretchChar(draw_x, y, CHAR_WIDTH * scale, CHAR_HEIGHT * scale, draw_flags, *str++, scr.font_pic);
            draw_x += CHAR_WIDTH * scale;
        }
    } else {
        while (*str) {
            draw_x += R_DrawKFontChar(draw_x, y, scale, draw_flags, *str++, &scr.kfont);
        }
    }

    CGX_ClearColor();
}

static bool CG_CL_GetTextInput(const char **msg, bool *is_team)
{
    // FIXME: Hook up with chat prompt
    return false;
}

static int32_t CG_CL_GetWarnAmmoCount(int32_t weapon_id)
{
    // FIXME: Where to get this?
    return 0;
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

static const rgba_t rgba_white = {255, 255, 255, 255};

static int32_t CG_SCR_DrawBind(int32_t isplit, const char *binding, const char *purpose, int x, int y, int scale)
{
    /* - 'binding' is the name of the action/command (eg "+moveup") whose binding should be displayed
     * - 'purpose' is a string describing what that action/key does. Needs localization.
     * - Rerelease has some fancy graphics for keys and such. We ... don't ¯\_(ツ)_/¯
     */
    const char *key = Key_GetBinding(binding);

    char str[MAX_STRING_CHARS];
    if (!*key)
        Q_snprintf(str, sizeof(str), "<unbound> %s", CG_Localize(purpose, NULL, 0));
    else
        Q_snprintf(str, sizeof(str), "[%s] %s", key, CG_Localize(purpose, NULL, 0));
    CG_SCR_DrawFontString(str, x, y, scale, &rgba_white, false, CENTER);
    return CHAR_HEIGHT;
}

static bool CG_CL_InAutoDemoLoop(void)
{
    // FIXME: implement
    return false;
}

const cgame_export_t *cgame = NULL;
static char *current_game = NULL;
static int current_protocol;
static void *cgame_library;

typedef cgame_export_t *(*cgame_entry_t)(cgame_import_t *);

void CG_Load(const char* new_game)
{
    if (!current_game || strcmp(current_game, new_game) != 0 || current_protocol != cls.serverProtocol) {
        cgame_import_t cgame_imports = {
            .tick_rate = 1000 / CL_FRAMETIME,
            .frame_time_s = CL_FRAMETIME * 0.001f,
            .frame_time_ms = CL_FRAMETIME,

            .Com_Print = CG_Print,

            .get_configstring = CG_get_configstring,

            .Com_Error = CG_Com_Error,

            .TagMalloc = CG_TagMalloc,
            .TagFree = Z_Free,
            .FreeTags = CG_FreeTags,

            .cvar = CG_cvar,
            .cvar_set = Cvar_UserSet,
            .cvar_forceset = Cvar_Set,

            .AddCommandString = CG_AddCommandString,

            .GetExtension = CG_GetExtension,

            .CL_FrameValid = CG_CL_FrameValid,

            .CL_FrameTime = CG_CL_FrameTime,

            .CL_ClientTime = CG_CL_ClientTime,
            .CL_ClientRealTime = CG_CL_ClientRealTime,
            .CL_ServerFrame = CG_CL_ServerFrame,
            .CL_ServerProtocol = CG_CL_ServerProtocol,
            .CL_GetClientName = CG_CL_GetClientName,
            .CL_GetClientPic = CG_CL_GetClientPic,
            .CL_GetClientDogtag = CG_CL_GetClientDogtag,
            .CL_GetKeyBinding = CG_CL_GetKeyBinding,
            .Draw_RegisterPic = CG_Draw_RegisterPic,
            .Draw_GetPicSize = CG_Draw_GetPicSize,
            .SCR_DrawChar = CG_SCR_DrawChar,
            .SCR_DrawPic = CG_SCR_DrawPic,
            .SCR_DrawColorPic = CG_SCR_DrawColorPic,

            .SCR_SetAltTypeface = CG_SCR_SetAltTypeface,
            .SCR_DrawFontString = CG_SCR_DrawFontString,
            .SCR_MeasureFontString = CG_SCR_MeasureFontString,
            .SCR_FontLineHeight = CG_SCR_FontLineHeight,

            .CL_GetTextInput = CG_CL_GetTextInput,

            .CL_GetWarnAmmoCount = CG_CL_GetWarnAmmoCount,

            .Localize = CG_Localize,

            .SCR_DrawBind = CG_SCR_DrawBind,

            .CL_InAutoDemoLoop = CG_CL_InAutoDemoLoop,
        };

        cgame_entry_t entry = NULL;
        if (cls.serverProtocol == PROTOCOL_VERSION_RERELEASE) {
            if (cgame_library)
                Sys_FreeLibrary(cgame_library);
            cgame_library = GameDll_Load();
            if (cgame_library)
                entry = Sys_GetProcAddress(cgame_library, "GetCGameAPI");
        } else {
            // if we're connected to a "classic" Q2PRO server, always use builtin cgame
            entry = GetClassicCGameAPI;
        }

        if(!entry) {
            // FIXME: Should probably be a fatal error
            Com_Printf("cgame functions not available, falling back to built-in cgame\n");
            entry = GetClassicCGameAPI;
        }

        cgame = entry(&cgame_imports);
        current_game = Z_CopyString(new_game);
        current_protocol = cls.serverProtocol;
    }
}

void CG_Unload(void)
{
    cgame = NULL;
    Z_Freep(&current_game);

    Sys_FreeLibrary(cgame_library);
    cgame_library = NULL;
}
