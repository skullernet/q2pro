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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"
#include "src/jump/strafe_helper/strafe_helper.h"

#define STAT_PICS       11
#define STAT_MINUS      (STAT_PICS - 1)  // num frame for '-' stats digit

typedef struct {
    const char  *name;
    uint32_t    size;
    uint16_t    start;
    uint16_t    crop;
} cin_crop_t;

static struct {
    bool        initialized;        // ready to draw

    qhandle_t   crosshair_pic;
    int         crosshair_width, crosshair_height;
    color_t     crosshair_color;

    qhandle_t   hit_marker_pic;
    int         hit_marker_width, hit_marker_height;

    qhandle_t   pause_pic;

    qhandle_t   loading_pic;
    bool        draw_loading;

    qhandle_t   sb_pics[2][STAT_PICS];
    qhandle_t   inven_pic;
    qhandle_t   field_pic;

    qhandle_t   backtile_pic;

    qhandle_t   net_pic;
    qhandle_t   font_pic;

    int         hud_width, hud_height;
    float       hud_scale;
} scr;

static cvar_t   *scr_viewsize;
static cvar_t   *scr_centertime;
static cvar_t   *scr_printspeed;
static cvar_t   *scr_showpause;
#if USE_DEBUG
static cvar_t   *scr_showstats;
static cvar_t   *scr_showpmove;
#endif
static cvar_t   *scr_showturtle;

static cvar_t   *scr_netgraph;
static cvar_t   *scr_timegraph;
static cvar_t   *scr_debuggraph;
static cvar_t   *scr_graphheight;
static cvar_t   *scr_graphscale;
static cvar_t   *scr_graphshift;

static cvar_t   *scr_draw2d;
static cvar_t   *scr_lag_x;
static cvar_t   *scr_lag_y;
static cvar_t   *scr_lag_draw;
static cvar_t   *scr_lag_min;
static cvar_t   *scr_lag_max;
static cvar_t   *scr_alpha;

static cvar_t   *scr_demobar;
static cvar_t   *scr_font;
static cvar_t   *scr_scale;

static cvar_t   *scr_crosshair;

static cvar_t   *scr_chathud;
static cvar_t   *scr_chathud_lines;
static cvar_t   *scr_chathud_time;
static cvar_t   *scr_chathud_x;
static cvar_t   *scr_chathud_y;

static cvar_t   *ch_health;
static cvar_t   *ch_red;
static cvar_t   *ch_green;
static cvar_t   *ch_blue;
static cvar_t   *ch_alpha;

static cvar_t   *ch_scale;
static cvar_t   *ch_x;
static cvar_t   *ch_y;

static cvar_t   *scr_hit_marker_time;

vrect_t     scr_vrect;      // position of render window on screen

const uint32_t colorTable[8] = {
    U32_BLACK, U32_RED, U32_GREEN, U32_YELLOW,
    U32_BLUE, U32_CYAN, U32_MAGENTA, U32_WHITE
};

static const cin_crop_t cin_crop[] = {
    { "ntro.cin",   82836235, 727, 30 },
    { "end.cin",    19311290,   0, 30 },
    { "rintro.cin", 38434032,   0, 24 },
    { "rend.cin",   22580919,   0, 24 },
    { "xin.cin",    13226649,   0, 32 },
    { "xout.cin",   11194445,   0, 32 },
};

/*
===============================================================================

UTILS

===============================================================================
*/

#define SCR_DrawString(x, y, flags, string) \
    SCR_DrawStringEx(x, y, flags, MAX_STRING_CHARS, string, scr.font_pic)

/*
==============
SCR_DrawStringEx
==============
*/
int SCR_DrawStringEx(int x, int y, int flags, size_t maxlen,
                     const char *s, qhandle_t font)
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

    return R_DrawString(x, y, flags, maxlen, s, font);
}


/*
==============
SCR_DrawStringMulti
==============
*/
void SCR_DrawStringMulti(int x, int y, int flags, size_t maxlen,
                         const char *s, qhandle_t font)
{
    char    *p;
    size_t  len;
    int     last_x = x;
    int     last_y = y;

    while (*s && maxlen) {
        p = strchr(s, '\n');
        if (!p) {
            last_x = SCR_DrawStringEx(x, y, flags, maxlen, s, font);
            last_y = y;
            break;
        }

        len = min(p - s, maxlen);
        last_x = SCR_DrawStringEx(x, y, flags, len, s, font);
        last_y = y;
        maxlen -= len;

        y += CHAR_HEIGHT;
        s = p + 1;
    }

    if (flags & UI_DRAWCURSOR && com_localTime & BIT(8))
        R_DrawChar(last_x, last_y, flags, 11, font);
}


/*
=================
SCR_FadeAlpha
=================
*/
float SCR_FadeAlpha(unsigned startTime, unsigned visTime, unsigned fadeTime)
{
    float alpha;
    unsigned timeLeft, delta = cls.realtime - startTime;

    if (delta >= visTime) {
        return 0;
    }

    if (fadeTime > visTime) {
        fadeTime = visTime;
    }

    alpha = 1;
    timeLeft = visTime - delta;
    if (timeLeft < fadeTime) {
        alpha = (float)timeLeft / fadeTime;
    }

    return alpha;
}

bool SCR_ParseColor(const char *s, color_t *color)
{
    int i;
    int c[8];

    // parse generic color
    if (*s == '#') {
        s++;
        for (i = 0; s[i]; i++) {
            if (i == 8) {
                return false;
            }
            c[i] = Q_charhex(s[i]);
            if (c[i] == -1) {
                return false;
            }
        }

        switch (i) {
        case 3:
            color->u8[0] = c[0] | (c[0] << 4);
            color->u8[1] = c[1] | (c[1] << 4);
            color->u8[2] = c[2] | (c[2] << 4);
            color->u8[3] = 255;
            break;
        case 6:
            color->u8[0] = c[1] | (c[0] << 4);
            color->u8[1] = c[3] | (c[2] << 4);
            color->u8[2] = c[5] | (c[4] << 4);
            color->u8[3] = 255;
            break;
        case 8:
            color->u8[0] = c[1] | (c[0] << 4);
            color->u8[1] = c[3] | (c[2] << 4);
            color->u8[2] = c[5] | (c[4] << 4);
            color->u8[3] = c[7] | (c[6] << 4);
            break;
        default:
            return false;
        }

        return true;
    }

    // parse name or index
    i = Com_ParseColor(s);
    if (i >= q_countof(colorTable)) {
        return false;
    }

    color->u32 = colorTable[i];
    return true;
}

int SCR_GetCinematicCrop(unsigned framenum, int64_t filesize)
{
    const cin_crop_t *c;
    int i;

    for (i = 0, c = cin_crop; i < q_countof(cin_crop); i++, c++) {
        if (!Q_stricmp(cl.mapname, c->name)) {
            if (framenum >= c->start && filesize == c->size)
                return c->crop * 2;
            break;
        }
    }

    return 0;
}

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
SCR_AddNetgraph

A new packet was just parsed
==============
*/
void SCR_AddNetgraph(void)
{
    int         i, color;
    unsigned    ping;

    // if using the debuggraph for something else, don't
    // add the net lines
    if (scr_debuggraph->integer || scr_timegraph->integer)
        return;

    for (i = 0; i < cls.netchan.dropped; i++)
        SCR_DebugGraph(30, 0x40);

    for (i = 0; i < cl.suppress_count; i++)
        SCR_DebugGraph(30, 0xdf);

    if (scr_netgraph->integer > 1) {
        ping = msg_read.cursize;
        if (ping < 200)
            color = 61;
        else if (ping < 500)
            color = 59;
        else if (ping < 800)
            color = 57;
        else if (ping < 1200)
            color = 224;
        else
            color = 242;
        ping /= 40;
    } else {
        // see what the latency was on this packet
        i = cls.netchan.incoming_acknowledged & CMD_MASK;
        ping = (cls.realtime - cl.history[i].sent) / 30;
        color = 0xd0;
    }

    SCR_DebugGraph(min(ping, 30), color);
}

