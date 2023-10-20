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
#include "cgame_classic.h"

#include "common/cvar.h"
#include "common/utils.h"

#include <malloc.h>

/* Some definitions copied from client.h
 * This file is deliberately not included to make sure only functions from
 * the cgame_import struct are used */
#define CHAR_WIDTH  8
#define CHAR_HEIGHT 8

#define UI_LEFT             BIT(0)
#define UI_RIGHT            BIT(1)
#define UI_CENTER           (UI_LEFT | UI_RIGHT)
#define UI_DROPSHADOW       BIT(4)
#define UI_XORCOLOR         BIT(7)

float SCR_FadeAlpha(unsigned startTime, unsigned visTime, unsigned fadeTime);
bool SCR_ParseColor(const char *s, color_t *color);

// ==========================================================================

static cgame_import_t cgi;
static cgame_q2pro_extended_support_ext_t cgix;

static cvar_t   *scr_centertime;
static cvar_t   *scr_draw2d;

static cvar_t   *ch_scale;
static cvar_t   *ch_x;
static cvar_t   *ch_y;

static char     scr_centerstring[MAX_STRING_CHARS];
static unsigned scr_centertime_start;   // for slow victory printing
static int      scr_center_lines;

static void CGC_Init(void)
{
    scr_centertime = cgi.cvar("scr_centertime", "2.5", 0);
    scr_draw2d = cgi.cvar("scr_draw2d", "2", 0);

    ch_scale = cgi.cvar("ch_scale", "1", 0);
    ch_x = cgi.cvar("ch_x", "0", 0);
    ch_y = cgi.cvar("ch_y", "0", 0);
}

static void CGC_Shutdown(void)
{
    scr_draw2d = NULL;
    ch_scale = NULL;
    ch_x = NULL;
    ch_y = NULL;
}

static void DrawPic(int x, int y, const char* pic)
{
    int w = 0, h = 0;
    cgi.Draw_GetPicSize(&w, &h, pic);
    if (!w || !h)
        return;
    cgi.SCR_DrawPic(x, y, w, h, pic);
}

static void CG_DrawString(int x, int y, int flags, size_t maxlen, const char *s)
{
    const char *write_str = s;
    if (flags & UI_XORCOLOR) {
        size_t new_str_len = min(strlen(s), maxlen);
        char *new_str = alloca(new_str_len + 1);
        for (size_t i = 0; i < new_str_len; i++) {
            new_str[i] = s[i] ^ 0x80;
        }
        new_str[new_str_len] = 0;
        write_str = new_str;
    }

    while (maxlen-- && *write_str) {
        byte c = *write_str++;
        cgi.SCR_DrawChar(x, y, 1, c, flags & UI_DROPSHADOW);
        x += CHAR_WIDTH;
    }
}

/*
==============
DrawStringEx
==============
*/
static void CG_DrawStringEx(int x, int y, int flags, size_t maxlen, const char *s)
{
    size_t len = strlen(s);

    if (len > maxlen) {
        len = maxlen;
    }

    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= len * CHAR_WIDTH / 2;
    } else if (flags & UI_RIGHT) {
        x -= len * CHAR_WIDTH;
    }

    CG_DrawString(x, y, flags, maxlen, s);
}

/*
==============
DrawStringMulti
==============
*/
static void CG_DrawStringMulti(int x, int y, int flags, size_t maxlen, const char *s)
{
    char    *p;
    size_t  len;

    while (*s) {
        p = strchr(s, '\n');
        if (!p) {
            CG_DrawStringEx(x, y, flags, maxlen, s);
            break;
        }

        len = p - s;
        if (len > maxlen) {
            len = maxlen;
        }
        CG_DrawStringEx(x, y, flags, len, s);

        y += CHAR_HEIGHT;
        s = p + 1;
    }
}

#define HUD_DrawString(x, y, string) \
    CG_DrawString(x, y, 0, MAX_STRING_CHARS, string)

#define HUD_DrawAltString(x, y, string) \
    CG_DrawString(x, y, UI_XORCOLOR, MAX_STRING_CHARS, string)

