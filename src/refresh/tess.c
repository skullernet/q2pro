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

tesselator_t tess;

#define FACE_HASH_BITS  8
#define FACE_HASH_SIZE  (1 << FACE_HASH_BITS)
#define FACE_HASH_MASK  (FACE_HASH_SIZE - 1)

static mface_t  *faces_head[FACE_HASH_SIZE];
static mface_t  **faces_next[FACE_HASH_SIZE];
static mface_t  *faces_alpha;

void GL_Flush2D(void)
{
    glStateBits_t bits;

    if (!tess.numverts)
        return;

    bits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_FALSE | GLS_CULL_DISABLE | tess.flags;
    if (bits & GLS_BLEND_BLEND)
        bits &= ~GLS_ALPHATEST_ENABLE;

    Scrap_Upload();

    GL_BindTexture(TMU_TEXTURE, tess.texnum[TMU_TEXTURE]);
    GL_BindArrays(VA_2D);
    GL_StateBits(bits);
    GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);
    GL_DrawIndexed(SHOWTRIS_PIC);

    c.batchesDrawn2D++;

    tess.numindices = 0;
    tess.numverts = 0;
    tess.texnum[TMU_TEXTURE] = 0;
    tess.flags = 0;
}

#define PARTICLE_SIZE   (1 + M_SQRT1_2f)
#define PARTICLE_SCALE  (1 / (2 * PARTICLE_SIZE))

void GL_DrawParticles(void)
{
    const particle_t *p;
    int total, count;
    vec3_t transformed;
    vec_t scale, scale2, dist;
    color_t color;
    int numverts;
    vec_t *dst_vert;
    glStateBits_t bits;

    if (!glr.fd.num_particles)
        return;

    GL_LoadMatrix(glr.viewmatrix);
    GL_BindArrays(VA_EFFECT);

    bits = (gl_partstyle->integer ? GLS_BLEND_ADD : GLS_BLEND_BLEND) | GLS_DEPTHMASK_FALSE;

    p = glr.fd.particles;
    total = glr.fd.num_particles;
    do {
        GL_BindTexture(TMU_TEXTURE, TEXNUM_PARTICLE);
        GL_StateBits(bits);
        GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);

        count = min(total, TESS_MAX_VERTICES / 3);
        total -= count;

        dst_vert = tess.vertices;
        numverts = count * 3;
        do {
            VectorSubtract(p->origin, glr.fd.vieworg, transformed);
            dist = DotProduct(transformed, glr.viewaxis[0]);

            scale = 1.0f;
            if (dist > 20)
                scale += dist * 0.004f;
            scale *= gl_partscale->value;
            scale2 = scale * PARTICLE_SCALE;

            VectorMA(p->origin, scale2, glr.viewaxis[1], dst_vert);
            VectorMA(dst_vert, -scale2, glr.viewaxis[2], dst_vert);
            VectorMA(dst_vert,  scale,  glr.viewaxis[2], dst_vert +  6);
            VectorMA(dst_vert, -scale,  glr.viewaxis[1], dst_vert + 12);

            dst_vert[ 3] = 0;               dst_vert[ 4] = 0;
            dst_vert[ 9] = 0;               dst_vert[10] = PARTICLE_SIZE;
            dst_vert[15] = PARTICLE_SIZE;   dst_vert[16] = 0;

            if (p->color == -1)
                color.u32 = p->rgba.u32;
            else
                color.u32 = d_8to24table[p->color & 0xff];
            color.u8[3] *= p->alpha;

            WN32(dst_vert +  5, color.u32);
            WN32(dst_vert + 11, color.u32);
            WN32(dst_vert + 17, color.u32);

            dst_vert += 18;
            p++;
        } while (--count);

        GL_LockArrays(numverts);
        qglDrawArrays(GL_TRIANGLES, 0, numverts);

        if (gl_showtris->integer & SHOWTRIS_FX)
            GL_DrawOutlines(numverts, NULL, false);

        GL_UnlockArrays();
    } while (total);
}