#define GRAPH_SAMPLES   4096
#define GRAPH_MASK      (GRAPH_SAMPLES - 1)

static struct {
    float       values[GRAPH_SAMPLES];
    byte        colors[GRAPH_SAMPLES];
    unsigned    current;
} graph;

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph(float value, int color)
{
    graph.values[graph.current & GRAPH_MASK] = value;
    graph.colors[graph.current & GRAPH_MASK] = color;
    graph.current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
static void SCR_DrawDebugGraph(void)
{
    int     a, y, w, i, h, height;
    float   v, scale, shift;

    scale = scr_graphscale->value;
    shift = scr_graphshift->value;
    height = scr_graphheight->integer;
    if (height < 1)
        return;

    w = scr.hud_width;
    y = scr.hud_height;

    for (a = 0; a < w; a++) {
        i = (graph.current - 1 - a) & GRAPH_MASK;
        v = graph.values[i] * scale + shift;

        if (v < 0)
            v += height * (1 + (int)(-v / height));

        h = (int)v % height;
        R_DrawFill8(w - 1 - a, y - h, 1, h, graph.colors[i]);
    }
}

/*
===============================================================================

DEMO BAR

===============================================================================
*/

static void draw_progress_bar(float progress, bool paused, int framenum)
{
    char buffer[16];
    int x, w, h;
    size_t len;

    w = Q_rint(scr.hud_width * progress);
    h = Q_rint(CHAR_HEIGHT / scr.hud_scale);

    scr.hud_height -= h;

    R_DrawFill8(0, scr.hud_height, w, h, 4);
    R_DrawFill8(w, scr.hud_height, scr.hud_width - w, h, 0);

    R_SetScale(scr.hud_scale);

    w = Q_rint(scr.hud_width * scr.hud_scale);
    h = Q_rint(scr.hud_height * scr.hud_scale);

    len = Q_scnprintf(buffer, sizeof(buffer), "%.f%%", progress * 100);
    x = (w - len * CHAR_WIDTH) / 2;
    R_DrawString(x, h, 0, MAX_STRING_CHARS, buffer, scr.font_pic);

    if (scr_demobar->integer > 1) {
        int sec = framenum / BASE_FRAMERATE;
        int min = sec / 60; sec %= 60;

        Q_scnprintf(buffer, sizeof(buffer), "%d:%02d.%d", min, sec, framenum % BASE_FRAMERATE);
        R_DrawString(0, h, 0, MAX_STRING_CHARS, buffer, scr.font_pic);
    }

    if (paused) {
        SCR_DrawString(w, h, UI_RIGHT, "[PAUSED]");
    }

    R_SetScale(1.0f);
}

static void SCR_DrawDemo(void)
{
#if USE_MVD_CLIENT
    float progress;
    bool paused;
    int framenum;
#endif

    if (!scr_demobar->integer) {
        return;
    }

    if (cls.demo.playback) {
        if (cls.demo.file_size && !cls.demo.compat) {
            draw_progress_bar(
                cls.demo.file_progress,
                sv_paused->integer &&
                cl_paused->integer &&
                scr_showpause->integer == 2,
                cls.demo.frames_read);
        }
        return;
    }

#if USE_MVD_CLIENT
    if (sv_running->integer != ss_broadcast) {
        return;
    }

    if (!MVD_GetDemoStatus(&progress, &paused, &framenum)) {
        return;
    }

    if (sv_paused->integer && cl_paused->integer && scr_showpause->integer == 2) {
        paused = true;
    }

    draw_progress_bar(progress, paused, framenum);
#endif
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

#define MAX_CENTERPRINTS_REAL   4
#define MAX_CENTERPRINTS        (cl.csr.extended ? MAX_CENTERPRINTS_REAL : 1)

typedef struct {
    char        string[MAX_STRING_CHARS - 8];
    uint32_t    start;
    uint16_t    lines;
    uint16_t    typewrite;  // msec to typewrite (0 if instant)
} centerprint_t;

static centerprint_t    scr_centerprints[MAX_CENTERPRINTS_REAL];
static unsigned         scr_centerhead, scr_centertail;

void SCR_ClearCenterPrints(void)
{
    memset(scr_centerprints, 0, sizeof(scr_centerprints));
    scr_centerhead = scr_centertail = 0;
}

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint(const char *str, bool typewrite)
{
    centerprint_t *cp;
    const char *s;

    // refresh duplicate message
    cp = &scr_centerprints[(scr_centerhead - 1) & (MAX_CENTERPRINTS - 1)];
    if (!strcmp(cp->string, str)) {
        if (cp->start)
            cp->start = cls.realtime;
        if (scr_centertail == scr_centerhead)
            scr_centertail--;
        return;
    }

    cp = &scr_centerprints[scr_centerhead & (MAX_CENTERPRINTS - 1)];
    Q_strlcpy(cp->string, str, sizeof(cp->string));

    // count the number of lines for centering
    cp->lines = 1;
    s = cp->string;
    while (*s) {
        if (*s == '\n')
            cp->lines++;
        s++;
    }

    cp->start = 0;  // not yet displayed
    cp->typewrite = 0;

    // for typewritten strings set minimum display time,
    // but no longer than 30 sec
    if (typewrite && scr_printspeed->value > 0) {
        size_t nb_chars = strlen(cp->string) - cp->lines + 2;
        cp->typewrite = min(nb_chars * 1000 / scr_printspeed->value + 300, 30000);
    }

    // echo it to the console
    Com_Printf("%s\n", cp->string);
    Con_ClearNotify_f();

    scr_centerhead++;
    if (scr_centerhead - scr_centertail > MAX_CENTERPRINTS)
        scr_centertail++;
}

static void SCR_DrawCenterString(void)
{
    centerprint_t *cp;
    int y, flags;
    float alpha;
    size_t maxlen;

    if (!scr_centertime->integer) {
        scr_centertail = scr_centerhead;
        return;
    }

    while (1) {
        if (scr_centertail == scr_centerhead)
            return;
        cp = &scr_centerprints[scr_centertail & (MAX_CENTERPRINTS - 1)];
        if (!cp->start)
            cp->start = cls.realtime;
        alpha = SCR_FadeAlpha(cp->start, scr_centertime->integer + cp->typewrite, 300);
        if (alpha > 0)
            break;
        scr_centertail++;
    }

    R_SetAlpha(alpha * scr_alpha->value);

    y = scr.hud_height / 4 - cp->lines * CHAR_HEIGHT / 2;
    flags = UI_CENTER;

    if (cp->typewrite) {
        maxlen = scr_printspeed->value * 0.001f * (cls.realtime - cp->start);
        flags |= UI_DROPSHADOW | UI_DRAWCURSOR;
    } else {
        maxlen = MAX_STRING_CHARS;
    }

    SCR_DrawStringMulti(scr.hud_width / 2, y, flags,
                        maxlen, cp->string, scr.font_pic);

    R_SetAlpha(scr_alpha->value);
}

static void scr_centertime_changed(cvar_t *self)
{
    if (self->value > 0)
        self->integer = 1000 * Cvar_ClampValue(self, 1.0f, 30.0f);
    else
        self->integer = 0;
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_WIDTH   48
#define LAG_HEIGHT  48

#define LAG_WARN_BIT    BIT(30)
#define LAG_CRIT_BIT    BIT(31)

#define LAG_BASE    0xD5
#define LAG_WARN    0xDC
#define LAG_CRIT    0xF2

static struct {
    unsigned samples[LAG_WIDTH];
    unsigned head;
} lag;

void SCR_LagClear(void)
{
    lag.head = 0;
}

void SCR_LagSample(void)
{
    int i = cls.netchan.incoming_acknowledged & CMD_MASK;
    client_history_t *h = &cl.history[i];
    unsigned ping;

    h->rcvd = cls.realtime;
    if (!h->cmdNumber || h->rcvd < h->sent) {
        return;
    }

    ping = h->rcvd - h->sent;
    for (i = 0; i < cls.netchan.dropped; i++) {
        lag.samples[lag.head % LAG_WIDTH] = ping | LAG_CRIT_BIT;
        lag.head++;
    }

    if (cl.frameflags & FF_SUPPRESSED) {
        ping |= LAG_WARN_BIT;
    }
    lag.samples[lag.head % LAG_WIDTH] = ping;
    lag.head++;
}

static void SCR_LagDraw(int x, int y)
{
    int i, j, v, c, v_min, v_max, v_range;

    v_min = Cvar_ClampInteger(scr_lag_min, 0, LAG_HEIGHT * 10);
    v_max = Cvar_ClampInteger(scr_lag_max, 0, LAG_HEIGHT * 10);

    v_range = v_max - v_min;
    if (v_range < 1)
        return;

    for (i = 0; i < LAG_WIDTH; i++) {
        j = lag.head - i - 1;
        if (j < 0) {
            break;
        }

        v = lag.samples[j % LAG_WIDTH];

        if (v & LAG_CRIT_BIT) {
            c = LAG_CRIT;
        } else if (v & LAG_WARN_BIT) {
            c = LAG_WARN;
        } else {
            c = LAG_BASE;
        }

        v &= ~(LAG_WARN_BIT | LAG_CRIT_BIT);
        v = Q_clip((v - v_min) * LAG_HEIGHT / v_range, 0, LAG_HEIGHT);

        R_DrawFill8(x + LAG_WIDTH - i - 1, y + LAG_HEIGHT - v, 1, v, c);
    }
}

static void SCR_DrawNet(void)
{
    int x = scr_lag_x->integer;
    int y = scr_lag_y->integer;

    if (x < 0) {
        x += scr.hud_width - LAG_WIDTH + 1;
    }
    if (y < 0) {
        y += scr.hud_height - LAG_HEIGHT + 1;
    }

    // draw ping graph
    if (scr_lag_draw->integer) {
        if (scr_lag_draw->integer > 1) {
            R_DrawFill8(x, y, LAG_WIDTH, LAG_HEIGHT, 4);
        }
        SCR_LagDraw(x, y);
    }

    // draw phone jack
    if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged >= CMD_BACKUP) {
        if ((cls.realtime >> 8) & 3) {
            R_DrawStretchPic(x, y, LAG_WIDTH, LAG_HEIGHT, scr.net_pic);
        }
    }
}


/*
===============================================================================

DRAW OBJECTS

===============================================================================
*/

typedef struct {
    list_t          entry;
    int             x, y;
    cvar_t          *cvar;
    cmd_macro_t     *macro;
    int             flags;
    color_t         color;
} drawobj_t;

#define FOR_EACH_DRAWOBJ(obj) \
    LIST_FOR_EACH(drawobj_t, obj, &scr_objects, entry)
#define FOR_EACH_DRAWOBJ_SAFE(obj, next) \
    LIST_FOR_EACH_SAFE(drawobj_t, obj, next, &scr_objects, entry)

static LIST_DECL(scr_objects);

static void SCR_Color_g(genctx_t *ctx)
{
    int color;

    for (color = 0; color < COLOR_COUNT; color++)
        Prompt_AddMatch(ctx, colorNames[color]);
}

static void SCR_Draw_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        Cvar_Variable_g(ctx);
        Cmd_Macro_g(ctx);
    } else if (argnum == 4) {
        SCR_Color_g(ctx);
    }
}

