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

    GL_BindTexture(0, tess.texnum[0]);
    GL_StateBits(bits);
    GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);

    GL_VertexPointer(2, 4, tess.vertices);
    GL_TexCoordPointer(2, 4, tess.vertices + 2);
    GL_ColorBytePointer(4, 0, tess.colors);

    GL_LockArrays(tess.numverts);

    GL_DrawTriangles(tess.numindices, tess.indices);

    if (gl_showtris->integer & SHOWTRIS_PIC)
        GL_DrawOutlines(tess.numindices, tess.indices);

    GL_UnlockArrays();

    c.batchesDrawn2D++;

    tess.numindices = 0;
    tess.numverts = 0;
    tess.texnum[0] = 0;
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
    uint32_t *dst_color;
    glStateBits_t bits;

    if (!glr.fd.num_particles)
        return;

    GL_LoadMatrix(glr.viewmatrix);

    GL_VertexPointer(3, 5, tess.vertices);
    GL_TexCoordPointer(2, 5, tess.vertices + 3);
    GL_ColorBytePointer(4, 0, tess.colors);

    bits = (gl_partstyle->integer ? GLS_BLEND_ADD : GLS_BLEND_BLEND) | GLS_DEPTHMASK_FALSE;

    p = glr.fd.particles;
    total = glr.fd.num_particles;
    do {
        GL_BindTexture(0, TEXNUM_PARTICLE);
        GL_StateBits(bits);
        GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);

        count = min(total, TESS_MAX_VERTICES / 3);
        total -= count;

        dst_vert = tess.vertices;
        dst_color = (uint32_t *)tess.colors;
        numverts = count * 3;
        do {
            VectorSubtract(p->origin, glr.fd.vieworg, transformed);
            dist = DotProduct(transformed, glr.viewaxis[0]);

            scale = gl_partscale->value;
            if (dist > 20)
                scale += dist * 0.01f;
            scale2 = scale * PARTICLE_SCALE;

            VectorMA(p->origin, scale2, glr.viewaxis[1], dst_vert);
            VectorMA(dst_vert, -scale2, glr.viewaxis[2], dst_vert);
            VectorMA(dst_vert,  scale,  glr.viewaxis[2], dst_vert +  5);
            VectorMA(dst_vert, -scale,  glr.viewaxis[1], dst_vert + 10);

            dst_vert[ 3] = 0;               dst_vert[ 4] = 0;
            dst_vert[ 8] = 0;               dst_vert[ 9] = PARTICLE_SIZE;
            dst_vert[13] = PARTICLE_SIZE;   dst_vert[14] = 0;

            dst_vert += 15;

            if (p->color == -1)
                color.u32 = p->rgba.u32;
            else
                color.u32 = d_8to24table[p->color & 0xff];
            color.u8[3] *= p->alpha;

            dst_color[0] = color.u32;
            dst_color[1] = color.u32;
            dst_color[2] = color.u32;
            dst_color += 3;

            p++;
        } while (--count);

        GL_LockArrays(numverts);
        qglDrawArrays(GL_TRIANGLES, 0, numverts);

        if (gl_showtris->integer & SHOWTRIS_FX)
            GL_DrawOutlines(numverts, NULL);

        GL_UnlockArrays();
    } while (total);
}

static void GL_FlushBeamSegments(void)
{
    if (!tess.numindices)
        return;

    GL_BindTexture(0, TEXNUM_BEAM);
    GL_StateBits(GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE);
    GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);

    GL_LockArrays(tess.numverts);

    GL_DrawTriangles(tess.numindices, tess.indices);

    if (gl_showtris->integer & SHOWTRIS_FX)
        GL_DrawOutlines(tess.numindices, tess.indices);

    GL_UnlockArrays();

    tess.numverts = tess.numindices = 0;
}