#define HUD_DrawCenterString(x, y, string) \
    CG_DrawStringMulti(x, y, UI_CENTER, MAX_STRING_CHARS, string)

#define HUD_DrawAltCenterString(x, y, string) \
    CG_DrawStringMulti(x, y, UI_CENTER | UI_XORCOLOR, MAX_STRING_CHARS, string)

#define HUD_DrawRightString(x, y, string) \
    CG_DrawStringEx(x, y, UI_RIGHT, MAX_STRING_CHARS, string)

#define HUD_DrawAltRightString(x, y, string) \
    CG_DrawStringEx(x, y, UI_RIGHT | UI_XORCOLOR, MAX_STRING_CHARS, string)

static const char field_pic[] = "field_3";
static const char inven_pic[] = "inventory";

#define DIGIT_WIDTH 16
#define STAT_PICS       11
#define STAT_MINUS      (STAT_PICS - 1)  // num frame for '-' stats digit

static const char *const sb_nums[2][STAT_PICS] = {
    {
        "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
        "num_6", "num_7", "num_8", "num_9", "num_minus"
    },
    {
        "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
        "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
    }
};

static void HUD_DrawNumber(int x, int y, int color, int width, int value)
{
    char    num[16], *ptr;
    int     l;
    int     frame;

    if (width < 1)
        return;

    // draw number string
    if (width > 5)
        width = 5;

    color &= 1;

    l = Q_scnprintf(num, sizeof(num), "%i", value);
    if (l > width)
        l = width;
    x += 2 + DIGIT_WIDTH * (width - l);

    ptr = num;
    while (*ptr && l) {
        if (*ptr == '-')
            frame = STAT_MINUS;
        else
            frame = *ptr - '0';

        DrawPic(x, y, sb_nums[color][frame]);
        x += DIGIT_WIDTH;
        ptr++;
        l--;
    }
}

static void SCR_SkipToEndif(const char **s)
{
    int i, skip = 1;
    char *token;

    while (*s) {
        token = COM_Parse(s);
        if (!strcmp(token, "xl") || !strcmp(token, "xr") || !strcmp(token, "xv") ||
            !strcmp(token, "yt") || !strcmp(token, "yb") || !strcmp(token, "yv") ||
            !strcmp(token, "pic") || !strcmp(token, "picn") || !strcmp(token, "color") ||
            strstr(token, "string")) {
            COM_Parse(s);
            continue;
        }

        if (!strcmp(token, "client")) {
            for (i = 0; i < 6; i++)
                COM_Parse(s);
            continue;
        }

        if (!strcmp(token, "ctf")) {
            for (i = 0; i < 5; i++)
                COM_Parse(s);
            continue;
        }

        if (!strcmp(token, "num")) {
            COM_Parse(s);
            COM_Parse(s);
            continue;
        }

        if (!strcmp(token, "hnum")) continue;
        if (!strcmp(token, "anum")) continue;
        if (!strcmp(token, "rnum")) continue;

        if (!strcmp(token, "if")) {
            COM_Parse(s);
            skip++;
            continue;
        }

        if (!strcmp(token, "endif")) {
            if (--skip > 0)
                continue;
            return;
        }
    }
}

static inline int flash_frame(void)
{
    /* Original logic:
     * ((cl.frame.number / CL_FRAMEDIV) >> 2) & 1
     * Which in vanilla works out to a flash lasting 4 frames, or 400 ms */
    return (cgi.CL_ClientTime() / 400) & 1;
}