static void GL_FlushBeamSegments(void)
{
    if (!tess.numindices)
        return;

    glArrayBits_t array = GLA_VERTEX | GLA_COLOR;
    GLuint texnum = TEXNUM_BEAM;

    if (gl_beamstyle->integer)
        texnum = TEXNUM_WHITE;
    else
        array |= GLA_TC;

    GL_BindTexture(TMU_TEXTURE, texnum);
    GL_StateBits(GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE);
    GL_ArrayBits(array);
    GL_DrawIndexed(SHOWTRIS_FX);

    tess.numverts = tess.numindices = 0;
}

#define BEAM_POINTS   12

static void GL_DrawPolyBeam(const vec3_t *segments, int num_segments, color_t color, float width)
{
    int i, j, k, firstvert;
    vec3_t points[BEAM_POINTS];
    vec3_t dir, right, up;
    vec_t *dst_vert;
    glIndex_t *dst_indices;

    VectorSubtract(segments[num_segments], segments[0], dir);
    if (VectorNormalize(dir) < 0.1f)
        return;

    MakeNormalVectors(dir, right, up);
    VectorScale(right, width, right);

    if (q_unlikely(tess.numverts + BEAM_POINTS * (num_segments + 1) > TESS_MAX_VERTICES ||
                   tess.numindices + BEAM_POINTS * 6 * num_segments > TESS_MAX_INDICES))
        GL_FlushBeamSegments();

    dst_vert = tess.vertices + tess.numverts * 4;

    for (i = 0; i < BEAM_POINTS; i++) {
        RotatePointAroundVector(points[i], dir, right, (360.0f / BEAM_POINTS) * i);
        VectorAdd(points[i], segments[0], dst_vert);
        WN32(dst_vert + 3, color.u32);
        dst_vert += 4;
    }

    dst_indices = tess.indices + tess.numindices;
    firstvert = tess.numverts;

    for (i = 1; i <= num_segments; i++) {
        for (j = 0; j < BEAM_POINTS; j++) {
            VectorAdd(points[j], segments[i], dst_vert);
            WN32(dst_vert + 3, color.u32);
            dst_vert += 4;

            k = (j + 1) % BEAM_POINTS;
            dst_indices[0] = firstvert + j;
            dst_indices[1] = firstvert + j + BEAM_POINTS;
            dst_indices[2] = firstvert + k + BEAM_POINTS;
            dst_indices[3] = firstvert + j;
            dst_indices[4] = firstvert + k + BEAM_POINTS;
            dst_indices[5] = firstvert + k;
            dst_indices += 6;
        }
        firstvert += BEAM_POINTS;
    }

    tess.numverts += BEAM_POINTS * (num_segments + 1);
    tess.numindices += BEAM_POINTS * 6 * num_segments;
}

static void GL_DrawSimpleBeam(const vec3_t start, const vec3_t end, color_t color, float width)
{
    vec3_t d1, d2, d3;
    vec_t *dst_vert;
    glIndex_t *dst_indices;

    VectorSubtract(end, start, d1);
    VectorSubtract(glr.fd.vieworg, start, d2);
    CrossProduct(d1, d2, d3);
    if (VectorNormalize(d3) < 0.1f)
        return;
    VectorScale(d3, width, d3);

    if (q_unlikely(tess.numverts + 4 > TESS_MAX_VERTICES ||
                   tess.numindices + 6 > TESS_MAX_INDICES))
        GL_FlushBeamSegments();

    dst_vert = tess.vertices + tess.numverts * 6;
    VectorAdd(start, d3, dst_vert);
    VectorSubtract(start, d3, dst_vert + 6);
    VectorSubtract(end, d3, dst_vert + 12);
    VectorAdd(end, d3, dst_vert + 18);

    dst_vert[ 3] = 0; dst_vert[ 4] = 0;
    dst_vert[ 9] = 1; dst_vert[10] = 0;
    dst_vert[15] = 1; dst_vert[16] = 1;
    dst_vert[21] = 0; dst_vert[22] = 1;

    WN32(dst_vert +  5, color.u32);
    WN32(dst_vert + 11, color.u32);
    WN32(dst_vert + 17, color.u32);
    WN32(dst_vert + 23, color.u32);

    dst_indices = tess.indices + tess.numindices;
    dst_indices[0] = tess.numverts + 0;
    dst_indices[1] = tess.numverts + 2;
    dst_indices[2] = tess.numverts + 3;
    dst_indices[3] = tess.numverts + 0;
    dst_indices[4] = tess.numverts + 1;
    dst_indices[5] = tess.numverts + 2;

    tess.numverts += 4;
    tess.numindices += 6;
}