// draw cl_fps -1 80
static void SCR_Draw_f(void)
{
    int x, y;
    const char *s, *c;
    drawobj_t *obj;
    cmd_macro_t *macro;
    cvar_t *cvar;
    color_t color;
    int flags;
    int argc = Cmd_Argc();

    if (argc == 1) {
        if (LIST_EMPTY(&scr_objects)) {
            Com_Printf("No draw strings registered.\n");
            return;
        }
        Com_Printf("Name               X    Y\n"
                   "--------------- ---- ----\n");
        FOR_EACH_DRAWOBJ(obj) {
            s = obj->macro ? obj->macro->name : obj->cvar->name;
            Com_Printf("%-15s %4d %4d\n", s, obj->x, obj->y);
        }
        return;
    }

    if (argc < 4) {
        Com_Printf("Usage: %s <name> <x> <y> [color]\n", Cmd_Argv(0));
        return;
    }

    color.u32 = U32_BLACK;
    flags = UI_IGNORECOLOR;

    s = Cmd_Argv(1);
    x = Q_atoi(Cmd_Argv(2));
    y = Q_atoi(Cmd_Argv(3));

    if (x < 0) {
        flags |= UI_RIGHT;
    }

    if (argc > 4) {
        c = Cmd_Argv(4);
        if (!strcmp(c, "alt")) {
            flags |= UI_ALTCOLOR;
        } else if (strcmp(c, "none")) {
            if (!SCR_ParseColor(c, &color)) {
                Com_Printf("Unknown color '%s'\n", c);
                return;
            }
            flags &= ~UI_IGNORECOLOR;
        }
    }

    cvar = NULL;
    macro = Cmd_FindMacro(s);
    if (!macro) {
        cvar = Cvar_WeakGet(s);
    }

    FOR_EACH_DRAWOBJ(obj) {
        if (obj->macro == macro && obj->cvar == cvar) {
            obj->x = x;
            obj->y = y;
            obj->flags = flags;
            obj->color.u32 = color.u32;
            return;
        }
    }

    obj = Z_Malloc(sizeof(*obj));
    obj->x = x;
    obj->y = y;
    obj->cvar = cvar;
    obj->macro = macro;
    obj->flags = flags;
    obj->color.u32 = color.u32;

    List_Append(&scr_objects, &obj->entry);
}

static void SCR_Draw_g(genctx_t *ctx)
{
    drawobj_t *obj;
    const char *s;

    if (LIST_EMPTY(&scr_objects)) {
        return;
    }

    Prompt_AddMatch(ctx, "all");

    FOR_EACH_DRAWOBJ(obj) {
        s = obj->macro ? obj->macro->name : obj->cvar->name;
        Prompt_AddMatch(ctx, s);
    }
}

static void SCR_UnDraw_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        SCR_Draw_g(ctx);
    }
}

static void SCR_UnDraw_f(void)
{
    char *s;
    drawobj_t *obj, *next;
    cmd_macro_t *macro;
    cvar_t *cvar;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
        return;
    }

    if (LIST_EMPTY(&scr_objects)) {
        Com_Printf("No draw strings registered.\n");
        return;
    }

    s = Cmd_Argv(1);
    if (!strcmp(s, "all")) {
        FOR_EACH_DRAWOBJ_SAFE(obj, next) {
            Z_Free(obj);
        }
        List_Init(&scr_objects);
        Com_Printf("Deleted all draw strings.\n");
        return;
    }

    cvar = NULL;
    macro = Cmd_FindMacro(s);
    if (!macro) {
        cvar = Cvar_WeakGet(s);
    }

    FOR_EACH_DRAWOBJ_SAFE(obj, next) {
        if (obj->macro == macro && obj->cvar == cvar) {
            List_Remove(&obj->entry);
            Z_Free(obj);
            return;
        }
    }

    Com_Printf("Draw string '%s' not found.\n", s);
}

static void SCR_DrawObjects(void)
{
    char buffer[MAX_QPATH];
    int x, y;
    drawobj_t *obj;

    FOR_EACH_DRAWOBJ(obj) {
        x = obj->x;
        y = obj->y;
        if (x < 0) {
            x += scr.hud_width + 1;
        }
        if (y < 0) {
            y += scr.hud_height - CHAR_HEIGHT + 1;
        }
        if (!(obj->flags & UI_IGNORECOLOR)) {
            R_SetColor(obj->color.u32);
        }
        if (obj->macro) {
            obj->macro->function(buffer, sizeof(buffer));
            SCR_DrawString(x, y, obj->flags, buffer);
        } else {
            SCR_DrawString(x, y, obj->flags, obj->cvar->string);
        }
        if (!(obj->flags & UI_IGNORECOLOR)) {
            R_ClearColor();
            R_SetAlpha(scr_alpha->value);
        }
    }
}

