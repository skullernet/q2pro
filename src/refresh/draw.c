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

drawStatic_t draw;

static inline void GL_StretchPic_(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2,
    uint32_t color, int texnum, int flags)
{
    vec_t *dst_vert;
    glIndex_t *dst_indices;

    if (tess.numverts + 4 > TESS_MAX_VERTICES ||
        tess.numindices + 6 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[TMU_TEXTURE] != texnum))
        GL_Flush2D();

    tess.texnum[TMU_TEXTURE] = texnum;

    dst_vert = tess.vertices + tess.numverts * 5;
    Vector4Set(dst_vert,      x,     y,     s1, t1);
    Vector4Set(dst_vert +  5, x + w, y,     s2, t1);
    Vector4Set(dst_vert + 10, x + w, y + h, s2, t2);
    Vector4Set(dst_vert + 15, x,     y + h, s1, t2);

    WN32(dst_vert +  4, color);
    WN32(dst_vert +  9, color);
    WN32(dst_vert + 14, color);
    WN32(dst_vert + 19, color);

    dst_indices = tess.indices + tess.numindices;
    dst_indices[0] = tess.numverts + 0;
    dst_indices[1] = tess.numverts + 2;
    dst_indices[2] = tess.numverts + 3;
    dst_indices[3] = tess.numverts + 0;
    dst_indices[4] = tess.numverts + 1;
    dst_indices[5] = tess.numverts + 2;

    if (flags & IF_TRANSPARENT) {
        if ((flags & IF_PALETTED) && draw.scale == 1)
            tess.flags |= GLS_ALPHATEST_ENABLE;
        else
            tess.flags |= GLS_BLEND_BLEND;
    }

    if ((color & U32_ALPHA) != U32_ALPHA)
        tess.flags |= GLS_BLEND_BLEND;

    tess.numverts += 4;
    tess.numindices += 6;
}

#define GL_StretchPic(x,y,w,h,s1,t1,s2,t2,color,image) \
    GL_StretchPic_(x,y,w,h,s1,t1,s2,t2,color,(image)->texnum,(image)->flags)

static void GL_DrawVignette(float frac, color_t outer, color_t inner)
{
    static const byte indices[24] = {
        0, 5, 4, 0, 1, 5, 1, 6, 5, 1, 2, 6, 6, 2, 3, 6, 3, 7, 0, 7, 3, 0, 4, 7
    };
    vec_t *dst_vert;
    glIndex_t *dst_indices;

    if (tess.numverts + 8 > TESS_MAX_VERTICES ||
        tess.numindices + 24 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[TMU_TEXTURE] != TEXNUM_WHITE))
        GL_Flush2D();

    tess.texnum[TMU_TEXTURE] = TEXNUM_WHITE;

    int x = 0, y = 0;
    int w = glr.fd.width, h = glr.fd.height;
    int distance = min(w, h) * frac;

    // outer vertices
    dst_vert = tess.vertices + tess.numverts * 5;
    Vector4Set(dst_vert,      x,     y,     0, 0);
    Vector4Set(dst_vert +  5, x + w, y,     0, 0);
    Vector4Set(dst_vert + 10, x + w, y + h, 0, 0);
    Vector4Set(dst_vert + 15, x,     y + h, 0, 0);

    WN32(dst_vert +  4, outer.u32);
    WN32(dst_vert +  9, outer.u32);
    WN32(dst_vert + 14, outer.u32);
    WN32(dst_vert + 19, outer.u32);

    // inner vertices
    x += distance;
    y += distance;
    w -= distance * 2;
    h -= distance * 2;

    dst_vert += 20;
    Vector4Set(dst_vert,      x,     y,     0, 0);
    Vector4Set(dst_vert +  5, x + w, y,     0, 0);
    Vector4Set(dst_vert + 10, x + w, y + h, 0, 0);
    Vector4Set(dst_vert + 15, x,     y + h, 0, 0);

    WN32(dst_vert +  4, inner.u32);
    WN32(dst_vert +  9, inner.u32);
    WN32(dst_vert + 14, inner.u32);
    WN32(dst_vert + 19, inner.u32);

    /*
    0             1
        4     5

        7     6
    3             2
    */

    dst_indices = tess.indices + tess.numindices;
    for (int i = 0; i < 24; i++)
        dst_indices[i] = tess.numverts + indices[i];

    tess.flags |= GLS_BLEND_BLEND | GLS_SHADE_SMOOTH;

    tess.numverts += 8;
    tess.numindices += 24;
}