#define MIN_LIGHTNING_SEGMENTS      3
#define MAX_LIGHTNING_SEGMENTS      7
#define MIN_SEGMENT_LENGTH          16

static void GL_DrawLightningBeam(const vec3_t start, const vec3_t end, color_t color, float width)
{
    vec3_t dir, segments[MAX_LIGHTNING_SEGMENTS + 1];
    vec3_t right, up;
    vec_t length, segment_length;
    int i, num_segments, max_segments;

    VectorSubtract(end, start, dir);
    length = VectorNormalize(dir);

    max_segments = Q_clip(length / MIN_SEGMENT_LENGTH, 1, MAX_LIGHTNING_SEGMENTS);

    if (max_segments <= MIN_LIGHTNING_SEGMENTS)
        num_segments = max_segments;
    else
        num_segments = MIN_LIGHTNING_SEGMENTS + Com_SlowRand() % (max_segments - MIN_LIGHTNING_SEGMENTS + 1);

    if (num_segments > 1)
        MakeNormalVectors(dir, right, up);

    segment_length = length / num_segments;
    for (i = 1; i < num_segments; i++) {
        vec3_t point;
        float offs;

        VectorMA(start, i * segment_length, dir, point);

        offs = Com_SlowCrand() * (segment_length * 0.35f);
        VectorMA(point, offs, right, point);

        offs = Com_SlowCrand() * (segment_length * 0.35f);
        VectorMA(point, offs, up, segments[i]);
    }

    VectorCopy(start, segments[0]);
    VectorCopy(end, segments[i]);

    if (gl_beamstyle->integer) {
        GL_DrawPolyBeam(segments, num_segments, color, width);
    } else {
        for (i = 0; i < num_segments; i++)
            GL_DrawSimpleBeam(segments[i], segments[i + 1], color, width);
    }
}

void GL_DrawBeams(void)
{
    vec3_t segs[2];
    color_t color;
    float width, scale;
    const entity_t *ent;
    int i;

    if (!glr.num_beams)
        return;

    GL_LoadMatrix(glr.viewmatrix);

    if (gl_beamstyle->integer) {
        GL_BindArrays(VA_NULLMODEL);
        scale = 0.5f;
    } else {
        GL_BindArrays(VA_EFFECT);
        scale = 1.2f;
    }

    for (i = 0, ent = glr.fd.entities; i < glr.fd.num_entities; i++, ent++) {
        if (!(ent->flags & RF_BEAM))
            continue;
        if (!ent->frame)
            continue;

        VectorCopy(ent->origin, segs[0]);
        VectorCopy(ent->oldorigin, segs[1]);

        if (ent->skinnum == -1)
            color.u32 = ent->rgba.u32;
        else
            color.u32 = d_8to24table[ent->skinnum & 0xff];
        color.u8[3] *= ent->alpha;

        width = abs((int16_t)ent->frame) * scale;

        if (ent->flags & RF_GLOW)
            GL_DrawLightningBeam(segs[0], segs[1], color, width);
        else if (gl_beamstyle->integer)
            GL_DrawPolyBeam(segs, 1, color, width);
        else
            GL_DrawSimpleBeam(segs[0], segs[1], color, width);
    }

    GL_FlushBeamSegments();
}

static void GL_FlushFlares(void)
{
    if (!tess.numindices)
        return;

    GL_BindTexture(TMU_TEXTURE, tess.texnum[TMU_TEXTURE]);
    GL_StateBits(GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_FALSE | GLS_BLEND_ADD | tess.flags);
    GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);
    GL_DrawIndexed(SHOWTRIS_FX);

    tess.numverts = tess.numindices = 0;
    tess.texnum[TMU_TEXTURE] = 0;
    tess.flags = 0;
}