/*
===============================================================================

CHAT HUD

===============================================================================
*/

#define MAX_CHAT_TEXT       150
#define MAX_CHAT_LINES      32
#define CHAT_LINE_MASK      (MAX_CHAT_LINES - 1)

typedef struct {
    char        text[MAX_CHAT_TEXT];
    unsigned    time;
} chatline_t;

static chatline_t   scr_chatlines[MAX_CHAT_LINES];
static unsigned     scr_chathead;

void SCR_ClearChatHUD_f(void)
{
    memset(scr_chatlines, 0, sizeof(scr_chatlines));
    scr_chathead = 0;
}

void SCR_AddToChatHUD(const char *text)
{
    chatline_t *line;
    char *p;

    line = &scr_chatlines[scr_chathead++ & CHAT_LINE_MASK];
    Q_strlcpy(line->text, text, sizeof(line->text));
    line->time = cls.realtime;

    p = strrchr(line->text, '\n');
    if (p)
        *p = 0;
}

static void SCR_DrawChatHUD(void)
{
    int x, y, i, lines, flags, step;
    float alpha;
    chatline_t *line;

    if (scr_chathud->integer == 0)
        return;

    x = scr_chathud_x->integer;
    y = scr_chathud_y->integer;

    if (scr_chathud->integer == 2)
        flags = UI_ALTCOLOR;
    else
        flags = 0;

    if (x < 0) {
        x += scr.hud_width + 1;
        flags |= UI_RIGHT;
    } else {
        flags |= UI_LEFT;
    }

    if (y < 0) {
        y += scr.hud_height - CHAR_HEIGHT + 1;
        step = -CHAR_HEIGHT;
    } else {
        step = CHAR_HEIGHT;
    }

    lines = scr_chathud_lines->integer;
    if (lines > scr_chathead)
        lines = scr_chathead;

    for (i = 0; i < lines; i++) {
        line = &scr_chatlines[(scr_chathead - i - 1) & CHAT_LINE_MASK];

        if (scr_chathud_time->integer) {
            alpha = SCR_FadeAlpha(line->time, scr_chathud_time->integer, 1000);
            if (!alpha)
                break;

            R_SetAlpha(alpha * scr_alpha->value);
            SCR_DrawString(x, y, flags, line->text);
            R_SetAlpha(scr_alpha->value);
        } else {
            SCR_DrawString(x, y, flags, line->text);
        }

        y += step;
    }
}

/*
===============================================================================

DEBUG STUFF

===============================================================================
*/

