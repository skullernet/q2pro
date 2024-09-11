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

static float    skyrotate;
static bool     skyautorotate;
static vec3_t   skyaxis;
static GLuint   sky_images[6];

static const vec3_t skyclip[6] = {
    { 1, 1, 0 },
    { 1, -1, 0 },
    { 0, -1, 1 },
    { 0, 1, 1 },
    { 1, 0, 1 },
    { -1, 0, 1 }
};

// 1 = s, 2 = t, 3 = 2048
static const int8_t st_to_vec[6][3] = {
    { 3, -1, 2 },
    { -3, 1, 2 },

    { 1, 3, 2 },
    { -1, -3, 2 },

    { -2, -1, 3 },  // 0 degrees yaw, look straight up
    { 2, -1, -3 }   // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int8_t vec_to_st[6][3] = {
    { -2, 3, 1 },
    { 2, 3, -1 },

    { 1, 3, 2 },
    { -1, 3, -2 },

    { -2, -1, 3 },
    { -2, 1, -3 }
};

static vec3_t       skymatrix[3];
static float        skymins[2][6], skymaxs[2][6];
static int          skyfaces;
static const float  sky_min = 1.0f / 512.0f;
static const float  sky_max = 511.0f / 512.0f;

static void DrawSkyPolygon(int nump, const vec3_t vecs)
{
    int         i, j;
    vec3_t      v, av;
    float       s, t, dv;
    int         axis;
    const float *vp;

    // decide which face it maps to
    VectorClear(v);
    for (i = 0, vp = vecs; i < nump; i++, vp += 3)
        VectorAdd(vp, v, v);

    av[0] = fabsf(v[0]);
    av[1] = fabsf(v[1]);
    av[2] = fabsf(v[2]);
    if (av[0] > av[1] && av[0] > av[2]) {
        if (v[0] < 0)
            axis = 1;
        else
            axis = 0;
    } else if (av[1] > av[2] && av[1] > av[0]) {
        if (v[1] < 0)
            axis = 3;
        else
            axis = 2;
    } else {
        if (v[2] < 0)
            axis = 5;
        else
            axis = 4;
    }

    // project new texture coords
    for (i = 0; i < nump; i++, vecs += 3) {
        j = vec_to_st[axis][2];
        if (j > 0)
            dv = vecs[j - 1];
        else
            dv = -vecs[-j - 1];
        if (dv < 0.001f)
            continue;    // don't divide by zero
        j = vec_to_st[axis][0];
        if (j < 0)
            s = -vecs[-j - 1] / dv;
        else
            s = vecs[j - 1] / dv;
        j = vec_to_st[axis][1];
        if (j < 0)
            t = -vecs[-j - 1] / dv;
        else
            t = vecs[j - 1] / dv;

        if (s < skymins[0][axis])
            skymins[0][axis] = s;
        if (t < skymins[1][axis])
            skymins[1][axis] = t;
        if (s > skymaxs[0][axis])
            skymaxs[0][axis] = s;
        if (t > skymaxs[1][axis])
            skymaxs[1][axis] = t;
    }
}

#define ON_EPSILON      0.1f    // point on plane side epsilon
#define MAX_CLIP_VERTS  64

#define SIDE_FRONT      0
#define SIDE_BACK       1
#define SIDE_ON         2

static void ClipSkyPolygon(int nump, vec3_t vecs, int stage)
{
    const float *v, *norm;
    bool        front, back;
    float       d, e;
    float       dists[MAX_CLIP_VERTS];
    int         sides[MAX_CLIP_VERTS];
    vec3_t      newv[2][MAX_CLIP_VERTS];
    int         newc[2];
    int         i, j;

    if (nump > MAX_CLIP_VERTS - 2) {
        Com_DPrintf("%s: too many verts\n", __func__);
        return;
    }

    if (stage == 6) {
        // fully clipped, so draw it
        DrawSkyPolygon(nump, vecs);
        return;
    }

    front = back = false;
    norm = skyclip[stage];
    for (i = 0, v = vecs; i < nump; i++, v += 3) {
        d = DotProduct(v, norm);
        if (d > ON_EPSILON) {
            front = true;
            sides[i] = SIDE_FRONT;
        } else if (d < -ON_EPSILON) {
            back = true;
            sides[i] = SIDE_BACK;
        } else {
            sides[i] = SIDE_ON;
        }
        dists[i] = d;
    }

    if (!front || !back) {
        // not clipped
        ClipSkyPolygon(nump, vecs, stage + 1);
        return;
    }

    // clip it
    sides[i] = sides[0];
    dists[i] = dists[0];
    VectorCopy(vecs, (vecs + (i * 3)));
    newc[0] = newc[1] = 0;

    for (i = 0, v = vecs; i < nump; i++, v += 3) {
        switch (sides[i]) {
        case SIDE_FRONT:
            VectorCopy(v, newv[0][newc[0]]);
            newc[0]++;
            break;
        case SIDE_BACK:
            VectorCopy(v, newv[1][newc[1]]);
            newc[1]++;
            break;
        case SIDE_ON:
            VectorCopy(v, newv[0][newc[0]]);
            newc[0]++;
            VectorCopy(v, newv[1][newc[1]]);
            newc[1]++;
            break;
        }

        if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
            continue;

        d = dists[i] / (dists[i] - dists[i + 1]);
        for (j = 0; j < 3; j++) {
            e = v[j] + d * (v[j + 3] - v[j]);
            newv[0][newc[0]][j] = e;
            newv[1][newc[1]][j] = e;
        }
        newc[0]++;
        newc[1]++;
    }

    // continue
    ClipSkyPolygon(newc[0], newv[0][0], stage + 1);
    ClipSkyPolygon(newc[1], newv[1][0], stage + 1);
}

