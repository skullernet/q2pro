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

#include "gl.h"
#include "shared/list.h"
#include <assert.h>

#define MAX_DEBUG_LINES     TESS_MAX_VERTICES
#define MAX_DEBUG_TEXTS     1024

typedef struct {
    list_t          entry;
    vec3_t          start, end;
    uint32_t        color;
    uint32_t        time;
    glStateBits_t   bits;
} debug_line_t;

static debug_line_t debug_lines[MAX_DEBUG_LINES];
static list_t debug_lines_free;
static list_t debug_lines_active;

typedef struct {
    list_t          entry;
    vec3_t          origin, angles;
    float           size;
    uint32_t        color;
    uint32_t        time;
    glStateBits_t   bits;
    char            text[128];
} debug_text_t;

static debug_text_t debug_texts[MAX_DEBUG_TEXTS];
static list_t debug_texts_free;
static list_t debug_texts_active;

static cvar_t *gl_debug_linewidth;
static cvar_t *gl_debug_distfrac;

void R_ClearDebugLines(void)
{
    List_Init(&debug_lines_free);
    List_Init(&debug_lines_active);

    List_Init(&debug_texts_free);
    List_Init(&debug_texts_active);
}

void R_AddDebugLine(const vec3_t start, const vec3_t end, uint32_t color, uint32_t time, qboolean depth_test)
{
    debug_line_t *l = LIST_FIRST(debug_line_t, &debug_lines_free, entry);

    if (LIST_EMPTY(&debug_lines_free)) {
        if (LIST_EMPTY(&debug_lines_active)) {
            for (int i = 0; i < MAX_DEBUG_LINES; i++)
                List_Append(&debug_lines_free, &debug_lines[i].entry);
        } else {
            debug_line_t *next;
            LIST_FOR_EACH_SAFE(debug_line_t, l, next, &debug_lines_active, entry) {
                if (l->time <= com_localTime2) {
                    List_Remove(&l->entry);
                    List_Insert(&debug_lines_free, &l->entry);
                }
            }
        }

        if (LIST_EMPTY(&debug_lines_free))
            l = LIST_FIRST(debug_line_t, &debug_lines_active, entry);
        else
            l = LIST_FIRST(debug_line_t, &debug_lines_free, entry);
    }

    // unlink from freelist
    List_Remove(&l->entry);
    List_Append(&debug_lines_active, &l->entry);

    VectorCopy(start, l->start);
    VectorCopy(end, l->end);
    l->color = color;
    l->time = com_localTime2 + time;
    if (l->time < com_localTime2)
        l->time = UINT32_MAX;
    l->bits = GLS_DEPTHMASK_FALSE | GLS_BLEND_BLEND;
    if (!depth_test)
        l->bits |= GLS_DEPTHTEST_DISABLE;
}

#define GL_DRAWLINE(sx, sy, sz, ex, ey, ez) \
    R_AddDebugLine((const vec3_t) { (sx), (sy), (sz) }, (const vec3_t) { (ex), (ey), (ez) }, color, time, depth_test)

#define GL_DRAWLINEV(s, e) \
    R_AddDebugLine(s, e, color, time, depth_test)

void R_AddDebugPoint(const vec3_t point, float size, uint32_t color, uint32_t time, qboolean depth_test)
{
    size *= 0.5f;
    GL_DRAWLINE(point[0] - size, point[1], point[2], point[0] + size, point[1], point[2]);
    GL_DRAWLINE(point[0], point[1] - size, point[2], point[0], point[1] + size, point[2]);
    GL_DRAWLINE(point[0], point[1], point[2] - size, point[0], point[1], point[2] + size);
}

void R_AddDebugAxis(const vec3_t origin, const vec3_t angles, float size, uint32_t time, qboolean depth_test)
{
    vec3_t axis[3], end;
    uint32_t color;

    if (angles) {
        AnglesToAxis(angles, axis);
    } else {
        VectorSet(axis[0], 1, 0, 0);
        VectorSet(axis[1], 0, 1, 0);
        VectorSet(axis[2], 0, 0, 1);
    }

    color = U32_RED;
    VectorMA(origin, size, axis[0], end);
    GL_DRAWLINEV(origin, end);

    color = U32_GREEN;
    VectorMA(origin, size, axis[1], end);
    GL_DRAWLINEV(origin, end);

    color = U32_BLUE;
    VectorMA(origin, size, axis[2], end);
    GL_DRAWLINEV(origin, end);
}