static void GL_DrawBeamSegment(const vec3_t start, const vec3_t end, color_t color, float width)
{
    vec3_t d1, d2, d3;
    vec_t *dst_vert;
    uint32_t *dst_color;
    QGL_INDEX_TYPE *dst_indices;

    VectorSubtract(end, start, d1);
    VectorSubtract(glr.fd.vieworg, start, d2);
    CrossProduct(d1, d2, d3);
    if (VectorNormalize(d3) < 0.1f)
        return;
    VectorScale(d3, width, d3);

    if (q_unlikely(tess.numverts + 4 > TESS_MAX_VERTICES ||
                   tess.numindices + 6 > TESS_MAX_INDICES))
        GL_FlushBeamSegments();

    dst_vert = tess.vertices + tess.numverts * 5;
    VectorAdd(start, d3, dst_vert);
    VectorSubtract(start, d3, dst_vert + 5);
    VectorSubtract(end, d3, dst_vert + 10);
    VectorAdd(end, d3, dst_vert + 15);

    dst_vert[ 3] = 0; dst_vert[ 4] = 0;
    dst_vert[ 8] = 1; dst_vert[ 9] = 0;
    dst_vert[13] = 1; dst_vert[14] = 1;
    dst_vert[18] = 0; dst_vert[19] = 1;

    dst_color = (uint32_t *)tess.colors + tess.numverts;
    dst_color[0] = color.u32;
    dst_color[1] = color.u32;
    dst_color[2] = color.u32;
    dst_color[3] = color.u32;

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
    vec3_t d1, segments[MAX_LIGHTNING_SEGMENTS - 1];
    vec_t length, segment_length;
    int i, num_segments, max_segments;

    VectorSubtract(end, start, d1);
    length = VectorNormalize(d1);

    max_segments = length / MIN_SEGMENT_LENGTH;
    if (max_segments <= 1) {
        GL_DrawBeamSegment(start, end, color, width);
        return;
    }

    if (max_segments <= MIN_LIGHTNING_SEGMENTS) {
        num_segments = max_segments;
    } else {
        max_segments = min(max_segments, MAX_LIGHTNING_SEGMENTS);
        num_segments = MIN_LIGHTNING_SEGMENTS + GL_rand() % (max_segments - MIN_LIGHTNING_SEGMENTS + 1);
    }

    segment_length = length / num_segments;
    for (i = 0; i < num_segments - 1; i++) {
        int dir = GL_rand() % q_countof(bytedirs);
        float offs = GL_frand() * (segment_length * 0.5f);
        float dist = (i + 1) * segment_length;
        VectorMA(start, dist, d1, segments[i]);
        VectorMA(segments[i], offs, bytedirs[dir], segments[i]);
    }

    for (i = 0; i < num_segments; i++) {
        const float *seg_start = (i == 0) ? start : segments[i - 1];
        const float *seg_end = (i == num_segments - 1) ? end : segments[i];

        GL_DrawBeamSegment(seg_start, seg_end, color, width);
    }
}

void GL_DrawBeams(void)
{
    const vec_t *start, *end;
    color_t color;
    float width;
    const entity_t *ent;
    int i;

    if (!glr.num_beams)
        return;

    GL_LoadMatrix(glr.viewmatrix);

    GL_VertexPointer(3, 5, tess.vertices);
    GL_TexCoordPointer(2, 5, tess.vertices + 3);
    GL_ColorBytePointer(4, 0, tess.colors);

    for (i = 0, ent = glr.fd.entities; i < glr.fd.num_entities; i++, ent++) {
        if (!(ent->flags & RF_BEAM))
            continue;

        start = ent->origin;
        end = ent->oldorigin;

        if (ent->skinnum == -1)
            color.u32 = ent->rgba.u32;
        else
            color.u32 = d_8to24table[ent->skinnum & 0xff];
        color.u8[3] *= ent->alpha;

        width = abs((int16_t)ent->frame) * 1.2f;

        if (ent->flags & RF_GLOW)
            GL_DrawLightningBeam(start, end, color, width);
        else
            GL_DrawBeamSegment(start, end, color, width);
    }

    GL_FlushBeamSegments();
}