void GL_DrawFlares(void)
{
    static const byte indices[12] = { 0, 2, 3, 0, 3, 4, 0, 4, 1, 0, 1, 2 };
    static const float tcoords[10] = { 0.5f, 0.5f, 0, 1, 0, 0, 1, 0, 1, 1 };
    vec3_t up, down, left, right;
    color_t inner, outer;
    vec_t *dst_vert;
    glIndex_t *dst_indices;
    GLuint result;
    const entity_t *ent;
    const image_t *image;
    glquery_t *q;
    float scale;
    bool def;
    int i, j;

    if (!glr.num_flares)
        return;
    if (!gl_static.queries)
        return;

    GL_LoadMatrix(glr.viewmatrix);
    GL_BindArrays(VA_EFFECT);

    for (i = 0, ent = glr.fd.entities; i < glr.fd.num_entities; i++, ent++) {
        if (!(ent->flags & RF_FLARE))
            continue;

        q = HashMap_Lookup(glquery_t, gl_static.queries, &ent->skinnum);
        if (!q)
            continue;

        if (q->pending && q->timestamp != com_eventTime) {
            if (gl_config.caps & QGL_CAP_QUERY_RESULT_NO_WAIT) {
                result = -1;
                qglGetQueryObjectuiv(q->query, GL_QUERY_RESULT_NO_WAIT, &result);
                if (result != -1) {
                    q->visible = result;
                    q->pending = false;
                }
            } else {
                qglGetQueryObjectuiv(q->query, GL_QUERY_RESULT_AVAILABLE, &result);
                if (result) {
                    qglGetQueryObjectuiv(q->query, GL_QUERY_RESULT, &result);
                    q->visible = result;
                    q->pending = false;
                }
            }
        }

        GL_AdvanceValue(&q->frac, q->visible, gl_flarespeed->value);
        if (!q->frac)
            continue;

        image = IMG_ForHandle(ent->skin);

        if (q_unlikely(tess.numverts + 5 > TESS_MAX_VERTICES ||
                       tess.numindices + 12 > TESS_MAX_INDICES) ||
            (tess.numindices && tess.texnum[TMU_TEXTURE] != image->texnum))
            GL_FlushFlares();

        tess.texnum[TMU_TEXTURE] = image->texnum;

        def = image->flags & IF_DEFAULT_FLARE;
        if (def)
            tess.flags |= GLS_DEFAULT_FLARE;

        scale = (25 << def) * (ent->scale * q->frac);

        if (ent->flags & RF_FLARE_LOCK_ANGLE) {
            VectorScale(glr.viewaxis[1],  scale, left);
            VectorScale(glr.viewaxis[1], -scale, right);
            VectorScale(glr.viewaxis[2], -scale, down);
            VectorScale(glr.viewaxis[2],  scale, up);
        } else {
            vec3_t dir, r, u;
            VectorSubtract(ent->origin, glr.fd.vieworg, dir);
            VectorNormalize(dir);
            MakeNormalVectors(dir, r, u);
            VectorScale(r, -scale, left);
            VectorScale(r,  scale, right);
            VectorScale(u, -scale, down);
            VectorScale(u,  scale, up);
        }

        dst_vert = tess.vertices + tess.numverts * 6;

        VectorCopy(ent->origin, dst_vert);
        VectorAdd3(ent->origin, down, left,  dst_vert +  6);
        VectorAdd3(ent->origin, up,   left,  dst_vert + 12);
        VectorAdd3(ent->origin, up,   right, dst_vert + 18);
        VectorAdd3(ent->origin, down, right, dst_vert + 24);

        for (j = 0; j < 5; j++) {
            dst_vert[j * 6 + 3] = tcoords[j * 2 + 0];
            dst_vert[j * 6 + 4] = tcoords[j * 2 + 1];
        }

        inner.u32 = ent->rgba.u32;
        inner.u8[3] = (128 + def * 32) * (ent->alpha * q->frac);
        outer.u32 = inner.u32;

        if (ent->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE)) {
            VectorClear(outer.u8);
            if (ent->flags & RF_SHELL_RED)
                outer.u8[0] = 255;
            if (ent->flags & RF_SHELL_GREEN)
                outer.u8[1] = 255;
            if (ent->flags & RF_SHELL_BLUE)
                outer.u8[2] = 255;
            tess.flags |= GLS_SHADE_SMOOTH;
        }

        WN32(dst_vert +  5, inner.u32);
        WN32(dst_vert + 11, outer.u32);
        WN32(dst_vert + 17, outer.u32);
        WN32(dst_vert + 23, outer.u32);
        WN32(dst_vert + 29, outer.u32);

        dst_indices = tess.indices + tess.numindices;
        for (j = 0; j < 12; j++)
            dst_indices[j] = tess.numverts + indices[j];

        tess.numverts += 5;
        tess.numindices += 12;
    }

    GL_FlushFlares();
}

