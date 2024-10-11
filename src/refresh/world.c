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

void GL_SampleLightPoint(vec3_t color)
{
    const mface_t       *surf = glr.lightpoint.surf;
    const byte          *lightmap;
    const byte          *b1, *b2, *b3, *b4;
    const lightstyle_t  *style;
    float               fracu, fracv;
    float               w1, w2, w3, w4;
    vec3_t              temp;
    int                 s, t, smax, tmax, size;

    s = glr.lightpoint.s;
    t = glr.lightpoint.t;

    fracu = glr.lightpoint.s - s;
    fracv = glr.lightpoint.t - t;

    // compute weights of lightmap blocks
    w1 = (1.0f - fracu) * (1.0f - fracv);
    w2 = fracu * (1.0f - fracv);
    w3 = fracu * fracv;
    w4 = (1.0f - fracu) * fracv;

    smax = surf->lm_width;
    tmax = surf->lm_height;
    size = smax * tmax * 3;

    VectorClear(color);

    // add all the lightmaps with bilinear filtering
    lightmap = surf->lightmap;
    for (int i = 0; i < surf->numstyles; i++) {
        b1 = &lightmap[3 * ((t + 0) * smax + (s + 0))];
        b2 = &lightmap[3 * ((t + 0) * smax + (s + 1))];
        b3 = &lightmap[3 * ((t + 1) * smax + (s + 1))];
        b4 = &lightmap[3 * ((t + 1) * smax + (s + 0))];

        temp[0] = w1 * b1[0] + w2 * b2[0] + w3 * b3[0] + w4 * b4[0];
        temp[1] = w1 * b1[1] + w2 * b2[1] + w3 * b3[1] + w4 * b4[1];
        temp[2] = w1 * b1[2] + w2 * b2[2] + w3 * b3[2] + w4 * b4[2];

        style = LIGHT_STYLE(surf->styles[i]);
        VectorMA(color, style->white, temp, color);

        lightmap += size;
    }
}

static bool GL_LightGridPoint(const lightgrid_t *grid, const vec3_t start, vec3_t color)
{
    vec3_t point, avg;
    uint32_t point_i[3];
    vec3_t samples[8];
    int i, j, mask, numsamples;

    if (!grid->numleafs || !gl_lightgrid->integer)
        return false;

    point[0] = (start[0] - grid->mins[0]) * grid->scale[0];
    point[1] = (start[1] - grid->mins[1]) * grid->scale[1];
    point[2] = (start[2] - grid->mins[2]) * grid->scale[2];

    VectorCopy(point, point_i);
    VectorClear(avg);

    for (i = mask = numsamples = 0; i < 8; i++) {
        uint32_t tmp[3];

        tmp[0] = point_i[0] + ((i >> 0) & 1);
        tmp[1] = point_i[1] + ((i >> 1) & 1);
        tmp[2] = point_i[2] + ((i >> 2) & 1);

        const lightgrid_sample_t *s = BSP_LookupLightgrid(grid, tmp);
        if (!s)
            continue;

        VectorClear(samples[i]);

        for (j = 0; j < grid->numstyles && s->style != 255; j++, s++) {
            const lightstyle_t *style = LIGHT_STYLE(s->style);
            VectorMA(samples[i], style->white, s->rgb, samples[i]);
        }

        // count non-occluded samples
        if (j) {
            mask |= BIT(i);
            VectorAdd(avg, samples[i], avg);
            numsamples++;
        }
    }

    if (!mask)
        return false;

    // replace occluded samples with average
    if (mask != 255) {
        VectorScale(avg, 1.0f / numsamples, avg);
        for (i = 0; i < 8; i++)
            if (!(mask & BIT(i)))
                VectorCopy(avg, samples[i]);
    }

    // trilinear interpolation
    float fx, fy, fz;
    float bx, by, bz;
    vec3_t lerp_x[4];
    vec3_t lerp_y[2];

    fx = point[0] - point_i[0];
    fy = point[1] - point_i[1];
    fz = point[2] - point_i[2];

    bx = 1.0f - fx;
    by = 1.0f - fy;
    bz = 1.0f - fz;

    LerpVector2(samples[0], samples[1], bx, fx, lerp_x[0]);
    LerpVector2(samples[2], samples[3], bx, fx, lerp_x[1]);
    LerpVector2(samples[4], samples[5], bx, fx, lerp_x[2]);
    LerpVector2(samples[6], samples[7], bx, fx, lerp_x[3]);

    LerpVector2(lerp_x[0], lerp_x[1], by, fy, lerp_y[0]);
    LerpVector2(lerp_x[2], lerp_x[3], by, fy, lerp_y[1]);

    LerpVector2(lerp_y[0], lerp_y[1], bz, fz, color);

    GL_AdjustColor(color);

    return true;
}