static void GL_FlushFlares(void)
{
    if (!tess.numindices)
        return;

    GL_BindTexture(0, tess.texnum[0]);
    GL_StateBits(GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_FALSE | GLS_BLEND_ADD);
    GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);

    GL_LockArrays(tess.numverts);

    GL_DrawTriangles(tess.numindices, tess.indices);

    if (gl_showtris->integer & SHOWTRIS_FX)
        GL_DrawOutlines(tess.numindices, tess.indices);

    GL_UnlockArrays();

    tess.numverts = tess.numindices = 0;
    tess.texnum[0] = 0;
}

void GL_DrawFlares(void)
{
    vec3_t up, down, left, right;
    color_t color;
    vec_t *dst_vert;
    uint32_t *dst_color;
    QGL_INDEX_TYPE *dst_indices;
    GLuint result, texnum;
    const entity_t *ent;
    glquery_t *q;
    float scale;
    int i;

    if (!glr.num_flares)
        return;
    if (!gl_static.queries)
        return;

    GL_LoadMatrix(glr.viewmatrix);

    GL_VertexPointer(3, 5, tess.vertices);
    GL_TexCoordPointer(2, 5, tess.vertices + 3);
    GL_ColorBytePointer(4, 0, tess.colors);

    for (i = 0, ent = glr.fd.entities; i < glr.fd.num_entities; i++, ent++) {
        if (!(ent->flags & RF_FLARE))
            continue;

        q = HashMap_Lookup(glquery_t, gl_static.queries, &ent->skinnum);
        if (!q)
            continue;

        if (q->pending) {
            qglGetQueryObjectuiv(q->query, GL_QUERY_RESULT_AVAILABLE, &result);
            if (result) {
                qglGetQueryObjectuiv(q->query, GL_QUERY_RESULT, &result);
                q->visible = result;
                q->pending = false;
            }
        }

        GL_AdvanceValue(&q->frac, q->visible, gl_flarespeed->value);
        if (!q->frac)
            continue;

        texnum = IMG_ForHandle(ent->skin)->texnum;

        if (q_unlikely(tess.numverts + 4 > TESS_MAX_VERTICES ||
                       tess.numindices + 6 > TESS_MAX_INDICES) ||
            (tess.numindices && tess.texnum[0] != texnum))
            GL_FlushFlares();

        tess.texnum[0] = texnum;

        scale = 25.0f * (ent->scale * q->frac);

        VectorScale(glr.viewaxis[1],  scale, left);
        VectorScale(glr.viewaxis[1], -scale, right);
        VectorScale(glr.viewaxis[2], -scale, down);
        VectorScale(glr.viewaxis[2],  scale, up);

        dst_vert = tess.vertices + tess.numverts * 5;

        VectorAdd3(ent->origin, down, left,  dst_vert);
        VectorAdd3(ent->origin, up,   left,  dst_vert +  5);
        VectorAdd3(ent->origin, up,   right, dst_vert + 10);
        VectorAdd3(ent->origin, down, right, dst_vert + 15);

        dst_vert[ 3] = 0; dst_vert[ 4] = 1;
        dst_vert[ 8] = 0; dst_vert[ 9] = 0;
        dst_vert[13] = 1; dst_vert[14] = 0;
        dst_vert[18] = 1; dst_vert[19] = 1;

        color.u32 = ent->rgba.u32;
        color.u8[3] = 128 * (ent->alpha * q->frac);

        dst_color = (uint32_t *)tess.colors + tess.numverts;
        dst_color[0] = color.u32;
        dst_color[1] = color.u32;
        dst_color[2] = color.u32;
        dst_color[3] = color.u32;

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

    GL_FlushFlares();
}

void GL_BindArrays(void)
{
    if (gl_static.world.vertices) {
        GL_VertexPointer(3, VERTEX_SIZE, tess.vertices);
        GL_ColorBytePointer(4, VERTEX_SIZE, (GLubyte *)(tess.vertices + 3));
        GL_TexCoordPointer(2, VERTEX_SIZE, tess.vertices + 4);
        if (lm.nummaps)
            GL_LightCoordPointer(2, VERTEX_SIZE, tess.vertices + 6);
    } else {
        qglBindBuffer(GL_ARRAY_BUFFER, gl_static.world.bufnum);

        GL_VertexPointer(3, VERTEX_SIZE, VBO_OFS(0));
        GL_ColorBytePointer(4, VERTEX_SIZE, VBO_OFS(3));
        GL_TexCoordPointer(2, VERTEX_SIZE, VBO_OFS(4));
        if (lm.nummaps)
            GL_LightCoordPointer(2, VERTEX_SIZE, VBO_OFS(6));

        qglBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void GL_Flush3D(void)
{
    glStateBits_t state = tess.flags;
    glArrayBits_t array = GLA_VERTEX | GLA_TC;

    if (!tess.numindices)
        return;

    if (q_likely(tess.texnum[1])) {
        state |= GLS_LIGHTMAP_ENABLE;
        array |= GLA_LMTC;

        if (q_unlikely(gl_lightmap->integer))
            state &= ~GLS_INTENSITY_ENABLE;

        if (tess.texnum[2])
            state |= GLS_GLOWMAP_ENABLE;
    }

    if (!(state & GLS_TEXTURE_REPLACE))
        array |= GLA_COLOR;

    GL_StateBits(state);
    GL_ArrayBits(array);

    if (qglBindTextures) {
        int count = 0;
        for (int i = 0; i < MAX_TMUS && tess.texnum[i]; i++) {
            if (gls.texnums[i] != tess.texnum[i]) {
                gls.texnums[i] = tess.texnum[i];
                count = i + 1;
            }
        }
        if (count)
            qglBindTextures(0, count, tess.texnum);
    } else {
        for (int i = 0; i < MAX_TMUS && tess.texnum[i]; i++)
            GL_BindTexture(i, tess.texnum[i]);
    }

    if (gl_static.world.vertices)
        GL_LockArrays(tess.numverts);

    GL_DrawTriangles(tess.numindices, tess.indices);

    if (gl_showtris->integer & SHOWTRIS_WORLD)
        GL_DrawOutlines(tess.numindices, tess.indices);

    if (gl_static.world.vertices)
        GL_UnlockArrays();

    c.batchesDrawn++;

    tess.texnum[0] = tess.texnum[1] = tess.texnum[2] = 0;
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
    if (q_unlikely(tex->next))
        for (int i = 0; i < glr.ent->frame % tex->numframes; i++)
            tex = tex->next;

    return tex->image;
}

static void GL_DrawFace(const mface_t *surf)
{
    int numtris = surf->numsurfedges - 2;
    int numindices = numtris * 3;
    GLuint texnum[MAX_TMUS];
    QGL_INDEX_TYPE *dst_indices;
    int i, j;

    if (q_unlikely(gl_lightmap->integer && surf->texnum[1])) {
        texnum[0] = TEXNUM_WHITE;
        texnum[2] = 0;
    } else {
        const image_t *tex = GL_TextureAnimation(surf->texinfo);
        texnum[0] = tex->texnum;
        texnum[2] = surf->texnum[1] ? tex->glow_texnum : 0;
    }
    texnum[1] = surf->texnum[1];

    if (tess.texnum[0] != texnum[0] ||
        tess.texnum[1] != texnum[1] ||
        tess.texnum[2] != texnum[2] ||
        tess.flags != surf->statebits ||
        tess.numindices + numindices > TESS_MAX_INDICES)
        GL_Flush3D();

    tess.texnum[0] = texnum[0];
    tess.texnum[1] = texnum[1];
    tess.texnum[2] = texnum[2];
    tess.flags = surf->statebits;

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

    c.trisDrawn += numtris;
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

    GL_BindArrays();

    for (const mface_t *face = faces_alpha; face; face = face->next) {
        if (glr.ent != face->entity) {
            glr.ent = face->entity;
            GL_Flush3D();
            GL_SetEntityAxis();
            GL_RotateForEntity();
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

void GL_AddAlphaFace(mface_t *face, entity_t *ent)
{
    // draw back-to-front
    face->entity = ent;
    face->next = faces_alpha;
    faces_alpha = face;
}