static void SCR_DrawTurtle(void)
{
    int x, y;

    if (scr_showturtle->integer <= 0)
        return;

    if (!cl.frameflags)
        return;

    x = CHAR_WIDTH;
    y = scr.hud_height - 11 * CHAR_HEIGHT;

#define DF(f) \
    if (cl.frameflags & FF_##f) { \
        SCR_DrawString(x, y, UI_ALTCOLOR, #f); \
        y += CHAR_HEIGHT; \
    }

    if (scr_showturtle->integer > 1) {
        DF(SUPPRESSED)
    }
    DF(CLIENTPRED)
    if (scr_showturtle->integer > 1) {
        DF(CLIENTDROP)
        DF(SERVERDROP)
    }
    DF(BADFRAME)
    DF(OLDFRAME)
    DF(OLDENT)
    DF(NODELTA)

#undef DF
}

#if USE_DEBUG

static void SCR_DrawDebugStats(void)
{
    char buffer[MAX_QPATH];
    int i, j;
    int x, y;

    j = scr_showstats->integer;
    if (j <= 0)
        return;

    if (j > MAX_STATS)
        j = MAX_STATS;

    x = CHAR_WIDTH;
    y = (scr.hud_height - j * CHAR_HEIGHT) / 2;
    for (i = 0; i < j; i++) {
        Q_snprintf(buffer, sizeof(buffer), "%2d: %d", i, cl.frame.ps.stats[i]);
        if (cl.oldframe.ps.stats[i] != cl.frame.ps.stats[i]) {
            R_SetColor(U32_RED);
        }
        R_DrawString(x, y, 0, MAX_STRING_CHARS, buffer, scr.font_pic);
        R_ClearColor();
        y += CHAR_HEIGHT;
    }
}

static void SCR_DrawDebugPmove(void)
{
    static const char * const types[] = {
        "NORMAL", "SPECTATOR", "DEAD", "GIB", "FREEZE"
    };
    static const char * const flags[] = {
        "DUCKED", "JUMP_HELD", "ON_GROUND",
        "TIME_WATERJUMP", "TIME_LAND", "TIME_TELEPORT",
        "NO_PREDICTION", "TELEPORT_BIT"
    };
    unsigned i, j;
    int x, y;

    if (!scr_showpmove->integer)
        return;

    x = CHAR_WIDTH;
    y = (scr.hud_height - 2 * CHAR_HEIGHT) / 2;

    i = cl.frame.ps.pmove.pm_type;
    if (i > PM_FREEZE)
        i = PM_FREEZE;

    R_DrawString(x, y, 0, MAX_STRING_CHARS, types[i], scr.font_pic);
    y += CHAR_HEIGHT;

    j = cl.frame.ps.pmove.pm_flags;
    for (i = 0; i < 8; i++) {
        if (j & (1 << i)) {
            x = R_DrawString(x, y, 0, MAX_STRING_CHARS, flags[i], scr.font_pic);
            x += CHAR_WIDTH;
        }
    }
}

#endif

//============================================================================

// Sets scr_vrect, the coordinates of the rendered window
static void SCR_CalcVrect(void)
{
    int     size;

    // bound viewsize
    size = Cvar_ClampInteger(scr_viewsize, 40, 100);

    scr_vrect.width = scr.hud_width * size / 100;
    scr_vrect.height = scr.hud_height * size / 100;

    scr_vrect.x = (scr.hud_width - scr_vrect.width) / 2;
    scr_vrect.y = (scr.hud_height - scr_vrect.height) / 2;
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
static void SCR_SizeUp_f(void)
{
    Cvar_SetInteger(scr_viewsize, scr_viewsize->integer + 10, FROM_CONSOLE);
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
static void SCR_SizeDown_f(void)
{
    Cvar_SetInteger(scr_viewsize, scr_viewsize->integer - 10, FROM_CONSOLE);
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed. If empty sky name is provided, falls
back to server defaults.
=================
*/
static void SCR_Sky_f(void)
{
    char    *name;
    float   rotate;
    vec3_t  axis;
    int     argc = Cmd_Argc();

    if (argc < 2) {
        Com_Printf("Usage: sky <basename> [rotate] [axis x y z]\n");
        return;
    }

    if (cls.state != ca_active) {
        Com_Printf("No map loaded.\n");
        return;
    }

    name = Cmd_Argv(1);
    if (!*name) {
        CL_SetSky();
        return;
    }

    if (argc > 2)
        rotate = Q_atof(Cmd_Argv(2));
    else
        rotate = 0;

    if (argc == 6) {
        axis[0] = Q_atof(Cmd_Argv(3));
        axis[1] = Q_atof(Cmd_Argv(4));
        axis[2] = Q_atof(Cmd_Argv(5));
    } else
        VectorSet(axis, 0, 0, 1);

    R_SetSky(name, rotate, true, axis);
}

/*
================
SCR_TimeRefresh_f
================
*/
static void SCR_TimeRefresh_f(void)
{
    int     i;
    unsigned    start, stop;
    float       time;

    if (cls.state != ca_active) {
        Com_Printf("No map loaded.\n");
        return;
    }

    start = Sys_Milliseconds();

    if (Cmd_Argc() == 2) {
        // run without page flipping
        R_BeginFrame();
        for (i = 0; i < 128; i++) {
            cl.refdef.viewangles[1] = i / 128.0f * 360.0f;
            R_RenderFrame(&cl.refdef);
        }
        R_EndFrame();
    } else {
        for (i = 0; i < 128; i++) {
            cl.refdef.viewangles[1] = i / 128.0f * 360.0f;

            R_BeginFrame();
            R_RenderFrame(&cl.refdef);
            R_EndFrame();
        }
    }

    stop = Sys_Milliseconds();
    time = (stop - start) * 0.001f;
    Com_Printf("%f seconds (%f fps)\n", time, 128.0f / time);
}


//============================================================================

static void ch_scale_changed(cvar_t *self)
{
    int w, h;
    float scale;

    scale = Cvar_ClampValue(self, 0.1f, 9.0f);

    // prescale
    R_GetPicSize(&w, &h, scr.crosshair_pic);
    scr.crosshair_width = Q_rint(w * scale);
    scr.crosshair_height = Q_rint(h * scale);

    R_GetPicSize(&w, &h, scr.hit_marker_pic);
    scr.hit_marker_width = Q_rint(w * scale);
    scr.hit_marker_height = Q_rint(h * scale);
}

static void ch_color_changed(cvar_t *self)
{
    if (ch_health->integer) {
        SCR_SetCrosshairColor();
    } else {
        scr.crosshair_color.u8[0] = Cvar_ClampValue(ch_red, 0, 1) * 255;
        scr.crosshair_color.u8[1] = Cvar_ClampValue(ch_green, 0, 1) * 255;
        scr.crosshair_color.u8[2] = Cvar_ClampValue(ch_blue, 0, 1) * 255;
    }
    scr.crosshair_color.u8[3] = Cvar_ClampValue(ch_alpha, 0, 1) * 255;
}

static void scr_crosshair_changed(cvar_t *self)
{
    if (self->integer > 0) {
        scr.crosshair_pic = R_RegisterPic(va("ch%i", self->integer));
        ch_scale_changed(ch_scale);
    } else {
        scr.crosshair_pic = 0;
    }
}

void SCR_SetCrosshairColor(void)
{
    int health;

    if (!ch_health->integer) {
        return;
    }

    health = cl.frame.ps.stats[STAT_HEALTH];
    if (health <= 0) {
        VectorSet(scr.crosshair_color.u8, 0, 0, 0);
        return;
    }

    // red
    scr.crosshair_color.u8[0] = 255;

    // green
    if (health >= 66) {
        scr.crosshair_color.u8[1] = 255;
    } else if (health < 33) {
        scr.crosshair_color.u8[1] = 0;
    } else {
        scr.crosshair_color.u8[1] = (255 * (health - 33)) / 33;
    }

    // blue
    if (health >= 99) {
        scr.crosshair_color.u8[2] = 255;
    } else if (health < 66) {
        scr.crosshair_color.u8[2] = 0;
    } else {
        scr.crosshair_color.u8[2] = (255 * (health - 66)) / 33;
    }
}

void SCR_ModeChanged(void)
{
    IN_Activate();
    Con_CheckResize();
    UI_ModeChanged();
    cls.disable_screen = 0;
    if (scr.initialized)
        scr.hud_scale = R_ClampScale(scr_scale);
}

static void scr_font_changed(cvar_t *self)
{
    scr.font_pic = R_RegisterFont(self->string);
    if (!scr.font_pic && strcmp(self->string, self->default_string)) {
        Cvar_Reset(self);
        scr.font_pic = R_RegisterFont(self->default_string);
    }
}

/*
==================
SCR_RegisterMedia
==================
*/
void SCR_RegisterMedia(void)
{
    int     i;

    for (i = 0; i < STAT_MINUS; i++)
        scr.sb_pics[0][i] = R_RegisterPic(va("num_%d", i));
    scr.sb_pics[0][i] = R_RegisterPic("num_minus");

    for (i = 0; i < STAT_MINUS; i++)
        scr.sb_pics[1][i] = R_RegisterPic(va("anum_%d", i));
    scr.sb_pics[1][i] = R_RegisterPic("anum_minus");

    scr.inven_pic = R_RegisterPic("inventory");
    scr.field_pic = R_RegisterPic("field_3");
    scr.backtile_pic = R_RegisterImage("backtile", IT_PIC, IF_PERMANENT | IF_REPEAT);
    scr.pause_pic = R_RegisterPic("pause");
    scr.loading_pic = R_RegisterPic("loading");
    scr.net_pic = R_RegisterPic("net");
    scr.hit_marker_pic = R_RegisterImage("marker", IT_PIC, IF_PERMANENT | IF_OPTIONAL);

    scr_crosshair_changed(scr_crosshair);
    scr_font_changed(scr_font);
}

static void scr_scale_changed(cvar_t *self)
{
    scr.hud_scale = R_ClampScale(self);
}

static const cmdreg_t scr_cmds[] = {
    { "timerefresh", SCR_TimeRefresh_f },
    { "sizeup", SCR_SizeUp_f },
    { "sizedown", SCR_SizeDown_f },
    { "sky", SCR_Sky_f },
    { "draw", SCR_Draw_f, SCR_Draw_c },
    { "undraw", SCR_UnDraw_f, SCR_UnDraw_c },
    { "clearchathud", SCR_ClearChatHUD_f },
    { NULL }
};

/*
==================
SCR_Init
==================
*/
void SCR_Init(void)
{
    scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
    scr_showpause = Cvar_Get("scr_showpause", "1", 0);
    scr_centertime = Cvar_Get("scr_centertime", "2.5", 0);
    scr_centertime->changed = scr_centertime_changed;
    scr_centertime->changed(scr_centertime);
    scr_printspeed = Cvar_Get("scr_printspeed", "16", 0);
    scr_demobar = Cvar_Get("scr_demobar", "1", 0);
    scr_font = Cvar_Get("scr_font", "conchars", 0);
    scr_font->changed = scr_font_changed;
    scr_scale = Cvar_Get("scr_scale", "0", 0);
    scr_scale->changed = scr_scale_changed;
    scr_crosshair = Cvar_Get("crosshair", "0", CVAR_ARCHIVE);
    scr_crosshair->changed = scr_crosshair_changed;

    scr_netgraph = Cvar_Get("netgraph", "0", 0);
    scr_timegraph = Cvar_Get("timegraph", "0", 0);
    scr_debuggraph = Cvar_Get("debuggraph", "0", 0);
    scr_graphheight = Cvar_Get("graphheight", "32", 0);
    scr_graphscale = Cvar_Get("graphscale", "1", 0);
    scr_graphshift = Cvar_Get("graphshift", "0", 0);

    scr_chathud = Cvar_Get("scr_chathud", "0", 0);
    scr_chathud_lines = Cvar_Get("scr_chathud_lines", "4", 0);
    scr_chathud_time = Cvar_Get("scr_chathud_time", "0", 0);
    scr_chathud_time->changed = cl_timeout_changed;
    scr_chathud_time->changed(scr_chathud_time);
    scr_chathud_x = Cvar_Get("scr_chathud_x", "8", 0);
    scr_chathud_y = Cvar_Get("scr_chathud_y", "-64", 0);

    ch_health = Cvar_Get("ch_health", "0", 0);
    ch_health->changed = ch_color_changed;
    ch_red = Cvar_Get("ch_red", "1", 0);
    ch_red->changed = ch_color_changed;
    ch_green = Cvar_Get("ch_green", "1", 0);
    ch_green->changed = ch_color_changed;
    ch_blue = Cvar_Get("ch_blue", "1", 0);
    ch_blue->changed = ch_color_changed;
    ch_alpha = Cvar_Get("ch_alpha", "1", 0);
    ch_alpha->changed = ch_color_changed;

    ch_scale = Cvar_Get("ch_scale", "1", 0);
    ch_scale->changed = ch_scale_changed;
    ch_x = Cvar_Get("ch_x", "0", 0);
    ch_y = Cvar_Get("ch_y", "0", 0);

    scr_draw2d = Cvar_Get("scr_draw2d", "2", 0);
    scr_showturtle = Cvar_Get("scr_showturtle", "1", 0);
    scr_lag_x = Cvar_Get("scr_lag_x", "-1", 0);
    scr_lag_y = Cvar_Get("scr_lag_y", "-1", 0);
    scr_lag_draw = Cvar_Get("scr_lag_draw", "0", 0);
    scr_lag_min = Cvar_Get("scr_lag_min", "0", 0);
    scr_lag_max = Cvar_Get("scr_lag_max", "200", 0);
    scr_alpha = Cvar_Get("scr_alpha", "1", 0);
#if USE_DEBUG
    scr_showstats = Cvar_Get("scr_showstats", "0", 0);
    scr_showpmove = Cvar_Get("scr_showpmove", "0", 0);
#endif

    scr_hit_marker_time = Cvar_Get("scr_hit_marker_time", "500", 0);

    Cmd_Register(scr_cmds);

    scr_scale_changed(scr_scale);
    ch_color_changed(NULL);

    scr.initialized = true;
}

void SCR_Shutdown(void)
{
    Cmd_Deregister(scr_cmds);
    scr.initialized = false;
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque(void)
{
    if (!cls.state) {
        return;
    }

    S_StopAllSounds();
    OGG_Stop();

    if (cls.disable_screen) {
        return;
    }

#if USE_DEBUG
    if (developer->integer) {
        return;
    }
#endif

    // if at console or menu, don't bring up the plaque
    if (cls.key_dest & (KEY_CONSOLE | KEY_MENU)) {
        return;
    }

    scr.draw_loading = true;
    SCR_UpdateScreen();

    cls.disable_screen = Sys_Milliseconds();
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque(void)
{
    if (!cls.state) {
        return;
    }
    cls.disable_screen = 0;
    Con_ClearNotify_f();
}

// Clear any parts of the tiled background that were drawn on last frame
static void SCR_TileClear(void)
{
    int top, bottom, left, right;

    //if (con.currentHeight == 1)
    //  return;     // full screen console

    if (scr_viewsize->integer == 100)
        return;     // full screen rendering

    top = scr_vrect.y;
    bottom = top + scr_vrect.height;
    left = scr_vrect.x;
    right = left + scr_vrect.width;

    // clear above view screen
    R_TileClear(0, 0, scr.hud_width, top, scr.backtile_pic);

    // clear below view screen
    R_TileClear(0, bottom, scr.hud_width,
                scr.hud_height - bottom, scr.backtile_pic);

    // clear left of view screen
    R_TileClear(0, top, left, scr_vrect.height, scr.backtile_pic);

    // clear right of view screen
    R_TileClear(right, top, scr.hud_width - right,
                scr_vrect.height, scr.backtile_pic);
}

/*
===============================================================================

STAT PROGRAMS

===============================================================================
*/

#define ICON_WIDTH  24
#define ICON_HEIGHT 24
#define DIGIT_WIDTH 16
#define ICON_SPACE  8

#define HUD_DrawString(x, y, string) \
    R_DrawString(x, y, 0, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawAltString(x, y, string) \
    R_DrawString(x, y, UI_XORCOLOR, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawCenterString(x, y, string) \
    SCR_DrawStringMulti(x, y, UI_CENTER, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawAltCenterString(x, y, string) \
    SCR_DrawStringMulti(x, y, UI_CENTER | UI_XORCOLOR, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawRightString(x, y, string) \
    SCR_DrawStringEx(x, y, UI_RIGHT, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawAltRightString(x, y, string) \
    SCR_DrawStringEx(x, y, UI_RIGHT | UI_XORCOLOR, MAX_STRING_CHARS, string, scr.font_pic)

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

        R_DrawPic(x, y, scr.sb_pics[color][frame]);
        x += DIGIT_WIDTH;
        ptr++;
        l--;
    }
}

#define DISPLAY_ITEMS   17

static void SCR_DrawInventory(void)
{
    int     i;
    int     num, selected_num, item;
    int     index[MAX_ITEMS];
    char    string[MAX_STRING_CHARS];
    int     x, y;
    const char  *bind;
    int     selected;
    int     top;

    if (!(cl.frame.ps.stats[STAT_LAYOUTS] & LAYOUTS_INVENTORY))
        return;

    selected = cl.frame.ps.stats[STAT_SELECTED_ITEM];

    num = 0;
    selected_num = 0;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (i == selected) {
            selected_num = num;
        }
        if (cl.inventory[i]) {
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

    x = (scr.hud_width - 256) / 2;
    y = (scr.hud_height - 240) / 2;

    R_DrawPic(x, y + 8, scr.inven_pic);
    y += 24;
    x += 24;

    HUD_DrawString(x, y, "hotkey ### item");
    y += CHAR_HEIGHT;

    HUD_DrawString(x, y, "------ --- ----");
    y += CHAR_HEIGHT;

    for (i = top; i < num && i < top + DISPLAY_ITEMS; i++) {
        item = index[i];
        // search for a binding
        Q_concat(string, sizeof(string), "use ", cl.configstrings[cl.csr.items + item]);
        bind = Key_GetBinding(string);

        Q_snprintf(string, sizeof(string), "%6s %3i %s",
                   bind, cl.inventory[item], cl.configstrings[cl.csr.items + item]);

        if (item != selected) {
            HUD_DrawAltString(x, y, string);
        } else {    // draw a blinky cursor by the selected item
            HUD_DrawString(x, y, string);
            if ((cls.realtime >> 8) & 1) {
                R_DrawChar(x - CHAR_WIDTH, y, 0, 15, scr.font_pic);
            }
        }

        y += CHAR_HEIGHT;
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
            COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "client")) {
            for (i = 0; i < 6; i++)
                COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "ctf")) {
            for (i = 0; i < 5; i++)
                COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "num") || !strcmp(token, "health_bars")) {
            COM_SkipToken(s);
            COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "hnum")) continue;
        if (!strcmp(token, "anum")) continue;
        if (!strcmp(token, "rnum")) continue;

        if (!strcmp(token, "if")) {
            COM_SkipToken(s);
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

static void SCR_DrawHealthBar(int x, int y, int value)
{
    if (!value)
        return;

    int bar_width = scr.hud_width / 3;
    float percent = (value - 1) / 254.0f;
    int w = bar_width * percent + 0.5f;
    int h = CHAR_HEIGHT / 2;

    x -= bar_width / 2;
    R_DrawFill8(x, y, w, h, 240);
    R_DrawFill8(x + w, y, bar_width - w, h, 4);
}

static void SCR_ExecuteLayoutString(const char *s)
{
    char    buffer[MAX_QPATH];
    int     x, y;
    int     value;
    char    *token;
    int     width;
    int     index;
    clientinfo_t    *ci;

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
                    x = Q_atoi(token);
                    continue;
                }

                if (token[1] == 'r') {
                    token = COM_Parse(&s);
                    x = scr.hud_width + Q_atoi(token);
                    continue;
                }

                if (token[1] == 'v') {
                    token = COM_Parse(&s);
                    x = scr.hud_width / 2 - 160 + Q_atoi(token);
                    continue;
                }
            }

            if (token[0] == 'y') {
                if (token[1] == 't') {
                    token = COM_Parse(&s);
                    y = Q_atoi(token);
                    continue;
                }

                if (token[1] == 'b') {
                    token = COM_Parse(&s);
                    y = scr.hud_height + Q_atoi(token);
                    continue;
                }

                if (token[1] == 'v') {
                    token = COM_Parse(&s);
                    y = scr.hud_height / 2 - 120 + Q_atoi(token);
                    continue;
                }
            }
        }

        if (!strcmp(token, "pic")) {
            // draw a pic from a stat number
            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                Com_Error(ERR_DROP, "%s: invalid stat index", __func__);
            }
            value = cl.frame.ps.stats[value];
            if (value < 0 || value >= cl.csr.max_images) {
                Com_Error(ERR_DROP, "%s: invalid pic index", __func__);
            }
            token = cl.configstrings[cl.csr.images + value];
            if (token[0]) {
                qhandle_t pic = cl.image_precache[value];
                // hack for action mod scope scaling
                if (x == scr.hud_width  / 2 - 160 &&
                    y == scr.hud_height / 2 - 120 &&
                    Com_WildCmp("scope?x", token))
                {
                    int w = 320 * ch_scale->value;
                    int h = 240 * ch_scale->value;
                    R_DrawStretchPic((scr.hud_width  - w) / 2 + ch_x->integer,
                                     (scr.hud_height - h) / 2 + ch_y->integer,
                                     w, h, pic);
                } else {
                    R_DrawPic(x, y, pic);
                }
            }
            continue;
        }

        if (!strcmp(token, "client")) {
            // draw a deathmatch client block
            int     score, ping, time;

            token = COM_Parse(&s);
            x = scr.hud_width / 2 - 160 + Q_atoi(token);
            token = COM_Parse(&s);
            y = scr.hud_height / 2 - 120 + Q_atoi(token);

            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_CLIENTS) {
                Com_Error(ERR_DROP, "%s: invalid client index", __func__);
            }
            ci = &cl.clientinfo[value];

            token = COM_Parse(&s);
            score = Q_atoi(token);

            token = COM_Parse(&s);
            ping = Q_atoi(token);

            token = COM_Parse(&s);
            time = Q_atoi(token);

            HUD_DrawAltString(x + 32, y, ci->name);
            HUD_DrawString(x + 32, y + CHAR_HEIGHT, "Score: ");
            Q_snprintf(buffer, sizeof(buffer), "%i", score);
            HUD_DrawAltString(x + 32 + 7 * CHAR_WIDTH, y + CHAR_HEIGHT, buffer);
            Q_snprintf(buffer, sizeof(buffer), "Ping:  %i", ping);
            HUD_DrawString(x + 32, y + 2 * CHAR_HEIGHT, buffer);
            Q_snprintf(buffer, sizeof(buffer), "Time:  %i", time);
            HUD_DrawString(x + 32, y + 3 * CHAR_HEIGHT, buffer);

            if (!ci->icon) {
                ci = &cl.baseclientinfo;
            }
            R_DrawPic(x, y, ci->icon);
            continue;
        }

        if (!strcmp(token, "ctf")) {
            // draw a ctf client block
            int     score, ping;

            token = COM_Parse(&s);
            x = scr.hud_width / 2 - 160 + Q_atoi(token);
            token = COM_Parse(&s);
            y = scr.hud_height / 2 - 120 + Q_atoi(token);

            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_CLIENTS) {
                Com_Error(ERR_DROP, "%s: invalid client index", __func__);
            }
            ci = &cl.clientinfo[value];

            token = COM_Parse(&s);
            score = Q_atoi(token);

            token = COM_Parse(&s);
            ping = Q_atoi(token);
            if (ping > 999)
                ping = 999;

            Q_snprintf(buffer, sizeof(buffer), "%3d %3d %-12.12s",
                       score, ping, ci->name);
            if (value == cl.frame.clientNum) {
                HUD_DrawAltString(x, y, buffer);
            } else {
                HUD_DrawString(x, y, buffer);
            }
            continue;
        }

        if (!strcmp(token, "picn")) {
            // draw a pic from a name
            token = COM_Parse(&s);
            R_DrawPic(x, y, R_RegisterTempPic(token));
            continue;
        }

        if (!strcmp(token, "num")) {
            // draw a number
            token = COM_Parse(&s);
            width = Q_atoi(token);
            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                Com_Error(ERR_DROP, "%s: invalid stat index", __func__);
            }
            value = cl.frame.ps.stats[value];
            HUD_DrawNumber(x, y, 0, width, value);
            continue;
        }

        if (!strcmp(token, "hnum")) {
            // health number
            int     color;

            width = 3;
            value = cl.frame.ps.stats[STAT_HEALTH];
            if (value > 25)
                color = 0;  // green
            else if (value > 0)
                color = ((cl.frame.number / CL_FRAMEDIV) >> 2) & 1;     // flash
            else
                color = 1;

            if (cl.frame.ps.stats[STAT_FLASHES] & 1)
                R_DrawPic(x, y, scr.field_pic);

            HUD_DrawNumber(x, y, color, width, value);
            continue;
        }

        if (!strcmp(token, "anum")) {
            // ammo number
            int     color;

            width = 3;
            value = cl.frame.ps.stats[STAT_AMMO];
            if (value > 5)
                color = 0;  // green
            else if (value >= 0)
                color = ((cl.frame.number / CL_FRAMEDIV) >> 2) & 1;     // flash
            else
                continue;   // negative number = don't show

            if (cl.frame.ps.stats[STAT_FLASHES] & 4)
                R_DrawPic(x, y, scr.field_pic);

            HUD_DrawNumber(x, y, color, width, value);
            continue;
        }

        if (!strcmp(token, "rnum")) {
            // armor number
            int     color;

            width = 3;
            value = cl.frame.ps.stats[STAT_ARMOR];
            if (value < 1)
                continue;

            color = 0;  // green

            if (cl.frame.ps.stats[STAT_FLASHES] & 2)
                R_DrawPic(x, y, scr.field_pic);

            HUD_DrawNumber(x, y, color, width, value);
            continue;
        }

        if (!strncmp(token, "stat_", 5)) {
            char *cmd = token + 5;
            token = COM_Parse(&s);
            index = Q_atoi(token);
            if (index < 0 || index >= MAX_STATS) {
                Com_Error(ERR_DROP, "%s: invalid stat index", __func__);
            }
            index = cl.frame.ps.stats[index];
            if (index < 0 || index >= cl.csr.end) {
                Com_Error(ERR_DROP, "%s: invalid string index", __func__);
            }
            token = cl.configstrings[index];
            if (!strcmp(cmd, "string"))
                HUD_DrawString(x, y, token);
            else if (!strcmp(cmd, "string2"))
                HUD_DrawAltString(x, y, token);
            else if (!strcmp(cmd, "cstring"))
                HUD_DrawCenterString(x + 320 / 2, y, token);
            else if (!strcmp(cmd, "cstring2"))
                HUD_DrawAltCenterString(x + 320 / 2, y, token);
            else if (!strcmp(cmd, "rstring"))
                HUD_DrawRightString(x, y, token);
            else if (!strcmp(cmd, "rstring2"))
                HUD_DrawAltRightString(x, y, token);
            continue;
        }

        if (!strcmp(token, "cstring")) {
            token = COM_Parse(&s);
            HUD_DrawCenterString(x + 320 / 2, y, token);
            continue;
        }

        if (!strcmp(token, "cstring2")) {
            token = COM_Parse(&s);
            HUD_DrawAltCenterString(x + 320 / 2, y, token);
            continue;
        }

        if (!strcmp(token, "string")) {
            token = COM_Parse(&s);
            HUD_DrawString(x, y, token);
            continue;
        }

        if (!strcmp(token, "string2")) {
            token = COM_Parse(&s);
            HUD_DrawAltString(x, y, token);
            continue;
        }

        if (!strcmp(token, "rstring")) {
            token = COM_Parse(&s);
            HUD_DrawRightString(x, y, token);
            continue;
        }

        if (!strcmp(token, "rstring2")) {
            token = COM_Parse(&s);
            HUD_DrawAltRightString(x, y, token);
            continue;
        }

        if (!strcmp(token, "if")) {
            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                Com_Error(ERR_DROP, "%s: invalid stat index", __func__);
            }
            value = cl.frame.ps.stats[value];
            if (!value) {   // skip to endif
                if (cl.csr.extended) {
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
                color.u8[3] *= scr_alpha->value;
                R_SetColor(color.u32);
            }
            continue;
        }

        if (!strcmp(token, "health_bars")) {
            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                Com_Error(ERR_DROP, "%s: invalid stat index", __func__);
            }
            value = cl.frame.ps.stats[value];

            token = COM_Parse(&s);
            index = Q_atoi(token);
            if (index < 0 || index >= cl.csr.end) {
                Com_Error(ERR_DROP, "%s: invalid string index", __func__);
            }

            HUD_DrawCenterString(x + 320 / 2, y, cl.configstrings[index]);
            SCR_DrawHealthBar(x + 320 / 2, y + CHAR_HEIGHT + 4, value & 0xff);
            SCR_DrawHealthBar(x + 320 / 2, y + CHAR_HEIGHT + 12, (value >> 8) & 0xff);
            continue;
        }
    }

    R_ClearColor();
    R_SetAlpha(scr_alpha->value);
}

