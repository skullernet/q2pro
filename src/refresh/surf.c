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

/*
 * gl_surf.c -- surface post-processing code
 *
 */
#include "gl.h"
#include "common/mdfour.h"

lightmap_builder_t lm;
static byte lm_buffer[0x4000000];

/*
=============================================================================

LIGHTMAP COLOR ADJUSTING

=============================================================================
*/

static inline void
adjust_color_f(vec_t *out, const vec_t *in, float add, float modulate, float scale)
{
    float r, g, b, y, max;

    // add & modulate
    r = (in[0] + add) * modulate;
    g = (in[1] + add) * modulate;
    b = (in[2] + add) * modulate;

    // catch negative lights
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    // determine the brightest of the three color components
    max = g;
    if (r > max)
        max = r;
    if (b > max)
        max = b;

    // rescale all the color components if the intensity of the greatest
    // channel exceeds 1.0
    if (max > 255) {
        y = 255.0f / max;
        r *= y;
        g *= y;
        b *= y;
    }

    // transform to grayscale by replacing color components with
    // overall pixel luminance computed from weighted color sum
    if (scale != 1) {
        y = LUMINANCE(r, g, b);
        r = y + (r - y) * scale;
        g = y + (g - y) * scale;
        b = y + (b - y) * scale;
    }

    out[0] = r;
    out[1] = g;
    out[2] = b;
}

void GL_AdjustColor(vec3_t color)
{
    adjust_color_f(color, color, lm.add, gl_static.entity_modulate, lm.scale);
    VectorScale(color, (1.0f / 255), color);
}

/*
=============================================================================

DYNAMIC BLOCKLIGHTS

=============================================================================
*/

#define MAX_LIGHTMAP_EXTENTS    513
#define MAX_BLOCKLIGHTS         (MAX_LIGHTMAP_EXTENTS * MAX_LIGHTMAP_EXTENTS)

#define LM_PIXELS(map, s, t)    ((map)->buffer + ((t) << lm.block_shift) + ((s) << 2))

static float blocklights[MAX_BLOCKLIGHTS * 3];

static void put_blocklights(const mface_t *surf)
{
    float add, modulate, scale = lm.scale;
    int i, j, smax, tmax, stride = 1 << lm.block_shift;
    const float *bl;
    byte *out;

    if (gl_static.use_shaders) {
        add = 0;
        modulate = 1;
    } else {
        add = lm.add;
        modulate = lm.modulate;
    }

    smax = surf->lm_width;
    tmax = surf->lm_height;

    out = LM_PIXELS(surf->light_m, surf->light_s, surf->light_t);

    for (i = 0, bl = blocklights; i < tmax; i++, out += stride) {
        byte *dst;
        for (j = 0, dst = out; j < smax; j++, bl += 3, dst += 4) {
            vec3_t tmp;
            adjust_color_f(tmp, bl, add, modulate, scale);
            dst[0] = (byte)tmp[0];
            dst[1] = (byte)tmp[1];
            dst[2] = (byte)tmp[2];
            dst[3] = 255;
        }
    }
}

static void add_dynamic_lights(const mface_t *surf)
{
    const dlight_t  *light;
    vec3_t          point;
    vec2_t          local;
    vec_t           s_scale, t_scale, sd, td;
    vec_t           dist, rad, minlight, scale, frac;
    float           *bl;
    int             i, smax, tmax, s, t;

    smax = surf->lm_width;
    tmax = surf->lm_height;
    s_scale = surf->lm_scale[0];
    t_scale = surf->lm_scale[1];

    for (i = 0; i < glr.fd.num_dlights; i++) {
        if (!(surf->dlightbits & BIT_ULL(i)))
            continue;

        light = &glr.fd.dlights[i];
        dist = PlaneDiffFast(light->transformed, surf->plane);
        rad = light->intensity - fabsf(dist);
        if (rad < DLIGHT_CUTOFF)
            continue;

        if (gl_dlight_falloff->integer) {
            minlight = rad - DLIGHT_CUTOFF * 0.8f;
            scale = rad / minlight; // fall off from rad to 0
        } else {
            minlight = rad - DLIGHT_CUTOFF;
            scale = 1;              // fall off from rad to minlight
        }

        VectorMA(light->transformed, -dist, surf->plane->normal, point);

        local[0] = DotProduct(point, surf->lm_axis[0]) + surf->lm_offset[0];
        local[1] = DotProduct(point, surf->lm_axis[1]) + surf->lm_offset[1];

        bl = blocklights;
        for (t = 0; t < tmax; t++) {
            td = fabsf(local[1] - t) * t_scale;
            for (s = 0; s < smax; s++) {
                sd = fabsf(local[0] - s) * s_scale;
                if (sd > td)
                    dist = sd + td * 0.5f;
                else
                    dist = td + sd * 0.5f;
                if (dist < minlight) {
                    frac = rad - dist * scale;
                    VectorMA(bl, frac, light->color, bl);
                }
                bl += 3;
            }
        }
    }
}