static inline void SkyInverseRotate(vec3_t out, const vec3_t in)
{
    out[0] = skymatrix[0][0] * in[0] + skymatrix[1][0] * in[1] + skymatrix[2][0] * in[2];
    out[1] = skymatrix[0][1] * in[0] + skymatrix[1][1] * in[1] + skymatrix[2][1] * in[2];
    out[2] = skymatrix[0][2] * in[0] + skymatrix[1][2] * in[1] + skymatrix[2][2] * in[2];
}

/*
=================
R_AddSkySurface
=================
*/
void R_AddSkySurface(const mface_t *fa)
{
    int                 i;
    vec3_t              verts[MAX_CLIP_VERTS];
    vec3_t              temp;
    const msurfedge_t   *surfedge;
    const mvertex_t     *vert;
    const medge_t       *edge;
    const bsp_t         *bsp = gl_static.world.cache;

    if (fa->numsurfedges > MAX_CLIP_VERTS) {
        Com_DPrintf("%s: too many verts\n", __func__);
        return;
    }

    // calculate vertex values for sky box
    surfedge = fa->firstsurfedge;
    if (skyrotate) {
        if (!skyfaces && skyautorotate)
            SetupRotationMatrix(skymatrix, skyaxis, glr.fd.time * skyrotate);

        for (i = 0; i < fa->numsurfedges; i++, surfedge++) {
            edge = bsp->edges + surfedge->edge;
            vert = bsp->vertices + edge->v[surfedge->vert];
            VectorSubtract(vert->point, glr.fd.vieworg, temp);
            SkyInverseRotate(verts[i], temp);
        }
    } else {
        for (i = 0; i < fa->numsurfedges; i++, surfedge++) {
            edge = bsp->edges + surfedge->edge;
            vert = bsp->vertices + edge->v[surfedge->vert];
            VectorSubtract(vert->point, glr.fd.vieworg, verts[i]);
        }
    }

    ClipSkyPolygon(fa->numsurfedges, verts[0], 0);
    skyfaces++;
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        skymins[0][i] = skymins[1][i] = 9999;
        skymaxs[0][i] = skymaxs[1][i] = -9999;
    }

    skyfaces = 0;
}

static void MakeSkyVec(float s, float t, int axis, vec_t *out)
{
    vec3_t  b, v;
    int     j, k;

    b[0] = s * gl_static.world.size;
    b[1] = t * gl_static.world.size;
    b[2] = gl_static.world.size;

    for (j = 0; j < 3; j++) {
        k = st_to_vec[axis][j];
        if (k < 0)
            v[j] = -b[-k - 1];
        else
            v[j] = b[k - 1];
    }

    if (skyrotate) {
        out[0] = DotProduct(skymatrix[0], v) + glr.fd.vieworg[0];
        out[1] = DotProduct(skymatrix[1], v) + glr.fd.vieworg[1];
        out[2] = DotProduct(skymatrix[2], v) + glr.fd.vieworg[2];
    } else {
        VectorAdd(v, glr.fd.vieworg, out);
    }

    // avoid bilerp seam
    s = Q_clipf((s + 1) * 0.5f, sky_min, sky_max);
    t = Q_clipf((t + 1) * 0.5f, sky_min, sky_max);

    out[3] = s;
    out[4] = 1.0f - t;
}

