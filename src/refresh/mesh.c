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

static int      oldframenum;
static int      newframenum;
static float    frontlerp;
static float    backlerp;
static vec3_t   origin;
static vec3_t   oldscale;
static vec3_t   newscale;
static vec3_t   translate;
static vec_t    shellscale;
static vec4_t   color;

static vec3_t   shadedir;
static bool     dotshading;

static float    celscale;

static GLfloat  shadowmatrix[16];

#if USE_MD5
static md5_joint_t  temp_skeleton[MD5_MAX_JOINTS];
#endif

static void setup_dotshading(void)
{
    float cp, cy, sp, sy;
    vec_t yaw;

    dotshading = false;

    if (!gl_dotshading->integer)
        return;

    if (glr.ent->flags & RF_SHELL_MASK)
        return;

    dotshading = true;

    // matches the anormtab.h precalculations
    yaw = -DEG2RAD(glr.ent->angles[YAW]);
    cy = cos(yaw);
    sy = sin(yaw);
    cp = cos(-M_PI / 4);
    sp = sin(-M_PI / 4);
    shadedir[0] = cp * cy;
    shadedir[1] = cp * sy;
    shadedir[2] = -sp;
}

static inline vec_t shadedot(const vec3_t normal)
{
    vec_t d = DotProduct(normal, shadedir);

    // matches the anormtab.h precalculations
    if (d < 0) {
        d *= 0.3f;
    }

    return d + 1;
}

static inline vec_t *get_static_normal(vec3_t normal, const maliasvert_t *vert)
{
    unsigned int lat = vert->norm[0];
    unsigned int lng = vert->norm[1];

    normal[0] = TAB_SIN(lat) * TAB_COS(lng);
    normal[1] = TAB_SIN(lat) * TAB_SIN(lng);
    normal[2] = TAB_COS(lat);

    return normal;
}

static void tess_static_shell(const maliasmesh_t *mesh)
{
    const maliasvert_t *src_vert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;

    while (count--) {
        get_static_normal(normal, src_vert);

        dst_vert[0] = normal[0] * shellscale +
                      src_vert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = normal[1] * shellscale +
                      src_vert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = normal[2] * shellscale +
                      src_vert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_vert++;
    }
}

static void tess_static_shade(const maliasmesh_t *mesh)
{
    const maliasvert_t *src_vert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;

    while (count--) {
        vec_t d = shadedot(get_static_normal(normal, src_vert));

        dst_vert[0] = src_vert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = src_vert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = src_vert->pos[2] * newscale[2] + translate[2];
        dst_vert[4] = color[0] * d;
        dst_vert[5] = color[1] * d;
        dst_vert[6] = color[2] * d;
        dst_vert[7] = color[3];
        dst_vert += VERTEX_SIZE;

        src_vert++;
    }
}

static void tess_static_plain(const maliasmesh_t *mesh)
{
    const maliasvert_t *src_vert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;

    while (count--) {
        dst_vert[0] = src_vert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = src_vert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = src_vert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_vert++;
    }
}

static inline vec_t *get_lerped_normal(vec3_t normal,
                                       const maliasvert_t *oldvert,
                                       const maliasvert_t *newvert)
{
    vec3_t oldnorm, newnorm, tmp;
    vec_t scale;

    get_static_normal(oldnorm, oldvert);
    get_static_normal(newnorm, newvert);

    LerpVector2(oldnorm, newnorm, backlerp, frontlerp, tmp);

    // normalize result
    scale = 1.0f / VectorLength(tmp);
    VectorScale(tmp, scale, normal);

    return normal;
}

static void tess_lerped_shell(const maliasmesh_t *mesh)
{
    const maliasvert_t *src_oldvert = &mesh->verts[oldframenum * mesh->numverts];
    const maliasvert_t *src_newvert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;

    while (count--) {
        get_lerped_normal(normal, src_oldvert, src_newvert);

        dst_vert[0] = normal[0] * shellscale +
                      src_oldvert->pos[0] * oldscale[0] +
                      src_newvert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = normal[1] * shellscale +
                      src_oldvert->pos[1] * oldscale[1] +
                      src_newvert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = normal[2] * shellscale +
                      src_oldvert->pos[2] * oldscale[2] +
                      src_newvert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_oldvert++;
        src_newvert++;
    }
}