static void add_light_styles(mface_t *surf)
{
    const lightstyle_t *style;
    const byte *src;
    float *bl;
    int i, j, size = surf->lm_width * surf->lm_height;

    if (!surf->numstyles) {
        // should this ever happen?
        memset(blocklights, 0, sizeof(blocklights[0]) * size * 3);
        return;
    }

    // init primary lightmap
    style = LIGHT_STYLE(surf->styles[0]);

    src = surf->lightmap;
    bl = blocklights;
    if (style->white == 1) {
        for (j = 0; j < size; j++, bl += 3, src += 3)
            VectorCopy(src, bl);
    } else {
        for (j = 0; j < size; j++, bl += 3, src += 3)
            VectorScale(src, style->white, bl);
    }

    surf->stylecache[0] = style->white;

    // add remaining lightmaps
    for (i = 1; i < surf->numstyles; i++) {
        style = LIGHT_STYLE(surf->styles[i]);

        bl = blocklights;
        for (j = 0; j < size; j++, bl += 3, src += 3)
            VectorMA(bl, style->white, src, bl);

        surf->stylecache[i] = style->white;
    }
}

static void update_dynamic_lightmap(mface_t *surf)
{
    int s0, t0, s1, t1;

    // add all the lightmaps
    add_light_styles(surf);

    // add all the dynamic lights
    if (surf->dlightframe == glr.dlightframe)
        add_dynamic_lights(surf);
    else
        surf->dlightframe = 0;

    // put into texture format
    put_blocklights(surf);

    // add to dirty region
    s0 = surf->light_s;
    t0 = surf->light_t;

    s1 = s0 + surf->lm_width;
    t1 = t0 + surf->lm_height;

    lightmap_t *m = surf->light_m;

    m->mins[0] = min(m->mins[0], s0);
    m->mins[1] = min(m->mins[1], t0);

    m->maxs[0] = max(m->maxs[0], s1);
    m->maxs[1] = max(m->maxs[1], t1);
}

// updates lightmaps in RAM
void GL_PushLights(mface_t *surf)
{
    const lightstyle_t *style;
    int i;

    if (!surf->light_m)
        return;

    // dynamic this frame or dynamic previously
    if (surf->dlightframe) {
        update_dynamic_lightmap(surf);
        return;
    }

    // check for light style updates
    for (i = 0; i < surf->numstyles; i++) {
        style = LIGHT_STYLE(surf->styles[i]);
        if (style->white != surf->stylecache[i]) {
            update_dynamic_lightmap(surf);
            return;
        }
    }
}

static void clear_dirty_region(lightmap_t *m)
{
    m->mins[0] = lm.block_size;
    m->mins[1] = lm.block_size;
    m->maxs[0] = 0;
    m->maxs[1] = 0;
}

// uploads dirty lightmap regions to GL
void GL_UploadLightmaps(void)
{
    lightmap_t *m;
    bool set = false;
    int i;

    for (i = 0, m = lm.lightmaps; i < lm.nummaps; i++, m++) {
        int x, y, w, h;

        if (m->mins[0] >= m->maxs[0] || m->mins[1] >= m->maxs[1])
            continue;

        x = m->mins[0];
        y = m->mins[1];
        w = m->maxs[0] - x;
        h = m->maxs[1] - y;

        if (!(gl_config.caps & QGL_CAP_UNPACK_SUBIMAGE)) {
            x = 0;
            w = lm.block_size;
        } else if (!set) {
            qglPixelStorei(GL_UNPACK_ROW_LENGTH, lm.block_size);
            set = true;
        }

        // upload lightmap subimage
        GL_ForceTexture(TMU_LIGHTMAP, lm.texnums[i]);
        qglTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                         GL_RGBA, GL_UNSIGNED_BYTE, LM_PIXELS(m, x, y));
        clear_dirty_region(m);
        c.texUploads++;
        c.lightTexels += w * h;
    }

    if (set)
        qglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