void GL_Blend(void)
{
    if (glr.fd.screen_blend[3]) {
        color_t color;

        color.u8[0] = glr.fd.screen_blend[0] * 255;
        color.u8[1] = glr.fd.screen_blend[1] * 255;
        color.u8[2] = glr.fd.screen_blend[2] * 255;
        color.u8[3] = glr.fd.screen_blend[3] * 255;

        GL_StretchPic_(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                       color.u32, TEXNUM_WHITE, 0);
    }

    if (glr.fd.damage_blend[3]) {
        color_t outer, inner;

        outer.u8[0] = glr.fd.damage_blend[0] * 255;
        outer.u8[1] = glr.fd.damage_blend[1] * 255;
        outer.u8[2] = glr.fd.damage_blend[2] * 255;
        outer.u8[3] = glr.fd.damage_blend[3] * 255;

        inner.u32 = outer.u32 & U32_RGB;

        if (gl_damageblend_frac->value > 0)
            GL_DrawVignette(Cvar_ClampValue(gl_damageblend_frac, 0, 0.5f), outer, inner);
        else
            GL_StretchPic_(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                           outer.u32, TEXNUM_WHITE, 0);
    }
}

void R_ClearColor(void)
{
    draw.colors[0].u32 = U32_WHITE;
    draw.colors[1].u32 = U32_WHITE;
}

void R_SetAlpha(float alpha)
{
    draw.colors[0].u8[3] =
    draw.colors[1].u8[3] = alpha * 255;
}

void R_SetColor(uint32_t color)
{
    draw.colors[0].u32 = color;
    draw.colors[1].u8[3] = draw.colors[0].u8[3];
}

void R_SetClipRect(const clipRect_t *clip)
{
    clipRect_t rc;
    float scale;

    GL_Flush2D();

    if (!clip) {
clear:
        if (draw.scissor) {
            qglDisable(GL_SCISSOR_TEST);
            draw.scissor = false;
        }
        return;
    }

    scale = 1 / draw.scale;

    rc.left = clip->left * scale;
    rc.top = clip->top * scale;
    rc.right = clip->right * scale;
    rc.bottom = clip->bottom * scale;

    if (rc.left < 0)
        rc.left = 0;
    if (rc.top < 0)
        rc.top = 0;
    if (rc.right > r_config.width)
        rc.right = r_config.width;
    if (rc.bottom > r_config.height)
        rc.bottom = r_config.height;
    if (rc.right < rc.left)
        goto clear;
    if (rc.bottom < rc.top)
        goto clear;

    qglEnable(GL_SCISSOR_TEST);
    qglScissor(rc.left, r_config.height - rc.bottom,
               rc.right - rc.left, rc.bottom - rc.top);
    draw.scissor = true;
}

static int get_auto_scale(void)
{
    int scale = 1;

    if (r_config.height < r_config.width) {
        if (r_config.height >= 2160)
            scale = 4;
        else if (r_config.height >= 1080)
            scale = 2;
    } else {
        if (r_config.width >= 3840)
            scale = 4;
        else if (r_config.width >= 1920)
            scale = 2;
    }

    if (vid && vid->get_dpi_scale) {
        int min_scale = vid->get_dpi_scale();
        return max(scale, min_scale);
    }

    return scale;
}