static void tess_lerped_shade(const maliasmesh_t *mesh)
{
    const maliasvert_t *src_oldvert = &mesh->verts[oldframenum * mesh->numverts];
    const maliasvert_t *src_newvert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;

    while (count--) {
        vec_t d = shadedot(get_lerped_normal(normal, src_oldvert, src_newvert));

        dst_vert[0] =
            src_oldvert->pos[0] * oldscale[0] +
            src_newvert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] =
            src_oldvert->pos[1] * oldscale[1] +
            src_newvert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] =
            src_oldvert->pos[2] * oldscale[2] +
            src_newvert->pos[2] * newscale[2] + translate[2];
        dst_vert[4] = color[0] * d;
        dst_vert[5] = color[1] * d;
        dst_vert[6] = color[2] * d;
        dst_vert[7] = color[3];
        dst_vert += VERTEX_SIZE;

        src_oldvert++;
        src_newvert++;
    }
}

static void tess_lerped_plain(const maliasmesh_t *mesh)
{
    const maliasvert_t *src_oldvert = &mesh->verts[oldframenum * mesh->numverts];
    const maliasvert_t *src_newvert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;

    while (count--) {
        dst_vert[0] =
            src_oldvert->pos[0] * oldscale[0] +
            src_newvert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] =
            src_oldvert->pos[1] * oldscale[1] +
            src_newvert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] =
            src_oldvert->pos[2] * oldscale[2] +
            src_newvert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_oldvert++;
        src_newvert++;
    }
}

static glCullResult_t cull_static_model(const model_t *model)
{
    const maliasframe_t *newframe = &model->frames[newframenum];
    vec3_t bounds[2];
    glCullResult_t cull;

    if (glr.ent->flags & RF_WEAPONMODEL) {
        cull = CULL_IN;
    } else if (glr.entrotated) {
        cull = GL_CullSphere(origin, newframe->radius);
        if (cull == CULL_OUT) {
            c.spheresCulled++;
            return cull;
        }
        if (cull == CULL_CLIP) {
            cull = GL_CullLocalBox(origin, newframe->bounds);
            if (cull == CULL_OUT) {
                c.rotatedBoxesCulled++;
                return cull;
            }
        }
    } else {
        VectorAdd(newframe->bounds[0], origin, bounds[0]);
        VectorAdd(newframe->bounds[1], origin, bounds[1]);
        cull = GL_CullBox(bounds);
        if (cull == CULL_OUT) {
            c.boxesCulled++;
            return cull;
        }
    }

    VectorCopy(newframe->scale, newscale);
    VectorCopy(newframe->translate, translate);

    return cull;
}

static glCullResult_t cull_lerped_model(const model_t *model)
{
    const maliasframe_t *newframe = &model->frames[newframenum];
    const maliasframe_t *oldframe = &model->frames[oldframenum];
    vec3_t bounds[2];
    glCullResult_t cull;

    if (glr.ent->flags & RF_WEAPONMODEL) {
        cull = CULL_IN;
    } else if (glr.entrotated) {
        cull = GL_CullSphere(origin, max(newframe->radius, oldframe->radius));
        if (cull == CULL_OUT) {
            c.spheresCulled++;
            return cull;
        }
        UnionBounds(newframe->bounds, oldframe->bounds, bounds);
        if (cull == CULL_CLIP) {
            cull = GL_CullLocalBox(origin, bounds);
            if (cull == CULL_OUT) {
                c.rotatedBoxesCulled++;
                return cull;
            }
        }
    } else {
        UnionBounds(newframe->bounds, oldframe->bounds, bounds);
        VectorAdd(bounds[0], origin, bounds[0]);
        VectorAdd(bounds[1], origin, bounds[1]);
        cull = GL_CullBox(bounds);
        if (cull == CULL_OUT) {
            c.boxesCulled++;
            return cull;
        }
    }

    VectorScale(oldframe->scale, backlerp, oldscale);
    VectorScale(newframe->scale, frontlerp, newscale);

    LerpVector2(oldframe->translate, newframe->translate,
                backlerp, frontlerp, translate);

    return cull;
}