/*
=============================================================================

LIGHTMAPS BUILDING

=============================================================================
*/

#define LM_AllocBlock(w, h, s, t) \
    GL_AllocBlock(lm.block_size, lm.block_size, lm.inuse, w, h, s, t)

static void LM_InitBlock(void)
{
    memset(lm.inuse, 0, sizeof(lm.inuse));
    memset(lm.lightmaps[lm.nummaps].buffer, 0, lm.block_bytes);
}

static void LM_UploadBlock(void)
{
    if (!lm.dirty)
        return;

    Q_assert(lm.nummaps < lm.maxmaps);

    lightmap_t *m = &lm.lightmaps[lm.nummaps];
    clear_dirty_region(m);

    GL_ForceTexture(TMU_LIGHTMAP, lm.texnums[lm.nummaps]);
    qglTexImage2D(GL_TEXTURE_2D, 0, lm.comp,
                  lm.block_size, lm.block_size, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, m->buffer);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    lm.nummaps++;
    lm.dirty = false;
}

static void build_style_map(int dynamic)
{
    int i;

    if (!dynamic) {
        // make all styles fullbright
        memset(gl_static.lightstylemap, 0, sizeof(gl_static.lightstylemap));
        return;
    }

    for (i = 0; i < MAX_LIGHTSTYLES; i++)
        gl_static.lightstylemap[i] = i;

    if (dynamic != 1) {
        // make dynamic styles fullbright
        for (i = 1; i < 32; i++)
            gl_static.lightstylemap[i] = 0;
    }
}

static bool no_lightmaps(void)
{
    return gl_fullbright->integer || gl_vertexlight->integer;
}

static void LM_BeginBuilding(void)
{
    const bsp_t *bsp = gl_static.world.cache;
    int size_shift, bits;

    // start up with fullbright styles
    build_style_map(0);

    // lightmap textures are not deleted from memory when changing maps,
    // they are merely reused
    lm.nummaps = lm.maxmaps = 0;
    lm.dirty = false;

    if (no_lightmaps())
        return;

    // use larger lightmaps for DECOUPLED_LM maps
    if (gl_lightmap_bits->integer)
        bits = Cvar_ClampInteger(gl_lightmap_bits, 7, 10);
    else
        bits = 8 + bsp->lm_decoupled * 2;

    // clamp to maximum texture size
    bits = min(bits, gl_config.max_texture_size_log2);

    lm.block_size = 1 << bits;
    lm.block_shift = bits + 2;

    size_shift = bits * 2 + 2;
    lm.block_bytes = 1 << size_shift;
    lm.maxmaps = min(sizeof(lm_buffer) >> size_shift, LM_MAX_LIGHTMAPS);

    for (int i = 0; i < lm.maxmaps; i++)
        lm.lightmaps[i].buffer = lm_buffer + (i << size_shift);

    Com_DPrintf("%s: %d lightmaps max, %d block size\n", __func__, lm.maxmaps, lm.block_size);

    LM_InitBlock();
}

static void LM_EndBuilding(void)
{
    // vertex lighting implies fullbright styles
    if (no_lightmaps())
        return;

    // upload the last lightmap
    LM_UploadBlock();

    // now build the real lightstyle map
    build_style_map(gl_dynamic->integer);

    Com_DPrintf("%s: %d lightmaps built\n", __func__, lm.nummaps);
}

static void build_primary_lightmap(mface_t *surf)
{
    // add all the lightmaps
    add_light_styles(surf);

    surf->dlightframe = 0;

    // put into texture format
    put_blocklights(surf);
}

static void LM_BuildSurface(mface_t *surf)
{
    int smax, tmax, s, t;

    if (lm.nummaps >= lm.maxmaps)
        return;     // can't have any more

    smax = surf->lm_width;
    tmax = surf->lm_height;

    if (!LM_AllocBlock(smax, tmax, &s, &t)) {
        LM_UploadBlock();
        if (lm.nummaps >= lm.maxmaps) {
            Com_EPrintf("%s: too many lightmaps\n", __func__);
            return;
        }
        LM_InitBlock();
        if (!LM_AllocBlock(smax, tmax, &s, &t)) {
            Com_EPrintf("%s: LM_AllocBlock(%d, %d) failed\n",
                        __func__, smax, tmax);
            return;
        }
    }

    lm.dirty = true;

    // store the surface lightmap parameters
    surf->light_s = s;
    surf->light_t = t;
    surf->light_m = &lm.lightmaps[lm.nummaps];

    // build the primary lightmap
    build_primary_lightmap(surf);
}