void R_AddDebugBounds(const vec3_t mins, const vec3_t maxs, uint32_t color, uint32_t time, qboolean depth_test)
{
    for (int i = 0; i < 4; i++) {
        // draw column
        float x = ((i > 1) ? mins : maxs)[0];
        float y = ((((i + 1) % 4) > 1) ? mins : maxs)[1];
        GL_DRAWLINE(x, y, mins[2], x, y, maxs[2]);

        // draw bottom & top
        int n = (i + 1) % 4;
        float x2 = ((n > 1) ? mins : maxs)[0];
        float y2 = ((((n + 1) % 4) > 1) ? mins : maxs)[1];
        GL_DRAWLINE(x, y, mins[2], x2, y2, mins[2]);
        GL_DRAWLINE(x, y, maxs[2], x2, y2, maxs[2]);
    }
}

// https://danielsieger.com/blog/2021/03/27/generating-spheres.html
void R_AddDebugSphere(const vec3_t origin, float radius, uint32_t color, uint32_t time, qboolean depth_test)
{
    vec3_t verts[160];
    const int n_stacks = min(4 + radius / 32, 10);
    const int n_slices = min(6 + radius / 32, 16);
    const int v0 = 0;
    int v1 = 1;

    for (int i = 0; i < n_stacks - 1; i++) {
        float phi = M_PIf * (i + 1) / n_stacks;
        for (int j = 0; j < n_slices; j++) {
            float theta = 2 * M_PIf * j / n_slices;
            vec3_t v = {
                sinf(phi) * cosf(theta),
                sinf(phi) * sinf(theta),
                cosf(phi)
            };
            VectorMA(origin, radius, v, verts[v1]);
            v1++;
        }
    }

    VectorCopy(origin, verts[v0]);
    VectorCopy(origin, verts[v1]);

    verts[v0][2] += radius;
    verts[v1][2] -= radius;

    for (int i = 0; i < n_slices; i++) {
        int i0 = i + 1;
        int i1 = (i + 1) % n_slices + 1;
        GL_DRAWLINEV(verts[v0], verts[i1]);
        GL_DRAWLINEV(verts[i1], verts[i0]);
        GL_DRAWLINEV(verts[i0], verts[v0]);
        i0 = i + n_slices * (n_stacks - 2) + 1;
        i1 = (i + 1) % n_slices + n_slices * (n_stacks - 2) + 1;
        GL_DRAWLINEV(verts[v1], verts[i0]);
        GL_DRAWLINEV(verts[i0], verts[i1]);
        GL_DRAWLINEV(verts[i1], verts[v1]);
    }

    for (int j = 0; j < n_stacks - 2; j++) {
        int j0 = j * n_slices + 1;
        int j1 = (j + 1) * n_slices + 1;
        for (int i = 0; i < n_slices; i++) {
            int i0 = j0 + i;
            int i1 = j0 + (i + 1) % n_slices;
            int i2 = j1 + (i + 1) % n_slices;
            int i3 = j1 + i;
            GL_DRAWLINEV(verts[i0], verts[i1]);
            GL_DRAWLINEV(verts[i1], verts[i2]);
            GL_DRAWLINEV(verts[i2], verts[i3]);
            GL_DRAWLINEV(verts[i3], verts[i0]);
        }
    }
}

void R_AddDebugCircle(const vec3_t origin, float radius, uint32_t color, uint32_t time, qboolean depth_test)
{
    int vert_count = min(5 + radius / 8, 16);
    float rads = (2 * M_PIf) / vert_count;

    for (int i = 0; i < vert_count; i++) {
        float a = i * rads;
        float c = cosf(a);
        float s = sinf(a);
        float x = c * radius + origin[0];
        float y = s * radius + origin[1];

        a = ((i + 1) % vert_count) * rads;
        c = cosf(a);
        s = sinf(a);
        float x2 = c * radius + origin[0];
        float y2 = s * radius + origin[1];

        GL_DRAWLINE(x, y, origin[2], x2, y2, origin[2]);
    }
}