//=============================================================================

static void SCR_DrawPause(void)
{
    int x, y, w, h;

    if (!sv_paused->integer)
        return;
    if (!cl_paused->integer)
        return;
    if (scr_showpause->integer != 1)
        return;

    R_GetPicSize(&w, &h, scr.pause_pic);
    x = (scr.hud_width - w) / 2;
    y = (scr.hud_height - h) / 2;

    R_DrawPic(x, y, scr.pause_pic);
}

static void SCR_DrawLoading(void)
{
    int x, y, w, h;

    if (!scr.draw_loading)
        return;

    scr.draw_loading = false;

    R_SetScale(scr.hud_scale);

    R_GetPicSize(&w, &h, scr.loading_pic);
    x = (r_config.width * scr.hud_scale - w) / 2;
    y = (r_config.height * scr.hud_scale - h) / 2;

    R_DrawPic(x, y, scr.loading_pic);

    R_SetScale(1.0f);
}

static void SCR_DrawHitMarker(void)
{
    if (!cl.hit_marker_count)
        return;
    if (!scr.hit_marker_pic || scr_hit_marker_time->integer <= 0 ||
        cls.realtime - cl.hit_marker_time > scr_hit_marker_time->integer) {
        cl.hit_marker_count = 0;
        return;
    }

    float frac = (float)(cls.realtime - cl.hit_marker_time) / scr_hit_marker_time->integer;
    float alpha = 1.0f - (frac * frac);

    int x = (scr.hud_width - scr.hit_marker_width) / 2;
    int y = (scr.hud_height - scr.hit_marker_height) / 2;

    R_SetColor(MakeColor(255, 0, 0, alpha * 255));

    R_DrawStretchPic(x + ch_x->integer,
                     y + ch_y->integer,
                     scr.hit_marker_width,
                     scr.hit_marker_height,
                     scr.hit_marker_pic);
}