static void LM_RebuildSurfaces(void)
{
    const bsp_t *bsp = gl_static.world.cache;
    mface_t *surf;
    lightmap_t *m;
    int i;

    build_style_map(gl_dynamic->integer);

    if (!lm.nummaps)
        return;

    for (i = 0, surf = bsp->faces; i < bsp->numfaces; i++, surf++)
        if (surf->light_m)
            build_primary_lightmap(surf);

    // upload all lightmaps
    for (i = 0, m = lm.lightmaps; i < lm.nummaps; i++, m++) {
        GL_ForceTexture(TMU_LIGHTMAP, lm.texnums[i]);
        qglTexImage2D(GL_TEXTURE_2D, 0, lm.comp,
                      lm.block_size, lm.block_size, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, m->buffer);
        clear_dirty_region(m);
        c.texUploads++;
    }
}

/*
=============================================================================

POLYGONS BUILDING

=============================================================================
*/

#define DotProductDouble(x,y) \
    ((double)(x)[0]*(y)[0]+\
     (double)(x)[1]*(y)[1]+\
     (double)(x)[2]*(y)[2])

static uint32_t color_for_surface(const mface_t *surf)
{
    if (surf->drawflags & SURF_TRANS33)
        return gl_static.inverse_intensity_33;

    if (surf->drawflags & SURF_TRANS66)
        return gl_static.inverse_intensity_66;

    if (surf->drawflags & SURF_WARP)
        return gl_static.inverse_intensity_100;

    return U32_WHITE;
}

static bool enable_intensity_for_surface(const mface_t *surf)
{
    // enable for any surface with a lightmap in DECOUPLED_LM maps
    if (surf->lightmap && gl_static.world.cache->lm_decoupled)
        return true;

    // enable for non-transparent, non-warped surfaces
    if (!(surf->drawflags & SURF_COLOR_MASK))
        return true;

    // enable for non-transparent lava (hack)
    if (!(surf->drawflags & SURF_TRANS_MASK) && strstr(surf->texinfo->name, "lava"))
        return true;

    return false;
}

static glStateBits_t statebits_for_surface(const mface_t *surf)
{
    glStateBits_t statebits = GLS_DEFAULT;

    if (surf->drawflags & SURF_SKY) {
        if (Q_stricmpn(surf->texinfo->name, CONST_STR_LEN("n64/env/sky")) == 0)
            return GLS_TEXTURE_REPLACE | GLS_CLASSIC_SKY;
        else
            return GLS_TEXTURE_REPLACE | GLS_DEFAULT_SKY;
    }

    if (gl_static.use_shaders) {
        // no inverse intensity
        if (!(surf->drawflags & SURF_TRANS_MASK))
            statebits |= GLS_TEXTURE_REPLACE;
        if (enable_intensity_for_surface(surf))
            statebits |= GLS_INTENSITY_ENABLE;
    } else {
        if (!(surf->drawflags & SURF_COLOR_MASK))
            statebits |= GLS_TEXTURE_REPLACE;
    }

    if (surf->drawflags & SURF_WARP)
        statebits |= GLS_WARP_ENABLE;

    if (surf->drawflags & SURF_TRANS_MASK)
        statebits |= GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE;
    else if (surf->drawflags & SURF_ALPHATEST)
        statebits |= GLS_ALPHATEST_ENABLE;

    if (surf->drawflags & SURF_FLOWING) {
        statebits |= GLS_SCROLL_ENABLE;
        if (surf->drawflags & SURF_WARP)
            statebits |= GLS_SCROLL_SLOW;
    }

    if (surf->drawflags & SURF_N64_SCROLL_X)
        statebits |= GLS_SCROLL_ENABLE | GLS_SCROLL_X;

    if (surf->drawflags & SURF_N64_SCROLL_Y)
        statebits |= GLS_SCROLL_ENABLE | GLS_SCROLL_Y;

    if (surf->drawflags & SURF_N64_SCROLL_FLIP)
        statebits |= GLS_SCROLL_FLIP;

    return statebits;
}