static void layout_pic(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // draw a pic from a stat number
    char* token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_STATS) {
        cgi.Com_Error(va("%s: invalid stat index", __func__));
    }
    value = ps->stats[value];
    if (value < 0 || value >= cs_remap_rerelease.max_images) {
        cgi.Com_Error(va("%s: invalid pic index", __func__));
    }
    const char* pic = cgi.get_configstring(cs_remap_rerelease.images + value);
    if (pic[0]) {
        // hack for action mod scope scaling
        if (x == hud_vrect.width  / 2 - 160 &&
            y == hud_vrect.height / 2 - 120 &&
            Com_WildCmp("scope?x", pic))
        {
            int w = 320 * ch_scale->value;
            int h = 240 * ch_scale->value;
            cgi.SCR_DrawPic((hud_vrect.width  - w) / 2 + ch_x->integer,
                            (hud_vrect.height - h) / 2 + ch_y->integer,
                            w, h, pic);
        } else {
            DrawPic(x, y, pic);
        }
    }
}

static void layout_client(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // draw a deathmatch client block
    char    buffer[MAX_QPATH];
    int     score, ping, time;

    char* token = COM_Parse(s);
    x = hud_vrect.x + hud_vrect.width / 2 - 160 + atoi(token);
    token = COM_Parse(s);
    y = hud_vrect.y + hud_vrect.height / 2 - 120 + atoi(token);

    token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_CLIENTS) {
        cgi.Com_Error(va("%s: invalid client index", __func__));
    }

    token = COM_Parse(s);
    score = atoi(token);

    token = COM_Parse(s);
    ping = atoi(token);

    token = COM_Parse(s);
    time = atoi(token);

    HUD_DrawAltString(x + 32, y, cgi.CL_GetClientName(value));
    HUD_DrawString(x + 32, y + CHAR_HEIGHT, "Score: ");
    Q_snprintf(buffer, sizeof(buffer), "%i", score);
    HUD_DrawAltString(x + 32 + 7 * CHAR_WIDTH, y + CHAR_HEIGHT, buffer);
    Q_snprintf(buffer, sizeof(buffer), "Ping:  %i", ping);
    HUD_DrawString(x + 32, y + 2 * CHAR_HEIGHT, buffer);
    Q_snprintf(buffer, sizeof(buffer), "Time:  %i", time);
    HUD_DrawString(x + 32, y + 3 * CHAR_HEIGHT, buffer);

    DrawPic(x, y, cgi.CL_GetClientPic(value));
}

static void layout_ctf(vrect_t hud_vrect, const char **s, int32_t playernum, const player_state_t *ps, int x, int y)
{
    // draw a ctf client block
    char    buffer[MAX_QPATH];
    int     score, ping;

    char* token = COM_Parse(s);
    x = hud_vrect.x + hud_vrect.width / 2 - 160 + atoi(token);
    token = COM_Parse(s);
    y = hud_vrect.y + hud_vrect.height / 2 - 120 + atoi(token);

    token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_CLIENTS) {
        cgi.Com_Error(va("%s: invalid client index", __func__));
    }

    token = COM_Parse(s);
    score = atoi(token);

    token = COM_Parse(s);
    ping = atoi(token);
    if (ping > 999)
        ping = 999;

    Q_snprintf(buffer, sizeof(buffer), "%3d %3d %-12.12s",
                score, ping, cgi.CL_GetClientName(value));
    if (value == playernum) {
        HUD_DrawAltString(x, y, buffer);
    } else {
        HUD_DrawString(x, y, buffer);
    }
}

static void layout_num(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // draw a number
    char* token = COM_Parse(s);
    int width = atoi(token);
    token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_STATS) {
        cgi.Com_Error(va("%s: invalid stat index", __func__));
    }
    value = ps->stats[value];
    HUD_DrawNumber(x, y, 0, width, value);
}

static void layout_hnum(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // health number
    int     color;

    int width = 3;
    int value = ps->stats[STAT_HEALTH];
    if (value > 25)
        color = 0;  // green
    else if (value > 0)
        color = flash_frame();     // flash
    else
        color = 1;

    if (ps->stats[STAT_FLASHES] & 1)
        DrawPic(x, y, field_pic);

    HUD_DrawNumber(x, y, color, width, value);
}

static void layout_anum(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // ammo number
    int     color;

    int width = 3;
    int value = ps->stats[STAT_AMMO];
    if (value > 5)
        color = 0;  // green
    else if (value >= 0)
        color = flash_frame();     // flash
    else
        return;   // negative number = don't show

    if (ps->stats[STAT_FLASHES] & 4)
        DrawPic(x, y, field_pic);

    HUD_DrawNumber(x, y, color, width, value);
}