void R_AddDebugCylinder(const vec3_t origin, float half_height, float radius, uint32_t color, uint32_t time, qboolean depth_test)
{
    int vert_count = min(5 + radius / 8, 16);
    float rads = (2 * M_PIf) / vert_count;

    for (int i = 0; i < vert_count; i++) {
        float a = i * rads;
        float c = cosf(a);
        float s = sinf(a);
        float x = c * radius + origin[0];
        float y = s * radius + origin[1];

        a = ((i + 1) % vert_count) * rads;
        c = cosf(a);
        s = sinf(a);
        float x2 = c * radius + origin[0];
        float y2 = s * radius + origin[1];

        GL_DRAWLINE(x, y, origin[2] - half_height, x2, y2, origin[2] - half_height);
        GL_DRAWLINE(x, y, origin[2] + half_height, x2, y2, origin[2] + half_height);
        GL_DRAWLINE(x, y, origin[2] - half_height, x,  y,  origin[2] + half_height);
    }
}

void R_DrawArrowCap(const vec3_t apex, const vec3_t dir, float size,
                    uint32_t color, uint32_t time, qboolean depth_test)
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

void R_AddDebugArrow(const vec3_t start, const vec3_t end, float size, uint32_t line_color,
                     uint32_t arrow_color, uint32_t time, qboolean depth_test)
{
    vec3_t dir;
    VectorSubtract(end, start, dir);
    float len = VectorNormalize(dir);

    if (len > size) {
        vec3_t line_end;
        VectorMA(start, len - size, dir, line_end);
        R_AddDebugLine(start, line_end, line_color, time, depth_test);
        R_DrawArrowCap(line_end, dir, size, arrow_color, time, depth_test);
    } else {
        R_DrawArrowCap(end, dir, len, arrow_color, time, depth_test);
    }
}

void R_AddDebugCurveArrow(const vec3_t start, const vec3_t ctrl, const vec3_t end, float size,
                          uint32_t line_color, uint32_t arrow_color, uint32_t time, qboolean depth_test)
{
    int num_points = Q_clip(Distance(start, end) / 32, 3, 24);
    vec3_t last_point;

    for (int i = 0; i <= num_points; i++) {
        float t = i / (float)num_points;
        float it = 1.0f - t;

        float a = it * it;
        float b = 2.0f * t * it;
        float c = t * t;

        vec3_t p = {
            a * start[0] + b * ctrl[0] + c * end[0],
            a * start[1] + b * ctrl[1] + c * end[1],
            a * start[2] + b * ctrl[2] + c * end[2]
        };

        if (i == num_points)
            R_AddDebugArrow(last_point, p, size, line_color, arrow_color, time, depth_test);
        else if (i)
            R_AddDebugLine(last_point, p, line_color, time, depth_test);

        VectorCopy(p, last_point);
    }
}

static void R_AddDebugTextInternal(const vec3_t origin, const vec3_t angles, const char *text,
                                   size_t len, float size, uint32_t color, uint32_t time,
                                   qboolean depth_test)
{
    if (!len)
        return;

    debug_text_t *t = LIST_FIRST(debug_text_t, &debug_texts_free, entry);

    if (LIST_EMPTY(&debug_texts_free)) {
        if (LIST_EMPTY(&debug_texts_active)) {
            for (int i = 0; i < MAX_DEBUG_TEXTS; i++)
                List_Append(&debug_texts_free, &debug_texts[i].entry);
        } else {
            debug_text_t *next;
            LIST_FOR_EACH_SAFE(debug_text_t, t, next, &debug_texts_active, entry) {
                if (t->time <= com_localTime2) {
                    List_Remove(&t->entry);
                    List_Insert(&debug_texts_free, &t->entry);
                }
            }
        }

        if (LIST_EMPTY(&debug_texts_free))
            t = LIST_FIRST(debug_text_t, &debug_texts_active, entry);
        else
            t = LIST_FIRST(debug_text_t, &debug_texts_free, entry);
    }

    // unlink from freelist
    List_Remove(&t->entry);
    List_Append(&debug_texts_active, &t->entry);

    VectorCopy(origin, t->origin);
    if (angles)
        VectorCopy(angles, t->angles);
    t->size = size;
    t->color = color;
    t->time = com_localTime2 + time;
    if (t->time < com_localTime2)
        t->time = UINT32_MAX;
    t->bits = GLS_DEPTHMASK_FALSE | GLS_BLEND_BLEND;
    if (!depth_test)
        t->bits |= GLS_DEPTHTEST_DISABLE;
    if (angles)
        t->bits |= GLS_CULL_DISABLE;
    len = min(len, sizeof(t->text) - 1);
    memcpy(t->text, text, len);
    t->text[len] = 0;
}