static void build_surface_poly(mface_t *surf, vec_t *vbo)
{
    const bsp_t *bsp = gl_static.world.cache;
    const msurfedge_t *src_surfedge;
    const mvertex_t *src_vert;
    const medge_t *src_edge;
    const mtexinfo_t *texinfo = surf->texinfo;
    const uint32_t color = color_for_surface(surf);
    vec2_t scale, tc, mins, maxs;
    int i, bmins[2], bmaxs[2];

    // convert surface flags to state bits
    surf->statebits = statebits_for_surface(surf);

    // normalize texture coordinates
    scale[0] = 1.0f / texinfo->image->width;
    scale[1] = 1.0f / texinfo->image->height;

    if (surf->drawflags & SURF_N64_UV) {
        scale[0] *= 0.5f;
        scale[1] *= 0.5f;
    }

    mins[0] = mins[1] = 99999;
    maxs[0] = maxs[1] = -99999;

    src_surfedge = surf->firstsurfedge;
    for (i = 0; i < surf->numsurfedges; i++) {
        src_edge = bsp->edges + src_surfedge->edge;
        src_vert = bsp->vertices + src_edge->v[src_surfedge->vert];
        src_surfedge++;

        // vertex coordinates
        VectorCopy(src_vert->point, vbo);

        // vertex color
        WN32(vbo + 3, color);

        // texture0 coordinates
        tc[0] = DotProductDouble(vbo, texinfo->axis[0]) + texinfo->offset[0];
        tc[1] = DotProductDouble(vbo, texinfo->axis[1]) + texinfo->offset[1];

        vbo[4] = tc[0] * scale[0];
        vbo[5] = tc[1] * scale[1];

        // texture1 coordinates
        if (bsp->lm_decoupled) {
            vbo[6] = DotProduct(vbo, surf->lm_axis[0]) + surf->lm_offset[0];
            vbo[7] = DotProduct(vbo, surf->lm_axis[1]) + surf->lm_offset[1];
        } else {
            if (mins[0] > tc[0]) mins[0] = tc[0];
            if (maxs[0] < tc[0]) maxs[0] = tc[0];

            if (mins[1] > tc[1]) mins[1] = tc[1];
            if (maxs[1] < tc[1]) maxs[1] = tc[1];

            vbo[6] = tc[0] / 16;
            vbo[7] = tc[1] / 16;
        }

        vbo += VERTEX_SIZE;
    }

    if (bsp->lm_decoupled) {
        surf->lm_scale[0] = 1.0f / VectorLength(surf->lm_axis[0]);
        surf->lm_scale[1] = 1.0f / VectorLength(surf->lm_axis[1]);
        return;
    }

    // calculate surface extents
    bmins[0] = floor(mins[0] / 16);
    bmins[1] = floor(mins[1] / 16);
    bmaxs[0] = ceil(maxs[0] / 16);
    bmaxs[1] = ceil(maxs[1] / 16);

    VectorScale(texinfo->axis[0], 1.0f / 16, surf->lm_axis[0]);
    VectorScale(texinfo->axis[1], 1.0f / 16, surf->lm_axis[1]);
    surf->lm_offset[0] = texinfo->offset[0] / 16 - bmins[0];
    surf->lm_offset[1] = texinfo->offset[1] / 16 - bmins[1];
    surf->lm_width  = bmaxs[0] - bmins[0] + 1;
    surf->lm_height = bmaxs[1] - bmins[1] + 1;
    surf->lm_scale[0] = 16;
    surf->lm_scale[1] = 16;

    for (i = 0; i < surf->numsurfedges; i++) {
        vbo -= VERTEX_SIZE;
        vbo[6] -= bmins[0];
        vbo[7] -= bmins[1];
    }
}

// vertex lighting approximation
static void sample_surface_verts(mface_t *surf, vec_t *vbo)
{
    int     i;
    vec3_t  color;
    byte    *dst;

    if (surf->drawflags & SURF_COLOR_MASK)
        return;

    glr.lightpoint.surf = surf;

    for (i = 0; i < surf->numsurfedges; i++) {
        glr.lightpoint.s = (int)vbo[6];
        glr.lightpoint.t = (int)vbo[7];

        GL_SampleLightPoint(color);
        adjust_color_f(color, color, lm.add, lm.modulate, lm.scale);

        dst = (byte *)(vbo + 3);
        dst[0] = (byte)color[0];
        dst[1] = (byte)color[1];
        dst[2] = (byte)color[2];
        dst[3] = 255;

        vbo += VERTEX_SIZE;
    }

    surf->statebits &= ~GLS_TEXTURE_REPLACE;
    surf->statebits |= GLS_SHADE_SMOOTH;
}