static void layout_rnum(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // armor number
    int     color;

    int width = 3;
    int value = ps->stats[STAT_ARMOR];
    if (value < 1)
        return;

    color = 0;  // green

    if (ps->stats[STAT_FLASHES] & 2)
        DrawPic(x, y, field_pic);

    HUD_DrawNumber(x, y, color, width, value);
}

static void layout_stat(const char* token, vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    bool localize = !strncmp(token, "loc_", 4);
    const char *cmd = token + 5 + (localize ? 4 : 0);
    token = COM_Parse(s);
    int index = atoi(token);
    if (index < 0 || index >= MAX_STATS) {
        cgi.Com_Error(va("%s: invalid stat index", __func__));
    }
    index = ps->stats[index];
    if (index < 0 || index >= cs_remap_rerelease.end) {
        cgi.Com_Error(va("%s: invalid string index", __func__));
    }
    const char* str = cgi.get_configstring(index);
    if (localize)
        str = cgi.Localize(str, NULL, 0);
    if (!strcmp(cmd, "string"))
        HUD_DrawString(x, y, str);
    else if (!strcmp(cmd, "string2"))
        HUD_DrawAltString(x, y, str);
    else if (!strcmp(cmd, "cstring"))
        HUD_DrawCenterString(x + 320 / 2, y, str);
    else if (!strcmp(cmd, "cstring2"))
        HUD_DrawAltCenterString(x + 320 / 2, y, str);
    else if (!strcmp(cmd, "rstring"))
        HUD_DrawRightString(x, y, str);
    else if (!strcmp(cmd, "rstring2"))
        HUD_DrawAltRightString(x, y, str);
}

static char arg_tokens[MAX_LOCALIZATION_ARGS + 1][MAX_TOKEN_CHARS];
static const char *arg_buffers[MAX_LOCALIZATION_ARGS];

static const char *parse_loc_string(const char** s)
{
    int num_args = atoi(COM_Parse (s));

    if (num_args < 0 || num_args >= MAX_LOCALIZATION_ARGS)
        cgi.Com_Error(va("%s: Bad loc string", __func__));

    // parse base
    char* token = COM_Parse (s);
    Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

    // parse args
    for (int i = 0; i < num_args; i++)
    {
        token = COM_Parse (s);
        Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
        arg_buffers[i] = arg_tokens[1 + i];
    }

    return cgi.Localize(arg_tokens[0], arg_buffers, num_args);
}