static bool GL_LightPoint_(const vec3_t start, vec3_t color)
{
    const bsp_t     *bsp = gl_static.world.cache;
    int             i, index;
    lightpoint_t    pt;
    vec3_t          end, mins, maxs;
    const entity_t  *ent;
    const mmodel_t  *model;
    const vec_t     *angles;

    if (!bsp || !bsp->lightmap)
        return false;

    end[0] = start[0];
    end[1] = start[1];
    end[2] = start[2] - 8192;

    // get base lightpoint from world
    BSP_LightPoint(&glr.lightpoint, start, end, bsp->nodes, gl_static.nolm_mask);

    // trace to other BSP models
    for (i = 0, ent = glr.fd.entities; i < glr.fd.num_entities; i++, ent++) {
        index = ent->model;
        if (!(index & BIT(31)))
            break;  // BSP models are at the start of entity array

        index = ~index;
        if (index < 1 || index >= bsp->nummodels)
            continue;

        model = &bsp->models[index];
        if (!model->numfaces)
            continue;

        // cull in X/Y plane
        if (!VectorEmpty(ent->angles)) {
            if (fabsf(start[0] - ent->origin[0]) > model->radius)
                continue;
            if (fabsf(start[1] - ent->origin[1]) > model->radius)
                continue;
            angles = ent->angles;
        } else {
            VectorAdd(model->mins, ent->origin, mins);
            VectorAdd(model->maxs, ent->origin, maxs);
            if (start[0] < mins[0] || start[0] > maxs[0])
                continue;
            if (start[1] < mins[1] || start[1] > maxs[1])
                continue;
            angles = NULL;
        }

        BSP_TransformedLightPoint(&pt, start, end, model->headnode,
                                  gl_static.nolm_mask, ent->origin, angles);

        if (pt.fraction < glr.lightpoint.fraction)
            glr.lightpoint = pt;
    }

    if (GL_LightGridPoint(&bsp->lightgrid, start, color))
        return true;

    if (!glr.lightpoint.surf)
        return false;

    GL_SampleLightPoint(color);

    GL_AdjustColor(color);

    return true;
}

static void GL_MarkLights_r(const mnode_t *node, const dlight_t *light, uint64_t lightbit)
{
    mface_t *face;
    vec_t dot;
    int i;

    while (node->plane) {
        dot = PlaneDiffFast(light->transformed, node->plane);
        if (dot > light->intensity - DLIGHT_CUTOFF) {
            node = node->children[0];
            continue;
        }
        if (dot < -light->intensity + DLIGHT_CUTOFF) {
            node = node->children[1];
            continue;
        }

        for (i = 0, face = node->firstface; i < node->numfaces; i++, face++) {
            if (face->drawflags & gl_static.nolm_mask)
                continue;
            if (face->dlightframe != glr.dlightframe) {
                face->dlightframe = glr.dlightframe;
                face->dlightbits = 0;
            }
            face->dlightbits |= lightbit;
        }

        GL_MarkLights_r(node->children[0], light, lightbit);
        node = node->children[1];
    }
}

static void GL_MarkLights(void)
{
    int i;
    dlight_t *light;

    glr.dlightframe++;

    for (i = 0, light = glr.fd.dlights; i < glr.fd.num_dlights; i++, light++) {
        VectorCopy(light->origin, light->transformed);
        GL_MarkLights_r(gl_static.world.cache->nodes, light, BIT_ULL(i));
    }
}

static void GL_TransformLights(const mmodel_t *model)
{
    int i;
    dlight_t *light;
    vec3_t temp;

    glr.dlightframe++;

    for (i = 0, light = glr.fd.dlights; i < glr.fd.num_dlights; i++, light++) {
        VectorSubtract(light->origin, glr.ent->origin, temp);
        VectorRotate(temp, glr.entaxis, light->transformed);
        GL_MarkLights_r(model->headnode, light, BIT_ULL(i));
    }
}

static void GL_AddLights(const vec3_t origin, vec3_t color)
{
    dlight_t *light;
    vec_t f;
    int i;

    for (i = 0, light = glr.fd.dlights; i < glr.fd.num_dlights; i++, light++) {
        f = light->intensity - DLIGHT_CUTOFF - Distance(light->origin, origin);
        if (f > 0) {
            f *= (1.0f / 255);
            VectorMA(color, f, light->color, color);
        }
    }
}