// normalizes and stores lightmap texture coordinates in vertices
static void normalize_surface_lmtc(const mface_t *surf, vec_t *vbo)
{
    float s, t, scale = 1.0f / lm.block_size;
    int i;

    s = surf->light_s + 0.5f;
    t = surf->light_t + 0.5f;

    for (i = 0; i < surf->numsurfedges; i++) {
        vbo[6] += s;
        vbo[7] += t;
        vbo[6] *= scale;
        vbo[7] *= scale;

        vbo += VERTEX_SIZE;
    }
}

// validates and processes surface lightmap
static void build_surface_light(mface_t *surf, vec_t *vbo)
{
    const bsp_t *bsp = gl_static.world.cache;
    int smax, tmax, size, ofs;

    if (gl_fullbright->integer)
        return;

    if (!surf->lightmap)
        return;

    if (surf->drawflags & gl_static.nolm_mask)
        return;

    smax = surf->lm_width;
    tmax = surf->lm_height;

    // validate lightmap extents
    if (smax < 1 || tmax < 1 || smax > MAX_LIGHTMAP_EXTENTS || tmax > MAX_LIGHTMAP_EXTENTS) {
        Com_WPrintf("Bad lightmap extents: %d x %d\n", smax, tmax);
        surf->lightmap = NULL;  // don't use this lightmap
        return;
    }

    // validate lightmap bounds
    size = smax * tmax;
    ofs = surf->lightmap - bsp->lightmap;
    if (surf->numstyles * size * 3 > bsp->numlightmapbytes - ofs) {
        Com_WPrintf("Bad surface lightmap\n");
        surf->lightmap = NULL;  // don't use this lightmap
        return;
    }

    if (gl_vertexlight->integer) {
        sample_surface_verts(surf, vbo);
    } else {
        LM_BuildSurface(surf);
        normalize_surface_lmtc(surf, vbo);
    }
}

static void calc_surface_hash(mface_t *surf)
{
    uint32_t args[] = {
        surf->texinfo->image - r_images,
        surf->light_m ? surf->light_m - lm.lightmaps : 0,
        surf->statebits
    };
    struct mdfour md;
    uint8_t out[16];

    mdfour_begin(&md);
    mdfour_update(&md, (uint8_t *)args, sizeof(args));
    mdfour_result(&md, out);

    surf->hash = 0;
    for (int i = 0; i < 16; i++)
        surf->hash ^= out[i];
}

static bool create_surface_vbo(size_t size)
{
    GLuint buf = 0;

    if (!qglGenBuffers)
        return false;

#if USE_GLES
    if (size > 65536 * VERTEX_SIZE * sizeof(vec_t))
        return false;
#endif

#if USE_DEBUG
    if (gl_novbo->integer)
        return false;
#endif

    GL_ClearErrors();

    qglGenBuffers(1, &buf);
    GL_BindBuffer(GL_ARRAY_BUFFER, buf);
    qglBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);

    if (GL_ShowErrors("Failed to create world model VBO")) {
        GL_BindBuffer(GL_ARRAY_BUFFER, 0);
        qglDeleteBuffers(1, &buf);
        return false;
    }

    gl_static.world.vertices = NULL;
    gl_static.world.buffer = buf;
    return true;
}

static void upload_surface_vbo(int lastvert)
{
    size_t offset = lastvert * VERTEX_SIZE * sizeof(vec_t);
    size_t size = tess.numverts * VERTEX_SIZE * sizeof(vec_t);

    Com_DDPrintf("%s: %zu bytes at %zu\n", __func__, size, offset);

    qglBufferSubData(GL_ARRAY_BUFFER, offset, size, tess.vertices);
    tess.numverts = 0;
}

static void check_multitexture(void)
{
    if (gl_vertexlight->integer)
        return;
    if (qglActiveTexture && (qglClientActiveTexture || gl_static.use_shaders))
        return;
    Com_WPrintf("OpenGL doesn't support multitexturing, forcing vertex lighting.\n");
    Cvar_Set("gl_vertexlight", "1");
}