static void setup_color(void)
{
    int flags = glr.ent->flags;
    float f, m;
    int i;

    memset(&glr.lightpoint, 0, sizeof(glr.lightpoint));

    if (flags & RF_SHELL_MASK) {
        VectorClear(color);
        if (flags & RF_SHELL_LITE_GREEN) {
            VectorSet(color, 0.56f, 0.93f, 0.56f);
        }
        if (flags & RF_SHELL_HALF_DAM) {
            VectorSet(color, 0.56f, 0.59f, 0.45f);
        }
        if (flags & RF_SHELL_DOUBLE) {
            color[0] = 0.9f;
            color[1] = 0.7f;
        }
        if (flags & RF_SHELL_RED) {
            color[0] = 1;
        }
        if (flags & RF_SHELL_GREEN) {
            color[1] = 1;
        }
        if (flags & RF_SHELL_BLUE) {
            color[2] = 1;
        }
    } else if (flags & RF_FULLBRIGHT) {
        VectorSet(color, 1, 1, 1);
    } else if ((flags & RF_IR_VISIBLE) && (glr.fd.rdflags & RDF_IRGOGGLES)) {
        VectorSet(color, 1, 0, 0);
    } else {
        GL_LightPoint(origin, color);

        if (flags & RF_MINLIGHT) {
            f = VectorLength(color);
            if (!f)
                VectorSet(color, 0.1f, 0.1f, 0.1f);
            else if (f < 0.1f)
                VectorScale(color, 0.1f / f, color);
        }

        if (flags & RF_GLOW) {
            f = 0.1f * sin(glr.fd.time * 7);
            for (i = 0; i < 3; i++) {
                m = color[i] * 0.8f;
                color[i] += f;
                if (color[i] < m)
                    color[i] = m;
            }
        }

        color[0] = Q_clipf(color[0], 0, 1);
        color[1] = Q_clipf(color[1], 0, 1);
        color[2] = Q_clipf(color[2], 0, 1);
    }

    if (flags & RF_TRANSLUCENT) {
        color[3] = glr.ent->alpha;
    } else {
        color[3] = 1;
    }
}

static void setup_celshading(void)
{
    float value = Cvar_ClampValue(gl_celshading, 0, 10);

    celscale = 0;

    if (value == 0)
        return;

    if (glr.ent->flags & (RF_TRANSLUCENT | RF_SHELL_MASK))
        return;

    if (!qglPolygonMode)
        return;

    celscale = 1.0f - Distance(origin, glr.fd.vieworg) / 700.0f;
}

static void draw_celshading(const QGL_INDEX_TYPE *indices, int num_indices)
{
    if (celscale < 0.01f || celscale > 1)
        return;

    GL_BindTexture(0, TEXNUM_BLACK);
    GL_StateBits(GLS_BLEND_BLEND);
    GL_ArrayBits(GLA_VERTEX);

    qglLineWidth(gl_celshading->value * celscale);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    qglCullFace(GL_FRONT);
    GL_Color(0, 0, 0, color[3] * celscale);
    qglDrawElements(GL_TRIANGLES, num_indices, QGL_INDEX_ENUM, indices);
    qglCullFace(GL_BACK);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglLineWidth(1);
}

static void proj_matrix(GLfloat *matrix, const cplane_t *plane, const vec3_t dir)
{
    matrix[0] = plane->normal[1] * dir[1] + plane->normal[2] * dir[2];
    matrix[4] = -plane->normal[1] * dir[0];
    matrix[8] = -plane->normal[2] * dir[0];
    matrix[12] = plane->dist * dir[0];

    matrix[1] = -plane->normal[0] * dir[1];
    matrix[5] = plane->normal[0] * dir[0] + plane->normal[2] * dir[2];
    matrix[9] = -plane->normal[2] * dir[1];
    matrix[13] = plane->dist * dir[1];

    matrix[2] = -plane->normal[0] * dir[2];
    matrix[6] = -plane->normal[1] * dir[2];
    matrix[10] = plane->normal[0] * dir[0] + plane->normal[1] * dir[1];
    matrix[14] = plane->dist * dir[2];

    matrix[3] = 0;
    matrix[7] = 0;
    matrix[11] = 0;
    matrix[15] = DotProduct(plane->normal, dir);
}

static void setup_shadow(void)
{
    GLfloat matrix[16], tmp[16];
    vec3_t dir;

    shadowmatrix[15] = 0;

    if (!gl_shadows->integer)
        return;

    if (glr.ent->flags & (RF_WEAPONMODEL | RF_NOSHADOW))
        return;

    if (!glr.lightpoint.surf)
        return;

    // position fake light source straight over the model
    if (glr.lightpoint.surf->drawflags & DSURF_PLANEBACK)
        VectorSet(dir, 0, 0, -1);
    else
        VectorSet(dir, 0, 0, 1);

    // project shadow on ground plane
    proj_matrix(matrix, &glr.lightpoint.plane, dir);
    GL_MultMatrix(tmp, glr.viewmatrix, matrix);

    // rotate for entity
    GL_RotationMatrix(matrix);
    GL_MultMatrix(shadowmatrix, tmp, matrix);
}