#define ATTR_FLOAT(a, b, c) { a, false, b * sizeof(GLfloat), c * sizeof(GLfloat) }
#define ATTR_UBYTE(a, b, c) { a, true,  b * sizeof(GLfloat), c * sizeof(GLfloat) }

static const glVaDesc_t arraydescs[VA_TOTAL][VERT_ATTR_COUNT] = {
    [VA_SPRITE] = {
        [VERT_ATTR_POS] = ATTR_FLOAT(3, 5, 0),
        [VERT_ATTR_TC]  = ATTR_FLOAT(2, 5, 3),
    },
    [VA_EFFECT] = {
        [VERT_ATTR_POS]   = ATTR_FLOAT(3, 6, 0),
        [VERT_ATTR_TC]    = ATTR_FLOAT(2, 6, 3),
        [VERT_ATTR_COLOR] = ATTR_UBYTE(4, 6, 5),
    },
    [VA_NULLMODEL] = {
        [VERT_ATTR_POS]   = ATTR_FLOAT(3, 4, 0),
        [VERT_ATTR_COLOR] = ATTR_UBYTE(4, 4, 3),
    },
    [VA_OCCLUDE] = {
        [VERT_ATTR_POS] = ATTR_FLOAT(3, 3, 0),
    },
    [VA_WATERWARP] = {
        [VERT_ATTR_POS] = ATTR_FLOAT(2, 4, 0),
        [VERT_ATTR_TC]  = ATTR_FLOAT(2, 4, 2),
    },
    [VA_MESH_SHADE] = {
        [VERT_ATTR_POS]   = ATTR_FLOAT(3, VERTEX_SIZE, 0),
        [VERT_ATTR_COLOR] = ATTR_FLOAT(4, VERTEX_SIZE, 4),
    },
    [VA_MESH_FLAT] = {
        [VERT_ATTR_POS] = ATTR_FLOAT(3, 4, 0),
    },
    [VA_2D] = {
        [VERT_ATTR_POS]   = ATTR_FLOAT(2, 5, 0),
        [VERT_ATTR_TC]    = ATTR_FLOAT(2, 5, 2),
        [VERT_ATTR_COLOR] = ATTR_UBYTE(4, 5, 4),
    },
    [VA_3D] = {
        [VERT_ATTR_POS]   = ATTR_FLOAT(3, VERTEX_SIZE, 0),
        [VERT_ATTR_TC]    = ATTR_FLOAT(2, VERTEX_SIZE, 4),
        [VERT_ATTR_LMTC]  = ATTR_FLOAT(2, VERTEX_SIZE, 6),
        [VERT_ATTR_COLOR] = ATTR_UBYTE(4, VERTEX_SIZE, 3),
    },
};

void GL_BindArrays(glVertexArray_t va)
{
    const GLfloat *ptr = tess.vertices;
    GLuint buffer = 0;

    if (gls.currentva == va)
        return;

    if (va == VA_3D && !gl_static.world.vertices) {
        buffer = gl_static.world.buffer;
        ptr = NULL;
    } else if (!(gl_config.caps & QGL_CAP_CLIENT_VA)) {
        buffer = gl_static.vertex_buffer;
        ptr = NULL;
    }

    GL_BindBuffer(GL_ARRAY_BUFFER, buffer);
    gl_backend->array_pointers(arraydescs[va], ptr);

    gls.currentva = va;
    c.vertexArrayBinds++;
}