static void upload_world_surfaces(void)
{
    const bsp_t *bsp = gl_static.world.cache;
    size_t size = gl_static.world.buffer_size;
    vec_t *vbo;
    mface_t *surf;
    int i, currvert, lastvert;

    // force vertex lighting if multitexture is not supported
    check_multitexture();

    // begin building lightmaps
    LM_BeginBuilding();

    if (!gl_static.world.vertices)
        GL_BindBuffer(GL_ARRAY_BUFFER, gl_static.world.buffer);

    currvert = 0;
    lastvert = 0;
    for (i = 0, surf = bsp->faces; i < bsp->numfaces; i++, surf++) {
        if (surf->drawflags & SURF_SKY && !gl_static.use_cubemaps)
            continue;
        if (surf->drawflags & SURF_NODRAW)
            continue;

        Q_assert(surf->numsurfedges >= 3 && surf->numsurfedges <= TESS_MAX_VERTICES);
        Q_assert(size >= surf->numsurfedges * VERTEX_SIZE * sizeof(vbo[0]));
        size -= surf->numsurfedges * VERTEX_SIZE * sizeof(vbo[0]);

        if (gl_static.world.vertices) {
            vbo = gl_static.world.vertices + currvert * VERTEX_SIZE;
        } else {
            // upload VBO chunk if needed
            if (tess.numverts + surf->numsurfedges > TESS_MAX_VERTICES) {
                upload_surface_vbo(lastvert);
                lastvert = currvert;
            }

            vbo = tess.vertices + tess.numverts * VERTEX_SIZE;
            tess.numverts += surf->numsurfedges;
        }

        surf->light_m = NULL;   // start with no lightmap
        surf->firstvert = currvert;
        build_surface_poly(surf, vbo);
        build_surface_light(surf, vbo);

        calc_surface_hash(surf);

        currvert += surf->numsurfedges;
    }

    // upload the last VBO chunk
    if (!gl_static.world.vertices)
        upload_surface_vbo(lastvert);

    // end building lightmaps
    LM_EndBuilding();

    gl_fullbright->modified = false;
    gl_vertexlight->modified = false;
    gl_lightmap_bits->modified = false;
}

static void set_world_size(const mnode_t *node)
{
    vec_t size;
    int i;

    for (i = 0, size = 0; i < 3; i++)
        size = max(size, node->maxs[i] - node->mins[i]);

    if (size > 4096)
        gl_static.world.size = 8192;
    else if (size > 2048)
        gl_static.world.size = 4096;
    else
        gl_static.world.size = 2048;
}

// called from the main loop whenever lighting parameters change
void GL_RebuildLighting(void)
{
    if (!gl_static.world.cache)
        return;

    // rebuild all surfaces if toggling lightmaps off/on
    if (gl_fullbright->modified || gl_vertexlight->modified) {
        upload_world_surfaces();
        return;
    }

    if (gl_fullbright->integer)
        return;

    // rebuild all surfaces if doing vertex lighting (and not fullbright)
    if (gl_vertexlight->integer || gl_lightmap_bits->modified) {
        upload_world_surfaces();
        return;
    }

    // rebuild all lightmaps
    LM_RebuildSurfaces();
}

void GL_FreeWorld(void)
{
    if (!gl_static.world.cache)
        return;

    BSP_Free(gl_static.world.cache);

    if (gl_static.world.vertices)
        Z_Free(gl_static.world.vertices);
    else if (qglDeleteBuffers)
        qglDeleteBuffers(1, &gl_static.world.buffer);

    // invalidate bindings
    if (gls.currentva == VA_3D)
        gls.currentva = VA_NONE;
    if (gls.currentbuffer[0] == gl_static.world.buffer)
        gls.currentbuffer[0] = 0;

    memset(&gl_static.world, 0, sizeof(gl_static.world));
}