float R_ClampScale(cvar_t *var)
{
    if (!var)
        return 1.0f;

    if (var->value)
        return 1.0f / Cvar_ClampValue(var, 1.0f, 10.0f);

    return 1.0f / get_auto_scale();
}

void R_SetScale(float scale)
{
    if (draw.scale == scale)
        return;

    GL_Flush2D();

    GL_Ortho(0, Q_rint(r_config.width * scale),
             Q_rint(r_config.height * scale), 0, -1, 1);

    draw.scale = scale;
}

void R_DrawStretchPic(int x, int y, int w, int h, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, w, h, image->sl, image->tl, image->sh, image->th,
                  draw.colors[0].u32, image);
}

void R_DrawKeepAspectPic(int x, int y, int w, int h, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    if (image->flags & IF_SCRAP) {
        R_DrawStretchPic(x, y, w, h, pic);
        return;
    }

    float scale_w = w;
    float scale_h = h * image->aspect;
    float scale = max(scale_w, scale_h);

    float s = (1.0f - scale_w / scale) * 0.5f;
    float t = (1.0f - scale_h / scale) * 0.5f;

    GL_StretchPic(x, y, w, h, s, t, 1.0f - s, 1.0f - t, draw.colors[0].u32, image);
}

void R_DrawPic(int x, int y, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, image->width, image->height,
                  image->sl, image->tl, image->sh, image->th, draw.colors[0].u32, image);
}

void R_DrawStretchRaw(int x, int y, int w, int h)
{
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, U32_WHITE, TEXNUM_RAW, 0);
}

void R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic)
{
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_RAW);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic_w, pic_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
}

#define DIV64 (1.0f / 64.0f)

void R_TileClear(int x, int y, int w, int h, qhandle_t pic)
{
    GL_StretchPic(x, y, w, h, x * DIV64, y * DIV64,
                  (x + w) * DIV64, (y + h) * DIV64, U32_WHITE, IMG_ForHandle(pic));
}

void R_DrawFill8(int x, int y, int w, int h, int c)
{
    if (!w || !h)
        return;
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, d_8to24table[c & 0xff], TEXNUM_WHITE, 0);
}

void R_DrawFill32(int x, int y, int w, int h, uint32_t color)
{
    if (!w || !h)
        return;
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, color, TEXNUM_WHITE, 0);
}

static inline void draw_char(int x, int y, int flags, int c, const image_t *image)
{
    float s, t;

    if ((c & 127) == 32)
        return;

    if (flags & UI_ALTCOLOR)
        c |= 0x80;

    if (flags & UI_XORCOLOR)
        c ^= 0x80;

    s = (c & 15) * 0.0625f;
    t = (c >> 4) * 0.0625f;

    if (gl_fontshadow->integer > 0 && c != 0x83) {
        uint32_t black = draw.colors[0].u32 & U32_ALPHA;

        GL_StretchPic(x + 1, y + 1, CHAR_WIDTH, CHAR_HEIGHT, s, t,
                      s + 0.0625f, t + 0.0625f, black, image);

        if (gl_fontshadow->integer > 1)
            GL_StretchPic(x + 2, y + 2, CHAR_WIDTH, CHAR_HEIGHT, s, t,
                          s + 0.0625f, t + 0.0625f, black, image);
    }

    GL_StretchPic(x, y, CHAR_WIDTH, CHAR_HEIGHT, s, t,
                  s + 0.0625f, t + 0.0625f, draw.colors[c >> 7].u32, image);
}

void R_DrawChar(int x, int y, int flags, int c, qhandle_t font)
{
    draw_char(x, y, flags, c & 255, IMG_ForHandle(font));
}

int R_DrawString(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font)
{
    const image_t *image = IMG_ForHandle(font);

    while (maxlen-- && *s) {
        byte c = *s++;
        draw_char(x, y, flags, c, image);
        x += CHAR_WIDTH;
    }

    return x;
}

#if USE_DEBUG