static void draw_shadow(const QGL_INDEX_TYPE *indices, int num_indices)
{
    if (shadowmatrix[15] < 0.5f)
        return;

    // load shadow projection matrix
    GL_LoadMatrix(shadowmatrix);

    // eliminate z-fighting by utilizing stencil buffer, if available
    if (gl_config.stencilbits) {
        qglEnable(GL_STENCIL_TEST);
        qglStencilFunc(GL_EQUAL, 0, 0xff);
        qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    }

    GL_StateBits(GLS_BLEND_BLEND);
    GL_BindTexture(0, TEXNUM_WHITE);
    GL_ArrayBits(GLA_VERTEX);

    qglEnable(GL_POLYGON_OFFSET_FILL);
    qglPolygonOffset(-1.0f, -2.0f);
    GL_Color(0, 0, 0, color[3] * 0.5f);
    qglDrawElements(GL_TRIANGLES, num_indices, QGL_INDEX_ENUM, indices);
    qglDisable(GL_POLYGON_OFFSET_FILL);

    // once we have drawn something to stencil buffer, continue to clear it for
    // the lifetime of OpenGL context. leaving stencil buffer "dirty" and
    // clearing just depth is slower (verified for Nvidia and ATI drivers).
    if (gl_config.stencilbits) {
        qglDisable(GL_STENCIL_TEST);
        gl_static.stencil_buffer_bit |= GL_STENCIL_BUFFER_BIT;
    }
}

static image_t *skin_for_mesh(image_t **skins, int num_skins)
{
    const entity_t *ent = glr.ent;

    if (ent->flags & RF_SHELL_MASK) {
        static image_t shell_texture;
        shell_texture.texnum = TEXNUM_WHITE;
        return &shell_texture;
    }

    if (ent->skin)
        return IMG_ForHandle(ent->skin);

    if (!num_skins)
        return R_NOTEXTURE;

    if (ent->skinnum < 0 || ent->skinnum >= num_skins) {
        Com_DPrintf("%s: no such skin: %d\n", "GL_DrawAliasModel", ent->skinnum);
        return skins[0];
    }

    if (skins[ent->skinnum] == R_NOTEXTURE)
        return skins[0];

    return skins[ent->skinnum];
}

static void draw_alias_mesh(const QGL_INDEX_TYPE *indices, int num_indices,
                            const maliastc_t *tcoords, int num_verts,
                            image_t **skins, int num_skins)
{
    glStateBits_t state = GLS_INTENSITY_ENABLE;
    image_t *skin = skin_for_mesh(skins, num_skins);

    // fall back to entity matrix
    GL_LoadMatrix(glr.entmatrix);

    // avoid drawing hidden faces for transparent gun
    if ((glr.ent->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_FULLBRIGHT)) == (RF_TRANSLUCENT | RF_WEAPONMODEL)) {
        GL_StateBits(GLS_DEFAULT);
        GL_ArrayBits(GLA_VERTEX);
        GL_BindTexture(0, TEXNUM_WHITE);
        GL_VertexPointer(3, dotshading ? VERTEX_SIZE : 4, tess.vertices);
        qglColorMask(0, 0, 0, 0);
        qglDrawElements(GL_TRIANGLES, num_indices, QGL_INDEX_ENUM, indices);
        qglColorMask(1, 1, 1, 1);
    }

    if (dotshading)
        state |= GLS_SHADE_SMOOTH;

    if (glr.ent->flags & RF_TRANSLUCENT)
        state |= GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE;

    if (skin->glow_texnum)
        state |= GLS_GLOWMAP_ENABLE;

    GL_StateBits(state);

    GL_BindTexture(0, skin->texnum);

    if (skin->glow_texnum)
        GL_BindTexture(2, skin->glow_texnum);

    if (dotshading) {
        GL_ArrayBits(GLA_VERTEX | GLA_TC | GLA_COLOR);
        GL_VertexPointer(3, VERTEX_SIZE, tess.vertices);
        GL_ColorFloatPointer(4, VERTEX_SIZE, tess.vertices + 4);
    } else {
        GL_ArrayBits(GLA_VERTEX | GLA_TC);
        GL_VertexPointer(3, 4, tess.vertices);
        GL_Color(color[0], color[1], color[2], color[3]);
    }

    GL_TexCoordPointer(2, 0, tcoords->st);

    GL_LockArrays(num_verts);

    qglDrawElements(GL_TRIANGLES, num_indices, QGL_INDEX_ENUM, indices);
    c.trisDrawn += num_indices / 3;

    draw_celshading(indices, num_indices);

    if (gl_showtris->integer & BIT(1)) {
        GL_DrawOutlines(num_indices, indices);
    }

    // FIXME: unlock arrays before changing matrix?
    draw_shadow(indices, num_indices);

    GL_UnlockArrays();
}