void GL_LoadWorld(const char *name)
{
    char buffer[MAX_QPATH];
    size_t size;
    bsp_t *bsp;
    mtexinfo_t *info;
    mface_t *surf;
    int i, n64surfs, ret;

    if (!name || !*name)
        return;

    Q_concat(buffer, sizeof(buffer), "maps/", name, ".bsp");
    ret = BSP_Load(buffer, &bsp);
    if (!bsp)
        Com_Error(ERR_DROP, "%s: couldn't load %s: %s",
                  __func__, buffer, BSP_ErrorString(ret));

    // check if the required world model was already loaded
    if (gl_static.world.cache == bsp) {
        for (i = 0; i < bsp->numtexinfo; i++)
            bsp->texinfo[i].image->registration_sequence = r_registration_sequence;

        for (i = 0; i < bsp->numnodes; i++)
            bsp->nodes[i].visframe = 0;

        for (i = 0; i < bsp->numleafs; i++)
            bsp->leafs[i].visframe = 0;

        Com_DPrintf("%s: reused old world model\n", __func__);
        bsp->refcount--;
        return;
    }

    // free previous model, if any
    GL_FreeWorld();

    // delete occlusion queries
    GL_DeleteQueries();
    GL_InitQueries();

    gl_static.world.cache = bsp;

    // calculate world size for far clip plane and sky box
    set_world_size(bsp->nodes);

    // register all texinfo
    for (i = 0, info = bsp->texinfo; i < bsp->numtexinfo; i++, info++) {
        if (info->c.flags & SURF_SKY) {
            if (!gl_static.use_cubemaps) {
                info->image = R_NOTEXTURE;
            } else if (Q_stricmpn(info->name, CONST_STR_LEN("n64/env/sky")) == 0) {
                Q_concat(buffer, sizeof(buffer), "textures/", info->name, ".tga");
                info->image = IMG_Find(buffer, IT_SKY, IF_REPEAT | IF_CLASSIC_SKY);
            } else if (Q_stricmpn(info->name, CONST_STR_LEN("sky/")) == 0) {
                Q_concat(buffer, sizeof(buffer), info->name, ".tga");
                info->image = IMG_Find(buffer, IT_SKY, IF_CUBEMAP);
            } else {
                info->image = R_SKYTEXTURE;
            }
        } else if (info->c.flags & SURF_NODRAW) {
            info->image = R_NOTEXTURE;
        } else {
            imageflags_t flags = (info->c.flags & SURF_WARP) ? IF_TURBULENT : IF_NONE;
            Q_concat(buffer, sizeof(buffer), "textures/", info->name, ".wal");
            info->image = IMG_Find(buffer, IT_WALL, flags);
        }
    }

    // calculate vertex buffer size in bytes
    size = 0;
    for (i = n64surfs = 0, surf = bsp->faces; i < bsp->numfaces; i++, surf++) {
        // hack surface flags into drawflags for faster access
        surf->drawflags |= surf->texinfo->c.flags & ~DSURF_PLANEBACK;

        // clear statebits from previous load
        surf->statebits = GLS_DEFAULT;

        // don't count sky surfaces
        if (surf->drawflags & SURF_SKY) {
            if (!gl_static.use_cubemaps)
                continue;
            surf->drawflags &= ~SURF_NODRAW;
        }
        if (surf->drawflags & SURF_NODRAW)
            continue;
        if (surf->drawflags & SURF_N64_UV)
            n64surfs++;

        size += surf->numsurfedges * VERTEX_SIZE * sizeof(vec_t);
    }

    // try VBO first, then allocate on heap
    if (create_surface_vbo(size)) {
        Com_DPrintf("%s: %zu bytes of vertex data as VBO\n", __func__, size);
    } else {
        gl_static.world.vertices = Z_TagMalloc(size, TAG_RENDERER);
        Com_DPrintf("%s: %zu bytes of vertex data on heap\n", __func__, size);
    }
    gl_static.world.buffer_size = size;

    gl_static.nolm_mask = SURF_NOLM_MASK_DEFAULT;
    gl_static.use_bmodel_skies = false;

    // only supported in DECOUPLED_LM maps because vanilla maps have broken
    // lightofs for liquids/alphas. legacy renderer doesn't support lightmapped
    // liquids too.
    if ((bsp->lm_decoupled || n64surfs > 100) && gl_static.use_shaders) {
        gl_static.nolm_mask = SURF_NOLM_MASK_REMASTER;
        gl_static.use_bmodel_skies = gl_static.use_cubemaps;
    }

    glr.fd.lightstyles = &(lightstyle_t){ 1 };

    // post process all surfaces
    upload_world_surfaces();

    glr.fd.lightstyles = NULL;

    GL_ShowErrors(__func__);
}