void GL_LightPoint(const vec3_t origin, vec3_t color)
{
    if (gl_fullbright->integer) {
        VectorSet(color, 1, 1, 1);
        return;
    }

    // get lighting from world
    if (!GL_LightPoint_(origin, color))
        VectorSet(color, 1, 1, 1);

    // add dynamic lights
    GL_AddLights(origin, color);

    if (gl_doublelight_entities->integer) {
        // apply modulate twice to mimic original ref_gl behavior
        VectorScale(color, gl_static.entity_modulate, color);
    }
}

void R_LightPoint(const vec3_t origin, vec3_t color)
{
    GL_LightPoint(origin, color);

    color[0] = Q_clipf(color[0], 0, 1);
    color[1] = Q_clipf(color[1], 0, 1);
    color[2] = Q_clipf(color[2], 0, 1);
}

static void GL_MarkLeaves(void)
{
    const bsp_t *bsp = gl_static.world.cache;
    byte vis1[VIS_MAX_BYTES];
    byte vis2[VIS_MAX_BYTES];
    const mleaf_t *leaf;
    int i, cluster1, cluster2;
    vec3_t tmp;

    if (gl_lockpvs->integer)
        return;

    leaf = BSP_PointLeaf(bsp->nodes, glr.fd.vieworg);
    cluster1 = cluster2 = leaf->cluster;
    VectorCopy(glr.fd.vieworg, tmp);
    if (!leaf->contents)
        tmp[2] -= 16;
    else
        tmp[2] += 16;
    leaf = BSP_PointLeaf(bsp->nodes, tmp);
    if (!(leaf->contents & CONTENTS_SOLID))
        cluster2 = leaf->cluster;

    if (cluster1 == glr.viewcluster1 && cluster2 == glr.viewcluster2)
        return;

    glr.visframe++;
    glr.viewcluster1 = cluster1;
    glr.viewcluster2 = cluster2;

    if (!bsp->vis || gl_novis->integer || cluster1 == -1) {
        // mark everything visible
        for (i = 0; i < bsp->numnodes; i++)
            bsp->nodes[i].visframe = glr.visframe;

        for (i = 0; i < bsp->numleafs; i++)
            bsp->leafs[i].visframe = glr.visframe;

        glr.nodes_visible = bsp->numnodes;
        return;
    }

    BSP_ClusterVis(bsp, vis1, cluster1, DVIS_PVS);
    if (cluster1 != cluster2) {
        BSP_ClusterVis(bsp, vis2, cluster2, DVIS_PVS);
        int longs = VIS_FAST_LONGS(bsp);
        size_t *src1 = (size_t *)vis1;
        size_t *src2 = (size_t *)vis2;
        while (longs--)
            *src1++ |= *src2++;
    }

    glr.nodes_visible = 0;
    for (i = 0, leaf = bsp->leafs; i < bsp->numleafs; i++, leaf++) {
        cluster1 = leaf->cluster;
        if (cluster1 == -1)
            continue;
        if (!Q_IsBitSet(vis1, cluster1))
            continue;
        // mark parent nodes visible
        for (mnode_t *node = (mnode_t *)leaf; node && node->visframe != glr.visframe; node = node->parent) {
            node->visframe = glr.visframe;
            glr.nodes_visible++;
        }
    }
}

#define BACKFACE_EPSILON    0.01f

void GL_DrawBspModel(mmodel_t *model)
{
    mface_t *face;
    vec3_t bounds[2];
    vec_t dot;
    vec3_t transformed, temp;
    entity_t *ent = glr.ent;
    glCullResult_t cull;
    glStateBits_t skymask;
    int i;

    if (!model->numfaces)
        return;

    if (glr.entrotated) {
        cull = GL_CullSphere(ent->origin, model->radius);
        if (cull == CULL_OUT) {
            c.spheresCulled++;
            return;
        }
        if (cull == CULL_CLIP) {
            VectorCopy(model->mins, bounds[0]);
            VectorCopy(model->maxs, bounds[1]);
            cull = GL_CullLocalBox(ent->origin, bounds);
            if (cull == CULL_OUT) {
                c.rotatedBoxesCulled++;
                return;
            }
        }
        VectorSubtract(glr.fd.vieworg, ent->origin, temp);
        VectorRotate(temp, glr.entaxis, transformed);
    } else {
        VectorAdd(model->mins, ent->origin, bounds[0]);
        VectorAdd(model->maxs, ent->origin, bounds[1]);
        cull = GL_CullBox(bounds);
        if (cull == CULL_OUT) {
            c.boxesCulled++;
            return;
        }
        VectorSubtract(glr.fd.vieworg, ent->origin, transformed);
    }

    GL_TransformLights(model);

    GL_RotateForEntity(gl_static.use_bmodel_skies);

    skymask = gl_static.use_bmodel_skies ? GLS_SKY_MASK : 0;

    GL_BindArrays(VA_3D);

    GL_ClearSolidFaces();

    // draw visible faces
    for (i = 0, face = model->firstface; i < model->numfaces; i++, face++) {
        // sky faces don't have their polygon built
        if (face->drawflags & SURF_SKY && !(face->statebits & skymask))
            continue;
        if (face->drawflags & SURF_NODRAW)
            continue;

        dot = PlaneDiffFast(transformed, face->plane);
        if ((face->drawflags & DSURF_PLANEBACK) ? (dot > BACKFACE_EPSILON) : (dot < -BACKFACE_EPSILON)) {
            c.facesCulled++;
            continue;
        }

        if (gl_dynamic->integer)
            GL_PushLights(face);

        if (face->drawflags & SURF_TRANS_MASK) {
            if (model->drawframe != glr.drawframe)
                GL_AddAlphaFace(face);
            continue;
        }

        GL_AddSolidFace(face);
    }

    if (gl_dynamic->integer)
        GL_UploadLightmaps();

    GL_DrawSolidFaces();

    GL_Flush3D();

    // protect against infinite loop if the same inline model
    // with alpha faces is referenced by multiple entities
    model->drawframe = glr.drawframe;
}