void R_AddDebugText(const vec3_t origin, const vec3_t angles, const char *text,
                    float size, uint32_t color, uint32_t time, qboolean depth_test)
{
    vec3_t down, pos, up;
    const char *s, *p;

    if (!angles) {
        R_AddDebugTextInternal(origin, angles, text, strlen(text), size, color, time, depth_test);
        return;
    }

    AngleVectors(angles, NULL, NULL, up);
    VectorScale(up, -size, down);

    VectorCopy(origin, pos);

    // break oriented text into lines to allow for longer text
    s = text;
    while (*s) {
        p = strchr(s, '\n');
        if (!p) {
            R_AddDebugTextInternal(pos, angles, s, strlen(s), size, color, time, depth_test);
            break;
        }
        R_AddDebugTextInternal(pos, angles, s, p - s, size, color, time, depth_test);
        VectorAdd(pos, down, pos);
        s = p + 1;
    }
}

static void GL_DrawDebugLines(void)
{
    glStateBits_t bits = -1;
    debug_line_t *l, *next;
    GLfloat *dst_vert;
    int numverts;

    if (LIST_EMPTY(&debug_lines_active))
        return;

    GL_LoadMatrix(glr.viewmatrix);
    GL_LoadUniforms();
    GL_BindTexture(TMU_TEXTURE, TEXNUM_WHITE);
    GL_BindArrays(VA_NULLMODEL);
    GL_ArrayBits(GLA_VERTEX | GLA_COLOR);

    if (qglLineWidth)
        qglLineWidth(gl_debug_linewidth->value);

    if (gl_config.caps & QGL_CAP_LINE_SMOOTH)
        qglEnable(GL_LINE_SMOOTH);

    static_assert(q_countof(debug_lines) <= q_countof(tess.vertices) / 8, "Too many debug lines");

    dst_vert = tess.vertices;
    numverts = 0;
    LIST_FOR_EACH_SAFE(debug_line_t, l, next, &debug_lines_active, entry) {
        if (l->time < com_localTime2) { // expired
            List_Remove(&l->entry);
            List_Insert(&debug_lines_free, &l->entry);
            continue;
        }

        if (bits != l->bits) {
            if (numverts) {
                GL_LockArrays(numverts);
                qglDrawArrays(GL_LINES, 0, numverts);
                GL_UnlockArrays();
            }

            GL_StateBits(l->bits);
            bits = l->bits;

            dst_vert = tess.vertices;
            numverts = 0;
        }

        VectorCopy(l->start, dst_vert);
        VectorCopy(l->end, dst_vert + 4);
        WN32(dst_vert + 3, l->color);
        WN32(dst_vert + 7, l->color);
        dst_vert += 8;

        numverts += 2;
    }

    if (numverts) {
        GL_LockArrays(numverts);
        qglDrawArrays(GL_LINES, 0, numverts);
        GL_UnlockArrays();
    }

    if (gl_config.caps & QGL_CAP_LINE_SMOOTH)
        qglDisable(GL_LINE_SMOOTH);

    if (qglLineWidth)
        qglLineWidth(1.0f);
}

static void GL_FlushDebugChars(void)
{
    if (!tess.numindices)
        return;

    GL_BindTexture(TMU_TEXTURE, IMG_ForHandle(r_charset)->texnum);
    GL_StateBits(tess.flags);
    GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);
    GL_DrawIndexed(SHOWTRIS_NONE);

    tess.numverts = tess.numindices = 0;
    tess.flags = 0;
}