qhandle_t r_charset;

static void Draw_Stringf(int x, int y, const char *fmt, ...)
{
    va_list argptr;
    char buffer[MAX_STRING_CHARS];

    va_start(argptr, fmt);
    Q_vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

    R_DrawString(x, y, 0, -1, buffer, r_charset);
}

void Draw_Stats(void)
{
    int x = 10, y = 10;

    R_SetScale(1.0f / get_auto_scale());
    R_DrawFill8(8, 8, 25*8, 23*10+2, 4);

    Draw_Stringf(x, y, "Nodes visible  : %i", glr.nodes_visible); y += 10;
    Draw_Stringf(x, y, "Nodes culled   : %i", c.nodesCulled); y += 10;
    Draw_Stringf(x, y, "Nodes drawn    : %i", c.nodesDrawn); y += 10;
    Draw_Stringf(x, y, "Leaves drawn   : %i", c.leavesDrawn); y += 10;
    Draw_Stringf(x, y, "Faces drawn    : %i", c.facesDrawn); y += 10;
    Draw_Stringf(x, y, "Faces culled   : %i", c.facesCulled); y += 10;
    Draw_Stringf(x, y, "Boxes culled   : %i", c.boxesCulled); y += 10;
    Draw_Stringf(x, y, "Spheres culled : %i", c.spheresCulled); y += 10;
    Draw_Stringf(x, y, "RtBoxes culled : %i", c.rotatedBoxesCulled); y += 10;
    Draw_Stringf(x, y, "Tris drawn     : %i", c.trisDrawn); y += 10;
    Draw_Stringf(x, y, "Tex switches   : %i", c.texSwitches); y += 10;
    Draw_Stringf(x, y, "Tex uploads    : %i", c.texUploads); y += 10;
    Draw_Stringf(x, y, "LM texels      : %i", c.lightTexels); y += 10;
    Draw_Stringf(x, y, "Batches drawn  : %i", c.batchesDrawn); y += 10;
    Draw_Stringf(x, y, "Faces / batch  : %.1f", c.batchesDrawn ? (float)c.facesDrawn / c.batchesDrawn : 0.0f); y += 10;
    Draw_Stringf(x, y, "Tris / batch   : %.1f", c.batchesDrawn ? (float)c.facesTris / c.batchesDrawn : 0.0f); y += 10;
    Draw_Stringf(x, y, "2D batches     : %i", c.batchesDrawn2D); y += 10;
    Draw_Stringf(x, y, "Total entities : %i", glr.fd.num_entities); y += 10;
    Draw_Stringf(x, y, "Total dlights  : %i", glr.fd.num_dlights); y += 10;
    Draw_Stringf(x, y, "Total particles: %i", glr.fd.num_particles); y += 10;
    Draw_Stringf(x, y, "Uniform uploads: %i", c.uniformUploads); y += 10;
    Draw_Stringf(x, y, "Array binds    : %i", c.vertexArrayBinds); y += 10;
    Draw_Stringf(x, y, "Occl. queries  : %i", c.occlusionQueries); y += 10;

    R_SetScale(1.0f);
}

void Draw_Lightmaps(void)
{
    int block = lm.block_size;
    int rows = 0, cols = 0;

    while (block) {
        rows = max(r_config.height / block, 1);
        cols = max(lm.nummaps / rows, 1);
        if (cols * block <= r_config.width)
            break;
        block >>= 1;
    }

    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            int k = j * cols + i;
            if (k < lm.nummaps)
                GL_StretchPic_(block * i, block * j, block, block,
                               0, 0, 1, 1, U32_WHITE, lm.texnums[k], 0);
        }
    }
}

void Draw_Scrap(void)
{
    GL_StretchPic_(0, 0, 256, 256,
                   0, 0, 1, 1, U32_WHITE, TEXNUM_SCRAP, IF_PALETTED | IF_TRANSPARENT);
}

#endif