static void SCR_DrawCrosshair(void)
{
    int x, y;

    if (!scr_crosshair->integer)
        return;
    if (cl.frame.ps.stats[STAT_LAYOUTS] & (LAYOUTS_HIDE_HUD | LAYOUTS_HIDE_CROSSHAIR))
        return;

    x = (scr.hud_width - scr.crosshair_width) / 2;
    y = (scr.hud_height - scr.crosshair_height) / 2;

    R_SetColor(scr.crosshair_color.u32);

    R_DrawStretchPic(x + ch_x->integer,
                     y + ch_y->integer,
                     scr.crosshair_width,
                     scr.crosshair_height,
                     scr.crosshair_pic);

    SCR_DrawHitMarker();
}

// The status bar is a small layout program that is based on the stats array
static void SCR_DrawStats(void)
{
    if (scr_draw2d->integer <= 1)
        return;
    if (cl.frame.ps.stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD)
        return;

    SCR_ExecuteLayoutString(cl.configstrings[CS_STATUSBAR]);
}

static void SCR_DrawLayout(void)
{
    if (scr_draw2d->integer == 3 && !Key_IsDown(K_F1))
        return;     // turn off for GTV

    if (cls.demo.playback && Key_IsDown(K_F1))
        goto draw;

    if (!(cl.frame.ps.stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT))
        return;

draw:
  SCR_ExecuteLayoutString(cl.layout);
}