static void SCR_ExecuteLayoutString(vrect_t hud_vrect, const char *s, int32_t playernum, const player_state_t *ps)
{
    int     x, y;
    int     value;
    char    *token;

    if (!s[0])
        return;

    x = 0;
    y = 0;

    while (s) {
        token = COM_Parse(&s);
        if (token[2] == 0) {
            if (token[0] == 'x') {
                if (token[1] == 'l') {
                    token = COM_Parse(&s);
                    x = hud_vrect.x + atoi(token);
                    continue;
                }

                if (token[1] == 'r') {
                    token = COM_Parse(&s);
                    x = hud_vrect.x + hud_vrect.width + atoi(token);
                    continue;
                }

                if (token[1] == 'v') {
                    token = COM_Parse(&s);
                    x = hud_vrect.x + hud_vrect.width / 2 - 160 + atoi(token);
                    continue;
                }
            }

            if (token[0] == 'y') {
                if (token[1] == 't') {
                    token = COM_Parse(&s);
                    y = hud_vrect.y + atoi(token);
                    continue;
                }

                if (token[1] == 'b') {
                    token = COM_Parse(&s);
                    y = hud_vrect.y + hud_vrect.height + atoi(token);
                    continue;
                }

                if (token[1] == 'v') {
                    token = COM_Parse(&s);
                    y = hud_vrect.y + hud_vrect.height / 2 - 120 + atoi(token);
                    continue;
                }
            }
        }

        if (!strcmp(token, "pic")) {
            layout_pic(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "client")) {
            layout_client(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "ctf")) {
            layout_ctf(hud_vrect, &s, playernum, ps, x, y);
            continue;
        }

        if (!strcmp(token, "picn")) {
            // draw a pic from a name
            token = COM_Parse(&s);
            DrawPic(x, y, token);
            continue;
        }

        if (!strcmp(token, "num")) {
            layout_num(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "hnum")) {
            layout_hnum(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "anum")) {
            layout_anum(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "rnum")) {
            layout_rnum(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strncmp(token, "stat_", 5) || !strncmp(token, "loc_stat_", 9)) {
            layout_stat(token, hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "cstring") || !strcmp(token, "loc_cstring")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawCenterString(x + 320 / 2, y, str);
            continue;
        }

        if (!strcmp(token, "cstring2") || !strcmp(token, "loc_cstring2")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawAltCenterString(x + 320 / 2, y, str);
            continue;
        }

        if (!strcmp(token, "string") || !strcmp(token, "loc_string")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawString(x, y, str);
            continue;
        }

        if (!strcmp(token, "string2") || !strcmp(token, "string2")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawAltString(x, y, str);
            continue;
        }

        if (!strcmp(token, "rstring") || !strcmp(token, "loc_rstring")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawRightString(x, y, str);
            continue;
        }

        if (!strcmp(token, "rstring2") || !strcmp(token, "loc_rstring2")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawAltRightString(x, y, str);
            continue;
        }

        if (!strcmp(token, "if")) {
            token = COM_Parse(&s);
            value = atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                cgi.Com_Error(va("%s: invalid stat index", __func__));
            }
            value = ps->stats[value];
            if (!value) {   // skip to endif
                if (cgix.IsExtendedServer()) {
                    SCR_SkipToEndif(&s);
                } else while (strcmp(token, "endif")) {
                    token = COM_Parse(&s);
                    if (!s) {
                        break;
                    }
                }
            }
            continue;
        }

        // Q2PRO extension
        if (!strcmp(token, "color")) {
            color_t     color;

            token = COM_Parse(&s);
            if (SCR_ParseColor(token, &color)) {
                cgix.SetColor(color.u32);
            }
            continue;
        }
    }

    cgix.ClearColor();
}

// The status bar is a small layout program that is based on the stats array
static void SCR_DrawStats(vrect_t hud_vrect, int32_t playernum, const player_state_t *ps)
{
    if (scr_draw2d->integer <= 1)
        return;
    if (ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD)
        return;

    SCR_ExecuteLayoutString(hud_vrect, cgi.get_configstring(CS_STATUSBAR), playernum, ps);
}

static void SCR_DrawLayout(vrect_t hud_vrect, const cg_server_data_t *data, int32_t playernum, const player_state_t *ps)
{
    if (scr_draw2d->integer == 3 /*&& !Key_IsDown(K_F1)*/)
        return;     // turn off for GTV

    /*if (cls.demo.playback && Key_IsDown(K_F1))
        goto draw;*/

    if (!(ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT))
        return;

//draw:
    SCR_ExecuteLayoutString(hud_vrect, data->layout, playernum, ps);
}

#define DISPLAY_ITEMS   17

