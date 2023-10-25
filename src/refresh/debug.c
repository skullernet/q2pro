/*
Copyright (C) 2003-2006 Andrey Nazarov

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

#include "gl.h"
#include "client/client.h"
#include "common/prompt.h"
#include "debug_fonts/cursive.h"
#include "debug_fonts/futural.h"
#include "debug_fonts/futuram.h"
#include "debug_fonts/gothgbt.h"
#include "debug_fonts/gothgrt.h"
#include "debug_fonts/gothiceng.h"
#include "debug_fonts/gothicger.h"
#include "debug_fonts/gothicita.h"
#include "debug_fonts/gothitt.h"
#include "debug_fonts/rowmand.h"
#include "debug_fonts/rowmans.h"
#include "debug_fonts/rowmant.h"
#include "debug_fonts/scriptc.h"
#include "debug_fonts/scripts.h"
#include "debug_fonts/timesi.h"
#include "debug_fonts/timesib.h"
#include "debug_fonts/timesr.h"
#include "debug_fonts/timesrb.h"

typedef struct debug_font_s {
    // Number of glyphs
    int count;
    // Font height
    char height;
    // Widths of the glyphs
    const char* width;
    // Real widths of the glyphs (calculated from data)
    const char* realwidth;
    // Number of chars in each glyph
    const int* size;
    // Pointers to glyph data
    const char **glyph_data;
} debug_font_t;

#define DEBUG_FONT(NAME)        \
    {                           \
        #NAME,                  \
        {                       \
            NAME##_count,       \
            NAME##_height,      \
            NAME##_width,       \
            NAME##_realwidth,   \
            NAME##_size,        \
            NAME                \
        }                       \
    }

static const struct {
    const char *name;
    debug_font_t font;
} debug_fonts[] = {
    DEBUG_FONT(futural),
    DEBUG_FONT(cursive),
    DEBUG_FONT(futuram),
    DEBUG_FONT(gothgbt),
    DEBUG_FONT(gothgrt),
    DEBUG_FONT(gothiceng),
    DEBUG_FONT(gothicger),
    DEBUG_FONT(gothicita),
    DEBUG_FONT(gothitt),
    DEBUG_FONT(rowmand),
    DEBUG_FONT(rowmans),
    DEBUG_FONT(rowmant),
    DEBUG_FONT(scriptc),
    DEBUG_FONT(scripts),
    DEBUG_FONT(timesi),
    DEBUG_FONT(timesib),
    DEBUG_FONT(timesr),
    DEBUG_FONT(timesrb),
};

#undef DEBUG_FONT

static const debug_font_t *dbg_font;

static cvar_t *r_debug_font;
static cvar_t *r_debug_linewidth;

#define MAX_DEBUG_LINES		8192

typedef struct debug_line_s {
    vec3_t		start, end;
    color_t		color;
    int			time; // 0 = one frame only
    bool		depth_test;
    
    list_t      entry;
} debug_line_t;

static debug_line_t debug_lines[MAX_DEBUG_LINES];
static list_t debug_lines_free;
static list_t debug_lines_active;

#define DEBUG_FIRST(list)      LIST_FIRST(debug_line_t, list, entry)
#define DEBUG_TERM(l, list)   LIST_TERM(l, list, entry)

void GL_ClearDebugLines(void)
{
    memset(debug_lines, 0, sizeof(debug_lines));

    List_Init(&debug_lines_free);
    List_Init(&debug_lines_active);

    for (int i = 0; i < MAX_DEBUG_LINES; i++)
        List_Append(&debug_lines_free, &debug_lines[i].entry);
}

static int R_GetTime(void)
{
    return CL_ServerTime();
}

void R_AddDebugLine(const vec3_t start, const vec3_t end, color_t color, int time, bool depth_test)
{
    debug_line_t *l = DEBUG_FIRST(&debug_lines_free);

    if (DEBUG_TERM(l, &debug_lines_free)) {
        debug_line_t *nl, *next;
        bool found = false;

        LIST_FOR_EACH_SAFE(debug_line_t, nl, next, &debug_lines_active, entry) {
            
            if (nl->time <= R_GetTime()) {
                List_Remove(&nl->entry);
                List_Insert(&debug_lines_free, &nl->entry);
                found = true;
            }
        }

        if (!found) {
            l = DEBUG_FIRST(&debug_lines_active);
        } else {
            l = DEBUG_FIRST(&debug_lines_free);
        }
    }

    // unlink from freelist
    List_Remove(&l->entry);

    List_Append(&debug_lines_active, &l->entry);

    VectorCopy(start, l->start);
    VectorCopy(end, l->end);
    l->color = color;
    l->time = R_GetTime() + time;
    l->depth_test = depth_test;
}

#define GL_DRAWLINE(sx, sy, sz, ex, ey, ez) \
    R_AddDebugLine((const vec3_t) { (sx), (sy), (sz) }, (const vec3_t) { (ex), (ey), (ez) }, color, time, depth_test)
#define GL_DRAWLINEV(s, e) \
    GL_DRAWLINE((s)[0], (s)[1], (s)[2], (e)[0], (e)[1], (e)[2])

void R_AddDebugPoint(const vec3_t point, float size, color_t color, int time, bool depth_test)
{
    size *= 0.5f;
    
    GL_DRAWLINE(point[0] - size, point[1], point[2], point[0] + size, point[1], point[2]);
    GL_DRAWLINE(point[0], point[1] - size, point[2], point[0], point[1] + size, point[2]);
    GL_DRAWLINE(point[0], point[1], point[2] - size, point[0], point[1], point[2] + size);
}

void R_AddDebugAxis(const vec3_t point, float size, int time, bool depth_test)
{
    color_t color = { .u32 = U32_RED };
    GL_DRAWLINE(point[0], point[1], point[2], point[0] + size, point[1], point[2]);
    color = (color_t) { .u32 = U32_GREEN };
    GL_DRAWLINE(point[0], point[1], point[2], point[0], point[1] + size, point[2]);
    color = (color_t) { .u32 = U32_BLUE };
    GL_DRAWLINE(point[0], point[1], point[2], point[0], point[1], point[2] + size);
}

void R_AddDebugBounds(const vec3_t absmins, const vec3_t absmaxs, color_t color, int time, bool depth_test)
{
    for (int i = 0; i < 4; i++) {
        // draw column
        float x = ((i > 1) ? absmins : absmaxs)[0];
        float y = ((((i + 1) % 4) > 1) ? absmins : absmaxs)[1];
        GL_DRAWLINE(x, y, absmins[2], x, y, absmaxs[2]);

        // draw bottom & top
        int n = (i + 1) % 4;
        float x2 = ((n > 1) ? absmins : absmaxs)[0];
        float y2 = ((((n + 1) % 4) > 1) ? absmins : absmaxs)[1];
            
        GL_DRAWLINE(x, y, absmins[2], x2, y2, absmins[2]);
        GL_DRAWLINE(x, y, absmaxs[2], x2, y2, absmaxs[2]);
    }
}

typedef struct { int a, b; } line_t;

void R_AddDebugSphere(const vec3_t origin, float radius, color_t color, int time, bool depth_test)
{
    // https://danielsieger.com/blog/2021/03/27/generating-spheres.html
    radius *= 0.5f;

    static const vec3_t verts[20] = { // precalculated verts generated from 6 slices + 4 stacks
        { 0.00000000f,    0.00000000f,     1.00000000f },
        { 0.707106769f,   0.00000000f,     0.707106769f },
        { 0.353553385f,   0.612372458f,    0.707106769f },
        { -0.353553444f,  0.612372458f,    0.707106769f },
        { -0.707106769f, -6.18172393e-08f, 0.707106769f },
        { -0.353553325f, -0.612372518f,    0.707106769f },
        { 0.353553355f,  -0.612372458f,    0.707106769f },
        { 1.00000000f,   0.00000000f,      -4.37113883e-08f },
        { 0.499999970f,  0.866025448f,     -4.37113883e-08f },
        { -0.500000060f, 0.866025388f,     -4.37113883e-08f },
        { -1.00000000f,  -8.74227766e-08f, -4.37113883e-08f },
        { -0.499999911f, -0.866025448f,    -4.37113883e-08f },
        { 0.499999911f,  -0.866025448f,    -4.37113883e-08f },
        { 0.707106769f,  0.00000000f,      -0.707106769f },
        { 0.353553385f,  0.612372458f,     -0.707106769f },
        { -0.353553414f, 0.612372398f,     -0.707106769f },
        { -0.707106769f, -6.18172393e-08f, -0.707106769f },
        { -0.353553325f, -0.612372458f,    -0.707106769f },
        { 0.353553325f,  -0.612372458f,    -0.707106769f },
        { 0.00000000f,   0.00000000f,      -1.00000000f }
    };
    static const struct { int a, b; } lines[] = { // precalculated lines
        { 0, 1 },
        { 0, 2 },
        { 0, 3 },
        { 0, 4 },
        { 0, 5 },
        { 0, 6 },
        { 1, 2 },
        { 1, 6 },
        { 1, 7 },
        { 2, 3 },
        { 2, 8 },
        { 3, 4 },
        { 3, 9 },
        { 4, 5 },
        { 4, 10 },
        { 5, 6 },
        { 5, 11 },
        { 6, 12 },
        { 7, 8 },
        { 7, 12 },
        { 7, 13 },
        { 8, 9 },
        { 8, 14 },
        { 9, 10 },
        { 9, 15 },
        { 10, 11 },
        { 10, 16 },
        { 11, 12 },
        { 11, 17 },
        { 12, 18 },
        { 13, 14 },
        { 13, 18 },
        { 13, 19 },
        { 14, 15 },
        { 14, 19 },
        { 15, 16 },
        { 15, 19 },
        { 16, 17 },
        { 16, 19 },
        { 17, 18 },
        { 17, 19 },
        { 18, 19 }
    };

    for (int i = 0; i < q_countof(lines); i++) {
        GL_DRAWLINE(
            (verts[lines[i].a][0] * radius) + origin[0],
            (verts[lines[i].a][1] * radius) + origin[1],
            (verts[lines[i].a][2] * radius) + origin[2],
            (verts[lines[i].b][0] * radius) + origin[0],
            (verts[lines[i].b][1] * radius) + origin[1],
            (verts[lines[i].b][2] * radius) + origin[2]);
    }
}

void R_AddDebugCylinder(const vec3_t origin, float half_height, float radius, color_t color, int time, bool depth_test)
{
    radius *= 0.5f;

    int vert_count = (int) min(5 + (radius / 16.f), 16.0f);
    float rads = DEG2RAD(360.0f / vert_count);

    float irad = -radius;

    for (int i = 0; i < vert_count; i++) {
        float a = i * rads;
        float c = cos(a);
        float s = sin(a);
        float x = (c * irad - s) + origin[0];
        float y = (c + s * irad) + origin[1];

        float a2 = ((i + 1) % vert_count) * rads;
        float c2 = cos(a2);
        float s2 = sin(a2);
        float x2 = (c2 * irad - s2) + origin[0];
        float y2 = (c2 + s2 * irad) + origin[1];
        
        GL_DRAWLINE(x, y, origin[2] - half_height, x2, y2, origin[2] - half_height);
        GL_DRAWLINE(x, y, origin[2] + half_height, x2, y2, origin[2] + half_height);
    }
}

static inline void GL_TransformVec3(vec3_t out, const vec3_t *matrix, const vec3_t in)
{
    out[0] = matrix[0][0] * in[0] + matrix[1][0] * in[1] + matrix[2][0] * in[2];
    out[1] = matrix[0][1] * in[0] + matrix[1][1] * in[1] + matrix[2][1] * in[2];
    out[2] = matrix[0][2] * in[0] + matrix[1][2] * in[1] + matrix[2][2] * in[2];
}

static void GL_DrawArrowCap(const vec3_t apex, const vec3_t dir, float size, color_t color, int time, bool depth_test)
{
    vec3_t cap_end;
    VectorMA(apex, size, dir, cap_end);

    R_AddDebugLine(apex, cap_end, color, time, depth_test);

    vec3_t right, up;
    MakeNormalVectors(dir, right, up);

    vec3_t l;
    VectorMA(apex, size, right, l);
    R_AddDebugLine(l, cap_end, color, time, depth_test);

    VectorMA(apex, -size, right, l);
    R_AddDebugLine(l, cap_end, color, time, depth_test);
}

void R_AddDebugArrow(const vec3_t start, const vec3_t end, float size, color_t line_color, color_t arrow_color, int time, bool depth_test)
{
    vec3_t dir;
    VectorSubtract(end, start, dir);
    float len = VectorNormalize(dir);

    if (len > size) {
        vec3_t line_end;
        VectorMA(start, len - size, dir, line_end);
        R_AddDebugLine(start, line_end, line_color, time, depth_test);
        GL_DrawArrowCap(line_end, dir, size, arrow_color, time, depth_test);
    } else {
        GL_DrawArrowCap(end, dir, len, arrow_color, time, depth_test);
    }
}

void R_AddDebugRay(const vec3_t start, const vec3_t dir, float length, float size, color_t line_color, color_t arrow_color, int time, bool depth_test)
{
    if (length > size) {
        vec3_t line_end;
        VectorMA(start, length - size, dir, line_end);

        R_AddDebugLine(start, line_end, line_color, time, depth_test);
        GL_DrawArrowCap(line_end, dir, size, arrow_color, time, depth_test);
    } else {
        GL_DrawArrowCap(start, dir, length, arrow_color, time, depth_test);
    }
}

void R_AddDebugCircle(const vec3_t origin, float radius, color_t color, int time, bool depth_test)
{
    radius *= 0.5f;

    int vert_count = (int) min(5 + (radius / 8.f), 16.0f);
    float rads = DEG2RAD(360.0f / vert_count);

    float irad = -radius;

    for (int i = 0; i < vert_count; i++) {
        float a = i * rads;
        float c = cos(a);
        float s = sin(a);
        float x = (c * irad - s) + origin[0];
        float y = (c + s * irad) + origin[1];

        float a2 = ((i + 1) % vert_count) * rads;
        float c2 = cos(a2);
        float s2 = sin(a2);
        float x2 = (c2 * irad - s2) + origin[0];
        float y2 = (c2 + s2 * irad) + origin[1];

        GL_DRAWLINE(x, y, origin[2], x2, y2, origin[2]);
    }
}

void R_AddDebugText(const vec3_t origin, const char *text, float size, const vec3_t angles, color_t color, int time, bool depth_test)
{
    int total_lines = 1;
    float scale = (1.0f / dbg_font->height) * (size * 32);

    int l = strlen(text);

    for (int i = 0; i < l; i++) {
        if (text[i] == '\n')
            total_lines++;
    }

    if (!angles)
    {
        vec3_t d;
        VectorSubtract(origin, glr.fd.vieworg, d);
        VectorNormalize(d);
        d[2] = 0.0f;
        vectoangles2(d, d);
        angles = (const vec_t *) &d;
    }

    vec3_t right, up;
    AngleVectors(angles, NULL, right, up);

    float y_offset = -((dbg_font->height * scale) * 0.5f) * total_lines;

    const char *c = text;
    for (int line = 0; line < total_lines; line++) {
        const char *c_end = c;
        float width = 0;

        for (; *c_end && *c_end != '\n'; c_end++) {
            width += dbg_font->width[*c_end - ' '] * scale;
        }
        
        float x_offset = (width * 0.5f);

        for (const char *rc = c; rc != c_end; rc++) {
            char c = *rc - ' ';
            const float char_width = dbg_font->width[(int)c] * scale;
            const int char_size = dbg_font->size[(int)c];
            const char *char_data = dbg_font->glyph_data[(int)c];

            for (int i = 0; i < char_size; i += 4) {
                vec3_t s;
                float r = -char_data[i] * scale + x_offset;
                float u = -(char_data[i + 1] * scale + y_offset);
                VectorMA(origin, -r, right, s);
                VectorMA(s, u, up, s);
                vec3_t e;
                r = -char_data[i + 2] * scale + x_offset;
                u = -(char_data[i + 3] * scale + y_offset);
                VectorMA(origin, -r, right, e);
                VectorMA(e, u, up, e);
                GL_DRAWLINEV(s, e);
            }

            x_offset -= char_width;
        }

        y_offset += dbg_font->height * scale;

        c = c_end + 1;
    }
}

void GL_DrawDebugLines(void)
{
    GL_LoadMatrix(glr.viewmatrix);
    GL_BindTexture(0, TEXNUM_WHITE);
    GL_ArrayBits(GLA_VERTEX);
    qglEnable(GL_LINE_SMOOTH);
    qglLineWidth(r_debug_linewidth->value);

    GL_VertexPointer(3, 0, tess.vertices);

    GLfloat *pos_out = tess.vertices;
    tess.numverts = 0;

    int last_bits = -1;
    color_t last_color = { 0 };

    debug_line_t *l, *next;

    LIST_FOR_EACH_SAFE(debug_line_t, l, next, &debug_lines_active, entry) {
        if (l->time < R_GetTime()) {
            // expired
            List_Remove(&l->entry);
            List_Insert(&debug_lines_free, &l->entry);
            continue;
        }

        int bits = GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE;

        if (!l->depth_test) {
            bits |= GLS_DEPTHTEST_DISABLE;
        }

        if (last_bits != bits ||
            last_color.u32 != l->color.u32) {

            if (tess.numverts) {
                GL_LockArrays(tess.numverts);
                qglDrawArrays(GL_LINES, 0, tess.numverts);
                GL_UnlockArrays();
            }
        
            GL_Color(l->color.u8[0] / 255.f, l->color.u8[1] / 255.f, l->color.u8[2] / 255.f, l->color.u8[3] / 255.f);
            last_color = l->color;
            GL_StateBits(bits);
            last_bits = bits;

            pos_out = tess.vertices;
            tess.numverts = 0;
        }
        
        VectorCopy(l->start, pos_out);
        VectorCopy(l->end, pos_out + 3);
        pos_out += 6;

        tess.numverts += 2;
    }

    if (tess.numverts) {
        GL_LockArrays(tess.numverts);
        qglDrawArrays(GL_LINES, 0, tess.numverts);
        GL_UnlockArrays();
    }

    qglLineWidth(1.0f);
    qglDisable(GL_LINE_SMOOTH);
    GL_Color(1.0f, 1.0f, 1.0f, 1.0f);
}

static void r_debug_font_changed(cvar_t* cvar)
{
    int font_idx = -1;
    for (int i = 0; i < q_countof(debug_fonts); i++) {
        if (Q_strcasecmp(cvar->string, debug_fonts[i].name) == 0) {
            font_idx = i;
            break;
        }
    }
    if (font_idx < 0) {
        Com_WPrintf("unknown debug font: %s\n", cvar->string);
        font_idx = 0;
    }
    dbg_font = &debug_fonts[font_idx].font;
}

static void r_debug_font_generator(struct genctx_s *gen)
{
    for (int i = 0; i < q_countof(debug_fonts); i++) {
        Prompt_AddMatch(gen, debug_fonts[i].name);
    }
}

static void GL_DebugClear_f(void)
{
    GL_ClearDebugLines();
}

void GL_InitDebugDraw(void)
{
    GL_ClearDebugLines();

    r_debug_linewidth = Cvar_Get("r_debug_linewidth", "2", 0);
    r_debug_font = Cvar_Get("r_debug_font", debug_fonts[0].name, 0);
    r_debug_font->changed = r_debug_font_changed;
    r_debug_font->generator = r_debug_font_generator;
    r_debug_font_changed(r_debug_font);

    Cmd_AddCommand("debug_clear", GL_DebugClear_f);
}