void GL_LockArrays(GLsizei count)
{
    if (gls.currentva == VA_3D && !gl_static.world.vertices)
        return;
    if (gl_config.caps & QGL_CAP_CLIENT_VA) {
        if (qglLockArraysEXT)
            qglLockArraysEXT(0, count);
    } else {
        const glVaDesc_t *desc = &arraydescs[gls.currentva][VERT_ATTR_POS];
        GL_BindBuffer(GL_ARRAY_BUFFER, gl_static.vertex_buffer);
        qglBufferData(GL_ARRAY_BUFFER, count * desc->stride, tess.vertices, GL_STREAM_DRAW);
    }
}

void GL_UnlockArrays(void)
{
    if (gls.currentva == VA_3D && !gl_static.world.vertices)
        return;
    if (!(gl_config.caps & QGL_CAP_CLIENT_VA))
        return;
    if (qglUnlockArraysEXT)
        qglUnlockArraysEXT();
}

void GL_DrawIndexed(showtris_t showtris)
{
    const glIndex_t *indices = tess.indices;

    GL_LockArrays(tess.numverts);

    if (!(gl_config.caps & QGL_CAP_CLIENT_VA)) {
        GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_static.index_buffer);
        qglBufferData(GL_ELEMENT_ARRAY_BUFFER, tess.numindices * sizeof(indices[0]), indices, GL_STREAM_DRAW);
        indices = NULL;
    }

    GL_DrawTriangles(tess.numindices, indices);
    c.trisDrawn += tess.numindices / 3;

    if (gl_showtris->integer & showtris)
        GL_DrawOutlines(tess.numindices, indices, true);

    GL_UnlockArrays();
}

void GL_InitArrays(void)
{
    if (gl_config.caps & QGL_CAP_CLIENT_VA)
        return;

    qglGenVertexArrays(1, &gl_static.array_object);
    qglBindVertexArray(gl_static.array_object);

    qglGenBuffers(1, &gl_static.index_buffer);
    qglGenBuffers(1, &gl_static.vertex_buffer);
}

void GL_ShutdownArrays(void)
{
    if (gl_config.caps & QGL_CAP_CLIENT_VA)
        return;

    qglDeleteVertexArrays(1, &gl_static.array_object);
    qglDeleteBuffers(1, &gl_static.index_buffer);
    qglDeleteBuffers(1, &gl_static.vertex_buffer);
}

void GL_Flush3D(void)
{
    glStateBits_t state = tess.flags;
    glArrayBits_t array = GLA_VERTEX | GLA_TC;

    if (!tess.numindices)
        return;

    if (q_unlikely(state & GLS_SKY_MASK)) {
        array = GLA_VERTEX;
    } else if (q_likely(tess.texnum[TMU_LIGHTMAP])) {
        state |= GLS_LIGHTMAP_ENABLE;
        array |= GLA_LMTC;

        if (q_unlikely(gl_lightmap->integer))
            state &= ~GLS_INTENSITY_ENABLE;

        if (tess.texnum[TMU_GLOWMAP])
            state |= GLS_GLOWMAP_ENABLE;
    }

    if (!(state & GLS_TEXTURE_REPLACE))
        array |= GLA_COLOR;

    GL_StateBits(state);
    GL_ArrayBits(array);

    if (state & GLS_DEFAULT_SKY) {
        GL_BindCubemap(tess.texnum[TMU_TEXTURE]);
    } else if (qglBindTextures) {
#if USE_DEBUG
        if (q_unlikely(gl_nobind->integer))
            tess.texnum[TMU_TEXTURE] = TEXNUM_DEFAULT;
#endif
        int count = 0;
        for (int i = 0; i < MAX_TMUS && tess.texnum[i]; i++) {
            if (gls.texnums[i] != tess.texnum[i]) {
                gls.texnums[i] = tess.texnum[i];
                count = i + 1;
                c.texSwitches++;
            }
        }
        if (count)
            qglBindTextures(0, count, tess.texnum);
    } else {
        for (int i = 0; i < MAX_TMUS && tess.texnum[i]; i++)
            GL_BindTexture(i, tess.texnum[i]);
    }

    GL_DrawIndexed(SHOWTRIS_WORLD);

    c.batchesDrawn++;

    memset(tess.texnum, 0, sizeof(tess.texnum));
    tess.numindices = 0;
    tess.numverts = 0;
    tess.flags = 0;
}