static void SCR_DrawInventory(vrect_t hud_vrect, const cg_server_data_t *data, const player_state_t *ps)
{
    int     i;
    int     num, selected_num, item;
    int     index[MAX_ITEMS];
    char    string[MAX_STRING_CHARS];
    int     x, y;
    const char  *bind;
    int     selected;
    int     top;

    if (!(ps->stats[STAT_LAYOUTS] & LAYOUTS_INVENTORY))
        return;

    selected = ps->stats[STAT_SELECTED_ITEM];

    num = 0;
    selected_num = 0;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (i == selected) {
            selected_num = num;
        }
        if (data->inventory[i]) {
            index[num++] = i;
        }
    }

    // determine scroll point
    top = selected_num - DISPLAY_ITEMS / 2;
    if (top > num - DISPLAY_ITEMS) {
        top = num - DISPLAY_ITEMS;
    }
    if (top < 0) {
        top = 0;
    }

    x = hud_vrect.x + (hud_vrect.width - 256) / 2;
    y = hud_vrect.y + (hud_vrect.height - 240) / 2;

    DrawPic(x, y + 8, inven_pic);
    y += 24;
    x += 24;

    HUD_DrawString(x, y, "hotkey ### item");
    y += CHAR_HEIGHT;

    HUD_DrawString(x, y, "------ --- ----");
    y += CHAR_HEIGHT;

    for (i = top; i < num && i < top + DISPLAY_ITEMS; i++) {
        item = index[i];
        // search for a binding
        Q_concat(string, sizeof(string), "use ", cgi.get_configstring(cs_remap_rerelease.items + item));
        bind = cgi.CL_GetKeyBinding(string);

        Q_snprintf(string, sizeof(string), "%6s %3i %s",
                   bind, data->inventory[item],
                   cgi.Localize(cgi.get_configstring(cs_remap_rerelease.items + item), NULL, 0));

        if (item != selected) {
            HUD_DrawAltString(x, y, string);
        } else {    // draw a blinky cursor by the selected item
            HUD_DrawString(x, y, string);
            if ((cgi.CL_ClientRealTime() >> 8) & 1) {
                cgi.SCR_DrawChar(x - CHAR_WIDTH, y, 1, 15, false);
            }
        }

        y += CHAR_HEIGHT;
    }
}

static void SCR_DrawCenterString(vrect_t hud_vrect)
{
    int y;
    float alpha;

    Cvar_ClampValue(scr_centertime, 0.3f, 10.0f);

    alpha = SCR_FadeAlpha(scr_centertime_start, scr_centertime->value * 1000, 300);
    if (!alpha) {
        return;
    }

    cgix.SetAlpha(alpha);

    y = hud_vrect.height / 4 - scr_center_lines * 8 / 2;

    CG_DrawStringMulti(hud_vrect.width / 2, y, UI_CENTER,
                       MAX_STRING_CHARS, scr_centerstring);

    cgix.ClearColor();
}

static void CGC_DrawHUD (int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps)
{
    // Note: isplit is ignored, due to missing split screen support

    SCR_DrawStats(hud_vrect, playernum, ps);

    SCR_DrawLayout(hud_vrect, data, playernum, ps);

    SCR_DrawInventory(hud_vrect, data, ps);

    SCR_DrawCenterString(hud_vrect);
}

static void CGC_ParseCenterPrint(const char *str, int isplit, bool instant)
{
    const char  *s;

    scr_centertime_start = cgi.CL_ClientRealTime();
    if (!strcmp(scr_centerstring, str)) {
        return;
    }

    Q_strlcpy(scr_centerstring, str, sizeof(scr_centerstring));

    // count the number of lines for centering
    scr_center_lines = 1;
    s = str;
    while (*s) {
        if (*s == '\n')
            scr_center_lines++;
        s++;
    }

    // echo it to the console
    cgi.Com_Print(va("%s\n", scr_centerstring));
    // Con_ClearNotify_f();
}

static void CGC_ClearCenterprint(int32_t isplit)
{
    scr_centerstring[0] = 0;
}

const char cgame_q2pro_extended_support_ext[] = "q2pro:extended_support";

cgame_export_t cgame_classic = {
    .apiversion = CGAME_API_VERSION,

    .Init = CGC_Init,
    .Shutdown = CGC_Shutdown,

    .DrawHUD = CGC_DrawHUD,

    .ParseCenterPrint = CGC_ParseCenterPrint,
    .ClearCenterprint = CGC_ClearCenterprint,
};

cgame_export_t *GetClassicCGameAPI(cgame_import_t *import)
{
    cgi = *import;
    cgix = *((cgame_q2pro_extended_support_ext_t*)import->GetExtension(cgame_q2pro_extended_support_ext));
    return &cgame_classic;
}