//
// q2jump strafe_helper
//
static void SCR_DrawStrafeHelper(void) {
  const struct StrafeHelperParams params = {
      .center = cl_strafeHelperCenter->integer,
      .center_marker = cl_strafeHelperCenterMarker->integer,
      .scale = cl_strafeHelperScale->value,
      .height = cl_strafeHelperHeight->value,
      .y = cl_strafeHelperY->value,
  };
  StrafeHelper_Draw(&params, scr.hud_width, scr.hud_height);
}

static void SCR_Draw2D(void)
{
    if (scr_draw2d->integer <= 0)
        return;     // turn off for screenshots

    if (cls.key_dest & KEY_MENU)
        return;

    R_SetScale(scr.hud_scale);

    scr.hud_height = Q_rint(scr.hud_height * scr.hud_scale);
    scr.hud_width = Q_rint(scr.hud_width * scr.hud_scale);

    // crosshair has its own color and alpha
    SCR_DrawCrosshair();
    //
    // q2jump strafe_helper
    //
    if (cl_drawStrafeHelper->integer) {
      SCR_DrawStrafeHelper();
    }

    // the rest of 2D elements share common alpha
    R_ClearColor();
    R_SetAlpha(Cvar_ClampValue(scr_alpha, 0, 1));

    if (scr_timegraph->integer)
        SCR_DebugGraph(cls.frametime * 300, 0xdc);

    if (scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer)
        SCR_DrawDebugGraph();

    SCR_DrawStats();

    SCR_DrawLayout();

    SCR_DrawInventory();

    SCR_DrawCenterString();

    SCR_DrawNet();

    SCR_DrawObjects();

    SCR_DrawChatHUD();

    SCR_DrawTurtle();

    SCR_DrawPause();

    // debug stats have no alpha
    R_ClearColor();

#if USE_DEBUG
    SCR_DrawDebugStats();
    SCR_DrawDebugPmove();
#endif

    R_SetScale(1.0f);
}

static void SCR_DrawActive(void)
{
    // if full screen menu is up, do nothing at all
    if (!UI_IsTransparent())
        return;

    // draw black background if not active
    if (cls.state < ca_active) {
        R_DrawFill8(0, 0, r_config.width, r_config.height, 0);
        return;
    }

    if (cls.state == ca_cinematic) {
        SCR_DrawCinematic();
        return;
    }

    // start with full screen HUD
    scr.hud_height = r_config.height;
    scr.hud_width = r_config.width;

    SCR_DrawDemo();

    SCR_CalcVrect();

    // clear any dirty part of the background
    SCR_TileClear();

    // draw 3D game view
    V_RenderView();

    // draw all 2D elements
    SCR_Draw2D();
}

//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen(void)
{
    static int recursive;

    if (!scr.initialized) {
        return;             // not initialized yet
    }

    // if the screen is disabled (loading plaque is up), do nothing at all
    if (cls.disable_screen) {
        unsigned delta = Sys_Milliseconds() - cls.disable_screen;

        if (delta < 120 * 1000) {
            return;
        }

        cls.disable_screen = 0;
        Com_Printf("Loading plaque timed out.\n");
    }

    if (recursive > 1) {
        Com_Error(ERR_FATAL, "%s: recursively called", __func__);
    }

    recursive++;

    R_BeginFrame();

    // do 3D refresh drawing
    SCR_DrawActive();

    // draw main menu
    UI_Draw(cls.realtime);

    // draw console
    Con_DrawConsole();

    // draw loading plaque
    SCR_DrawLoading();

    R_EndFrame();

    recursive--;
}