/*
==============
R_DrawSkyBox
==============
*/
void R_DrawSkyBox(void)
{
    int i;

    // check for no sky at all
    if (!skyfaces)
        return; // nothing visible

    GL_BindArrays(VA_SPRITE);
    GL_StateBits(GLS_TEXTURE_REPLACE);
    GL_ArrayBits(GLA_VERTEX | GLA_TC);

    for (i = 0; i < 6; i++) {
        if (skymins[0][i] >= skymaxs[0][i] ||
            skymins[1][i] >= skymaxs[1][i])
            continue;

        GL_BindTexture(TMU_TEXTURE, sky_images[i]);

        MakeSkyVec(skymaxs[0][i], skymins[1][i], i, tess.vertices);
        MakeSkyVec(skymins[0][i], skymins[1][i], i, tess.vertices +  5);
        MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i, tess.vertices + 10);
        MakeSkyVec(skymins[0][i], skymaxs[1][i], i, tess.vertices + 15);

        GL_LockArrays(4);
        qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GL_UnlockArrays();
    }
}

static void DefaultSkyMatrix(GLfloat *matrix)
{
    if (skyautorotate) {
        SetupRotationMatrix(skymatrix, skyaxis, glr.fd.time * skyrotate);
        TransposeAxis(skymatrix);
    }

    matrix[ 0] = skymatrix[0][0];
    matrix[ 4] = skymatrix[0][1];
    matrix[ 8] = skymatrix[0][2];
    matrix[12] = -DotProduct(skymatrix[0], glr.fd.vieworg);

    matrix[ 1] = skymatrix[2][0];
    matrix[ 5] = skymatrix[2][1];
    matrix[ 9] = skymatrix[2][2];
    matrix[13] = -DotProduct(skymatrix[2], glr.fd.vieworg);

    matrix[ 2] = skymatrix[1][0];
    matrix[ 6] = skymatrix[1][1];
    matrix[10] = skymatrix[1][2];
    matrix[14] = -DotProduct(skymatrix[1], glr.fd.vieworg);

    matrix[ 3] = 0;
    matrix[ 7] = 0;
    matrix[11] = 0;
    matrix[15] = 1;
}

// classic skies don't rotate
static void ClassicSkyMatrix(GLfloat *matrix)
{
    matrix[ 0] = 1;
    matrix[ 4] = 0;
    matrix[ 8] = 0;
    matrix[12] = -glr.fd.vieworg[0];

    matrix[ 1] = 0;
    matrix[ 5] = 1;
    matrix[ 9] = 0;
    matrix[13] = -glr.fd.vieworg[1];

    matrix[ 2] = 0;
    matrix[ 6] = 0;
    matrix[10] = 3;
    matrix[14] = -glr.fd.vieworg[2] * 3;

    matrix[ 3] = 0;
    matrix[ 7] = 0;
    matrix[11] = 0;
    matrix[15] = 1;
}

/*
============
R_RotateForSky
============
*/
void R_RotateForSky(void)
{
    if (!gl_static.use_cubemaps)
        return;

    DefaultSkyMatrix(glr.skymatrix[0]);
    ClassicSkyMatrix(glr.skymatrix[1]);
}

static void R_UnsetSky(void)
{
    int i;

    skyrotate = 0;
    skyautorotate = false;
    for (i = 0; i < 6; i++)
        sky_images[i] = TEXNUM_BLACK;

    R_SKYTEXTURE->texnum = TEXNUM_CUBEMAP_BLACK;
}

/*
============
R_SetSky
============
*/
void R_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis)
{
    int             i;
    char            pathname[MAX_QPATH];
    const image_t   *image;
    imageflags_t    flags = IF_NONE;

    if (!gl_drawsky->integer) {
        R_UnsetSky();
        return;
    }

    skyrotate = rotate;
    skyautorotate = autorotate;
    VectorNormalize2(axis, skyaxis);
    SetupRotationMatrix(skymatrix, skyaxis, skyrotate);
    if (gl_static.use_cubemaps)
        TransposeAxis(skymatrix);
    if (!skyrotate)
        skyautorotate = false;

    // try to load cubemap image first
    if (gl_static.use_cubemaps) {
        if (Q_concat(pathname, sizeof(pathname), "sky/", name, ".tga") >= sizeof(pathname)) {
            R_UnsetSky();
            return;
        }
        image = IMG_Find(pathname, IT_SKY, IF_CUBEMAP);
        if (image != R_SKYTEXTURE) {
            R_SKYTEXTURE->texnum = image->texnum;
            return;
        }
        R_SKYTEXTURE->texnum = TEXNUM_CUBEMAP_DEFAULT;
        flags = IF_CUBEMAP | IF_TURBULENT;  // hack for IMG_Load()
    }

    // load legacy skybox
    for (i = 0; i < 6; i++) {
        if (Q_concat(pathname, sizeof(pathname), "env/", name,
                     com_env_suf[i], ".tga") >= sizeof(pathname)) {
            R_UnsetSky();
            return;
        }
        image = IMG_Find(pathname, IT_SKY, flags);
        if (image == R_SKYTEXTURE) {
            R_UnsetSky();
            return;
        }
        sky_images[i] = image->texnum;
    }
}