#if USE_MD5

// for the given vertex, set of weights & skeleton, calculate
// the output vertex (and optionally normal).
static inline void calc_skel_vert(const md5_vertex_t *vert,
                                  const md5_weight_t *weights,
                                  const md5_joint_t *skeleton,
                                  float *restrict out_position,
                                  float *restrict out_normal)
{
    VectorClear(out_position);

    if (out_normal)
        VectorClear(out_normal);

    for (int i = 0; i < vert->count; i++) {
        const md5_weight_t *weight = &weights[vert->start + i];
        const md5_joint_t *joint = &skeleton[weight->joint];

        vec3_t local_pos;
        VectorScale(weight->pos, joint->scale, local_pos);

        vec3_t wv;
        Quat_RotatePoint(joint->orient, local_pos, wv);

        VectorAdd(joint->pos, wv, wv);
        VectorMA(out_position, weight->bias, wv, out_position);

        if (out_normal) {
            Quat_RotatePoint(joint->orient, vert->normal, wv);
            VectorMA(out_normal, weight->bias, wv, out_normal);
        }
    }
}

static void tess_plain_skel(const md5_mesh_t *mesh, const md5_joint_t *skeleton)
{
    for (int i = 0; i < mesh->num_verts; i++)
        calc_skel_vert(&mesh->vertices[i], mesh->weights, skeleton, &tess.vertices[i * 4], NULL);
}

static void tess_shade_skel(const md5_mesh_t *mesh, const md5_joint_t *skeleton)
{
    vec_t *dst_vert = tess.vertices;

    for (int i = 0; i < mesh->num_verts; i++) {
        vec3_t normal;
        calc_skel_vert(&mesh->vertices[i], mesh->weights, skeleton, dst_vert, normal);

        vec_t d = shadedot(normal);
        dst_vert[4] = color[0] * d;
        dst_vert[5] = color[1] * d;
        dst_vert[6] = color[2] * d;
        dst_vert[7] = color[3];

        dst_vert += VERTEX_SIZE;
    }
}

static void tess_shell_skel(const md5_mesh_t *mesh, const md5_joint_t *skeleton)
{
    for (int i = 0; i < mesh->num_verts; i++) {
        vec3_t position, normal;
        calc_skel_vert(&mesh->vertices[i], mesh->weights, skeleton, position, normal);

        VectorMA(position, shellscale, normal, &tess.vertices[i * 4]);
    }
}

static void lerp_alias_skeleton(const md5_model_t *model)
{
    int frame_a = oldframenum % model->num_frames;
    int frame_b = newframenum % model->num_frames;
    const md5_joint_t *skel_a = &model->skeleton_frames[frame_a * model->num_joints];
    const md5_joint_t *skel_b = &model->skeleton_frames[frame_b * model->num_joints];

    for (int i = 0; i < model->num_joints; i++) {
        temp_skeleton[i].parent = skel_a[i].parent;
        temp_skeleton[i].scale = skel_b[i].scale;

        LerpVector2(skel_a[i].pos, skel_b[i].pos, backlerp, frontlerp, temp_skeleton[i].pos);
        Quat_SLerp(skel_a[i].orient, skel_b[i].orient, backlerp, frontlerp, temp_skeleton[i].orient);
    }
}

static void draw_skeleton_mesh(const md5_model_t *model, const md5_mesh_t *mesh, const md5_joint_t *skel)
{
    if (glr.ent->flags & RF_SHELL_MASK)
        tess_shell_skel(mesh, skel);
    else if (dotshading)
        tess_shade_skel(mesh, skel);
    else
        tess_plain_skel(mesh, skel);

    draw_alias_mesh(mesh->indices, mesh->num_indices,
                    mesh->tcoords, mesh->num_verts,
                    model->skins, model->num_skins);
}