static int GL_CopyVerts(const mface_t *surf)
{
    int firstvert;

    if (tess.numverts + surf->numsurfedges > TESS_MAX_VERTICES)
        GL_Flush3D();

    memcpy(tess.vertices + tess.numverts * VERTEX_SIZE,
           gl_static.world.vertices + surf->firstvert * VERTEX_SIZE,
           surf->numsurfedges * VERTEX_SIZE * sizeof(GLfloat));

    firstvert = tess.numverts;
    tess.numverts += surf->numsurfedges;
    return firstvert;
}

static const image_t *GL_TextureAnimation(const mtexinfo_t *tex)
{
    if (q_unlikely(tex->next)) {
        int c = glr.ent->frame % tex->numframes;
        while (c--)
            tex = tex->next;
    }

    return tex->image;
}

static void GL_DrawFace(const mface_t *surf)
{
    const image_t *image = GL_TextureAnimation(surf->texinfo);
    const int numtris = surf->numsurfedges - 2;
    const int numindices = numtris * 3;
    glStateBits_t state = surf->statebits;
    GLuint texnum[MAX_TMUS] = { 0 };
    glIndex_t *dst_indices;
    int i, j;

    texnum[TMU_TEXTURE] = image->texnum;
    if (q_likely(surf->light_m)) {
        texnum[TMU_LIGHTMAP] = lm.texnums[surf->light_m - lm.lightmaps];
        texnum[TMU_GLOWMAP ] = image->texnum2;

        if (q_unlikely(gl_lightmap->integer)) {
            texnum[TMU_TEXTURE] = TEXNUM_WHITE;
            texnum[TMU_GLOWMAP] = 0;
        }
    } else if (state & GLS_CLASSIC_SKY) {
        if (q_likely(gl_drawsky->integer)) {
            texnum[TMU_LIGHTMAP] = image->texnum2;
        } else {
            texnum[TMU_TEXTURE ] = TEXNUM_BLACK;
            state &= ~GLS_CLASSIC_SKY;
        }
    }

    if (memcmp(tess.texnum, texnum, sizeof(texnum)) ||
        tess.flags != state ||
        tess.numindices + numindices > TESS_MAX_INDICES)
        GL_Flush3D();

    if (q_unlikely(gl_static.world.vertices))
        j = GL_CopyVerts(surf);
    else
        j = surf->firstvert;

    dst_indices = tess.indices + tess.numindices;
    for (i = 0; i < numtris; i++) {
        dst_indices[0] = j;
        dst_indices[1] = j + (i + 1);
        dst_indices[2] = j + (i + 2);
        dst_indices += 3;
    }
    tess.numindices += numindices;

    memcpy(tess.texnum, texnum, sizeof(texnum));
    tess.flags = state;

    c.facesTris += numtris;
    c.facesDrawn++;
}

void GL_ClearSolidFaces(void)
{
    for (int i = 0; i < FACE_HASH_SIZE; i++)
        faces_next[i] = &faces_head[i];
}

void GL_DrawSolidFaces(void)
{
    for (int i = 0; i < FACE_HASH_SIZE; i++) {
        for (const mface_t *face = faces_head[i]; face; face = face->next)
            GL_DrawFace(face);
        faces_head[i] = NULL;
    }
}

void GL_DrawAlphaFaces(void)
{
    if (!faces_alpha)
        return;

    glr.ent = NULL;

    GL_BindArrays(VA_3D);

    for (const mface_t *face = faces_alpha; face; face = face->next) {
        if (glr.ent != face->entity) {
            glr.ent = face->entity;
            GL_Flush3D();
            GL_SetEntityAxis();
            GL_RotateForEntity(glr.ent == &gl_world ?
                               gl_static.use_cubemaps :
                               gl_static.use_bmodel_skies);
        }
        GL_DrawFace(face);
    }

    faces_alpha = NULL;

    GL_Flush3D();
}

void GL_AddSolidFace(mface_t *face)
{
    // preserve front-to-back ordering
    face->next = NULL;
    *faces_next[face->hash] = face;
    faces_next[face->hash] = &face->next;
}

void GL_AddAlphaFace(mface_t *face)
{
    // draw back-to-front
    face->entity = glr.ent;
    face->next = faces_alpha;
    faces_alpha = face;
}