#define NODE_CLIPPED    0
#define NODE_UNCLIPPED  MASK(4)

static inline bool GL_ClipNode(const mnode_t *node, int *clipflags)
{
    int flags = *clipflags;
    box_plane_t bits;

    if (flags == NODE_UNCLIPPED)
        return true;

    for (int i = 0, mask = 1; i < 4; i++, mask <<= 1) {
        if (flags & mask)
            continue;
        bits = BoxOnPlaneSide(node->mins, node->maxs,
                              &glr.frustumPlanes[i]);
        if (bits == BOX_BEHIND)
            return false;
        if (bits == BOX_INFRONT)
            flags |= mask;
    }

    *clipflags = flags;
    return true;
}

static inline void GL_DrawLeaf(const mleaf_t *leaf)
{
    if (leaf->contents == CONTENTS_SOLID)
        return; // solid leaf

    if (glr.fd.areabits && !Q_IsBitSet(glr.fd.areabits, leaf->area))
        return; // door blocks sight

    for (int i = 0; i < leaf->numleaffaces; i++)
        leaf->firstleafface[i]->drawframe = glr.drawframe;

    c.leavesDrawn++;
}

static inline void GL_DrawNode(const mnode_t *node)
{
    mface_t *face;
    int i;

    for (i = 0, face = node->firstface; i < node->numfaces; i++, face++) {
        if (face->drawframe != glr.drawframe)
            continue;

        if (face->drawflags & SURF_SKY && !(face->statebits & GLS_SKY_MASK)) {
            R_AddSkySurface(face);
            continue;
        }

        if (face->drawflags & SURF_NODRAW)
            continue;

        if (gl_dynamic->integer)
            GL_PushLights(face);

        if (face->drawflags & SURF_TRANS_MASK)
            GL_AddAlphaFace(face);
        else
            GL_AddSolidFace(face);
    }

    c.nodesDrawn++;
}

static void GL_WorldNode_r(const mnode_t *node, int clipflags)
{
    int side;
    vec_t dot;

    while (node->visframe == glr.visframe) {
        if (!GL_ClipNode(node, &clipflags)) {
            c.nodesCulled++;
            break;
        }

        if (!node->plane) {
            GL_DrawLeaf((const mleaf_t *)node);
            break;
        }

        dot = PlaneDiffFast(glr.fd.vieworg, node->plane);
        side = dot < 0;

        GL_WorldNode_r(node->children[side], clipflags);

        GL_DrawNode(node);

        node = node->children[side ^ 1];
    }
}

void GL_DrawWorld(void)
{
    // auto cycle the world frame for texture animation
    gl_world.frame = (int)(glr.fd.time * 2);

    glr.ent = &gl_world;

    GL_MarkLeaves();

    GL_MarkLights();

    R_ClearSkyBox();

    GL_LoadMatrix(glr.viewmatrix);

    GL_BindArrays(VA_3D);

    GL_ClearSolidFaces();

    GL_WorldNode_r(gl_static.world.cache->nodes,
                   gl_cull_nodes->integer ? NODE_CLIPPED : NODE_UNCLIPPED);

    if (gl_dynamic->integer)
        GL_UploadLightmaps();

    GL_DrawSolidFaces();

    GL_Flush3D();

    R_DrawSkyBox();
}