static void GL_DrawDebugChar(const vec3_t pos, const vec3_t right, const vec3_t down,
                             glStateBits_t bits, uint32_t color, int c)
{
    GLfloat *dst_vert;
    glIndex_t *dst_indices;
    float s, t;

    if ((c & 127) == 32)
        return;

    if (q_unlikely(tess.numverts + 4 > TESS_MAX_VERTICES ||
                   tess.numindices + 6 > TESS_MAX_INDICES) ||
        (tess.numindices && bits != tess.flags))
        GL_FlushDebugChars();

    dst_vert = tess.vertices + tess.numverts * 6;

    VectorCopy(pos, dst_vert);
    VectorAdd(dst_vert, right, dst_vert + 6);
    VectorAdd(dst_vert + 6, down, dst_vert + 12);
    VectorAdd(dst_vert, down, dst_vert + 18);

    s = (c & 15) * 0.0625f;
    t = (c >> 4) * 0.0625f;

    dst_vert[ 3] = s;
    dst_vert[ 4] = t;
    dst_vert[ 9] = s + 0.0625f;
    dst_vert[10] = t;
    dst_vert[15] = s + 0.0625f;
    dst_vert[16] = t + 0.0625f;
    dst_vert[21] = s;
    dst_vert[22] = t + 0.0625f;

    WN32(dst_vert +  5, color);
    WN32(dst_vert + 11, color);
    WN32(dst_vert + 17, color);
    WN32(dst_vert + 23, color);

    dst_indices = tess.indices + tess.numindices;
    dst_indices[0] = tess.numverts + 0;
    dst_indices[1] = tess.numverts + 2;
    dst_indices[2] = tess.numverts + 3;
    dst_indices[3] = tess.numverts + 0;
    dst_indices[4] = tess.numverts + 1;
    dst_indices[5] = tess.numverts + 2;

    tess.numverts += 4;
    tess.numindices += 6;
    tess.flags = bits;
}

static void GL_DrawDebugTextLine(const vec3_t origin, const vec3_t right, const vec3_t down,
                                 const debug_text_t *text, const char *s, size_t len)
{
    // frustum cull
    float radius = text->size * 0.5f * len;
    for (int i = 0; i < 4; i++)
        if (PlaneDiff(origin, &glr.frustumPlanes[i]) < -radius)
            return;

    // draw it
    vec3_t pos;
    VectorMA(origin, -0.5f * len, right, pos);
    while (*s && len--) {
        byte c = *s++;
        GL_DrawDebugChar(pos, right, down, text->bits, text->color, c);
        VectorAdd(pos, right, pos);
    }
}

static void GL_DrawDebugTexts(void)
{
    debug_text_t *text, *next;

    if (LIST_EMPTY(&debug_texts_active))
        return;

    GL_LoadMatrix(glr.viewmatrix);
    GL_BindArrays(VA_EFFECT);

    LIST_FOR_EACH_SAFE(debug_text_t, text, next, &debug_texts_active, entry) {
        vec3_t right, down, pos;
        const char *s, *p;

        if (text->time < com_localTime2) { // expired
            List_Remove(&text->entry);
            List_Insert(&debug_texts_free, &text->entry);
            continue;
        }

        // distance cull
        VectorSubtract(text->origin, glr.fd.vieworg, pos);
        if (text->size < DotProduct(pos, glr.viewaxis[0]) * gl_debug_distfrac->value)
            continue;

        if (text->bits & GLS_CULL_DISABLE) { // oriented
            vec3_t up;
            AngleVectors(text->angles, NULL, right, up);
            VectorScale(right, text->size, right);
            VectorScale(up,   -text->size, down);
        } else {
            VectorScale(glr.viewaxis[1], -text->size, right);
            VectorScale(glr.viewaxis[2], -text->size, down);
        }
        VectorCopy(text->origin, pos);

        s = text->text;
        while (*s) {
            p = strchr(s, '\n');
            if (!p) {
                GL_DrawDebugTextLine(pos, right, down, text, s, strlen(s));
                break;
            }
            GL_DrawDebugTextLine(pos, right, down, text, s, p - s);
            VectorAdd(pos, down, pos);
            s = p + 1;
        }
    }

    GL_FlushDebugChars();
}

void GL_DrawDebugObjects(void)
{
    GL_DrawDebugLines();
    GL_DrawDebugTexts();
}

void GL_InitDebugDraw(void)
{
    R_ClearDebugLines();

    gl_debug_linewidth = Cvar_Get("gl_debug_linewidth", "2", 0);
    gl_debug_distfrac = Cvar_Get("gl_debug_distfrac", "0.004", 0);

    Cmd_AddCommand("cleardebuglines", R_ClearDebugLines);
}

void GL_ShutdownDebugDraw(void)
{
    Cmd_RemoveCommand("cleardebuglines");
}