static void draw_alias_skeleton(const md5_model_t *model)
{
    const md5_joint_t *skel = temp_skeleton;

    if (newframenum == oldframenum)
        skel = &model->skeleton_frames[newframenum % model->num_frames * model->num_joints];
    else
        lerp_alias_skeleton(model);

    for (int i = 0; i < model->num_meshes; i++)
        draw_skeleton_mesh(model, &model->meshes[i], skel);
}

#endif  // USE_MD5

// extra ugly. this needs to be done on the client, but to avoid complexity of
// rendering gun model in its own refdef, and to preserve compatibility with
// existing RF_WEAPONMODEL flag, we do it here.
static void setup_weaponmodel(void)
{
    extern cvar_t   *info_hand;
    extern cvar_t   *cl_adjustfov;
    extern cvar_t   *cl_gunfov;
    extern cvar_t   *cl_gun;

    float fov_x = glr.fd.fov_x;
    float fov_y = glr.fd.fov_y;
    float reflect_x = 1.0f;

    if (cl_gunfov->value > 0) {
        fov_x = Cvar_ClampValue(cl_gunfov, 30, 160);
        if (cl_adjustfov->integer) {
            fov_y = V_CalcFov(fov_x, 4, 3);
            fov_x = V_CalcFov(fov_y, glr.fd.height, glr.fd.width);
        } else {
            fov_y = V_CalcFov(fov_x, glr.fd.width, glr.fd.height);
        }
    }

    if ((info_hand->integer == 1 && cl_gun->integer == 1) || cl_gun->integer == 3) {
        reflect_x = -1.0f;
        qglFrontFace(GL_CCW);
    }

    GL_Frustum(fov_x, fov_y, reflect_x);
}

void GL_DrawAliasModel(const model_t *model)
{
    const entity_t *ent = glr.ent;
    glCullResult_t cull;
    void (*tessfunc)(const maliasmesh_t *);
    int i;

    newframenum = ent->frame;
    if (newframenum < 0 || newframenum >= model->numframes) {
        Com_DPrintf("%s: no such frame %d\n", __func__, newframenum);
        newframenum = 0;
    }

    oldframenum = ent->oldframe;
    if (oldframenum < 0 || oldframenum >= model->numframes) {
        Com_DPrintf("%s: no such oldframe %d\n", __func__, oldframenum);
        oldframenum = 0;
    }

    backlerp = ent->backlerp;
    frontlerp = 1.0f - backlerp;

    // optimized case
    if (backlerp == 0)
        oldframenum = newframenum;

    VectorCopy(ent->origin, origin);

    // cull the model, setup scale and translate vectors
    if (newframenum == oldframenum)
        cull = cull_static_model(model);
    else
        cull = cull_lerped_model(model);
    if (cull == CULL_OUT)
        return;

    // setup parameters common for all meshes
    setup_color();
    setup_celshading();
    setup_dotshading();
    setup_shadow();

    // select proper tessfunc
    if (ent->flags & RF_SHELL_MASK) {
        shellscale = (ent->flags & RF_WEAPONMODEL) ?
            WEAPONSHELL_SCALE : POWERSUIT_SCALE;
        tessfunc = newframenum == oldframenum ?
            tess_static_shell : tess_lerped_shell;
    } else if (dotshading) {
        tessfunc = newframenum == oldframenum ?
            tess_static_shade : tess_lerped_shade;
    } else {
        tessfunc = newframenum == oldframenum ?
            tess_static_plain : tess_lerped_plain;
    }

    GL_RotateForEntity();

    if (ent->flags & RF_WEAPONMODEL)
        setup_weaponmodel();

    if (ent->flags & RF_DEPTHHACK)
        GL_DepthRange(0, 0.25f);

    // draw all the meshes
#if USE_MD5
    if (model->skeleton && gl_md5_use->integer)
        draw_alias_skeleton(model->skeleton);
    else
#endif
    for (i = 0; i < model->nummeshes; i++) {
        const maliasmesh_t *mesh = &model->meshes[i];
        (*tessfunc)(mesh);
        draw_alias_mesh(mesh->indices, mesh->numindices,
                        mesh->tcoords, mesh->numverts,
                        mesh->skins, mesh->numskins);
    }

    if (ent->flags & RF_DEPTHHACK)
        GL_DepthRange(0, 1);

    if (ent->flags & RF_WEAPONMODEL) {
        GL_Frustum(glr.fd.fov_x, glr.fd.fov_y, 1.0f);
        qglFrontFace(GL_CW);
    }
}
