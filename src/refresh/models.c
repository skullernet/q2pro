/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2008 Andrey Nazarov

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
#include "format/md2.h"
#if USE_MD3
#include "format/md3.h"
#endif
#include "format/sp2.h"

#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)

#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }
#define ENSURE(x, e)    if (!(x)) return e

// this used to be MAX_MODELS * 2, but not anymore. MAX_MODELS is 8192 now and
// half of it is implicitly reserved for inline BSP models.
#define MAX_RMODELS     MAX_MODELS

static model_t      r_models[MAX_RMODELS];
static int          r_numModels;

static model_t *MOD_Alloc(void)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            break;
        }
    }

    if (i == r_numModels) {
        if (r_numModels == MAX_RMODELS) {
            return NULL;
        }
        r_numModels++;
    }

    return model;
}

static model_t *MOD_Find(const char *name)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }
        if (!FS_pathcmp(model->name, name)) {
            return model;
        }
    }

    return NULL;
}

static void MOD_List_f(void)
{
    static const char types[4] = "FASE";
    int     i, count;
    model_t *model;
    size_t  bytes;

    Com_Printf("------------------\n");
    bytes = count = 0;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }
        Com_Printf("%c %8zu : %s\n", types[model->type],
                   model->hunk.mapped, model->name);
        bytes += model->hunk.mapped;
        count++;
    }
    Com_Printf("Total models: %d (out of %d slots)\n", count, r_numModels);
    Com_Printf("Total resident: %zu\n", bytes);
}

void MOD_FreeUnused(void)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }
        if (model->registration_sequence == registration_sequence) {
            // make sure it is paged in
            Com_PageInMemory(model->hunk.base, model->hunk.cursize);
        } else {
            // don't need this model
            Hunk_Free(&model->hunk);
            memset(model, 0, sizeof(*model));
        }
    }
}

void MOD_FreeAll(void)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }

        Hunk_Free(&model->hunk);
        memset(model, 0, sizeof(*model));
    }

    r_numModels = 0;
}

static void LittleBlock(void *out, const void *in, size_t size)
{
    memcpy(out, in, size);
#if USE_BIG_ENDIAN
    for (int i = 0; i < size / 4; i++)
        ((uint32_t *)out)[i] = LittleLong(((uint32_t *)out)[i]);
#endif
}

static int MOD_LoadSP2(model_t *model, const void *rawdata, size_t length)
{
    dsp2header_t header;
    dsp2frame_t *src_frame;
    mspriteframe_t *dst_frame;
    char buffer[SP2_MAX_FRAMENAME];
    int i;

    if (length < sizeof(header))
        return Q_ERR_FILE_TOO_SMALL;

    // byte swap the header
    LittleBlock(&header, rawdata, sizeof(header));

    if (header.ident != SP2_IDENT)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header.version != SP2_VERSION)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header.numframes < 1) {
        // empty models draw nothing
        model->type = MOD_EMPTY;
        return Q_ERR_SUCCESS;
    }
    if (header.numframes > SP2_MAX_FRAMES) {
        Com_SetLastError("too many frames");
        return Q_ERR_INVALID_FORMAT;
    }
    if (sizeof(dsp2header_t) + sizeof(dsp2frame_t) * header.numframes > length) {
        Com_SetLastError("frames out of bounds");
        return Q_ERR_INVALID_FORMAT;
    }

    Hunk_Begin(&model->hunk, sizeof(model->spriteframes[0]) * header.numframes);
    model->type = MOD_SPRITE;

    model->spriteframes = MOD_Malloc(sizeof(model->spriteframes[0]) * header.numframes);
    model->numframes = header.numframes;

    src_frame = (dsp2frame_t *)((byte *)rawdata + sizeof(dsp2header_t));
    dst_frame = model->spriteframes;
    for (i = 0; i < header.numframes; i++) {
        dst_frame->width = (int32_t)LittleLong(src_frame->width);
        dst_frame->height = (int32_t)LittleLong(src_frame->height);

        dst_frame->origin_x = (int32_t)LittleLong(src_frame->origin_x);
        dst_frame->origin_y = (int32_t)LittleLong(src_frame->origin_y);

        if (!Q_memccpy(buffer, src_frame->name, 0, sizeof(buffer))) {
            Com_WPrintf("%s has bad frame name\n", model->name);
            dst_frame->image = R_NOTEXTURE;
        } else {
            FS_NormalizePath(buffer);
            dst_frame->image = IMG_Find(buffer, IT_SPRITE, IF_NONE);
        }

        src_frame++;
        dst_frame++;
    }

    Hunk_End(&model->hunk);

    return Q_ERR_SUCCESS;
}

static const char *MOD_ValidateMD2(const dmd2header_t *header, size_t length)
{
    ENSURE(header->num_tris <= TESS_MAX_INDICES / 3, "too many tris");
    ENSURE(header->num_st <= INT_MAX / sizeof(dmd2stvert_t), "too many st");
    ENSURE(header->num_xyz <= MD2_MAX_VERTS, "too many xyz");
    ENSURE(header->num_frames <= MD2_MAX_FRAMES, "too many frames");
    ENSURE(header->num_skins <= MD2_MAX_SKINS, "too many skins");

    Q_assert(header->num_xyz);
    ENSURE(header->framesize >= sizeof(dmd2frame_t) + (header->num_xyz - 1) * sizeof(dmd2trivertx_t), "too small frame size");
    ENSURE(header->framesize <= MD2_MAX_FRAMESIZE, "too big frame size");

    ENSURE(header->ofs_tris + (uint64_t)header->num_tris * sizeof(dmd2triangle_t) <= length, "bad tris offset");
    ENSURE(header->ofs_st + (uint64_t)header->num_st * sizeof(dmd2stvert_t) <= length, "bad st offset");
    ENSURE(header->ofs_frames + (uint64_t)header->num_frames * header->framesize <= length, "bad frames offset");
    ENSURE(header->ofs_skins + (uint64_t)MD2_MAX_SKINNAME * header->num_skins <= length, "bad skins offset");

    ENSURE(!(header->ofs_tris % q_alignof(dmd2triangle_t)), "odd tris offset");
    ENSURE(!(header->ofs_st % q_alignof(dmd2stvert_t)), "odd st offset");
    ENSURE(!(header->ofs_frames % q_alignof(dmd2frame_t)), "odd frames offset");
    ENSURE(!(header->framesize % q_alignof(dmd2frame_t)), "odd frame size");

    ENSURE(header->skinwidth >= 1 && header->skinwidth <= MD2_MAX_SKINWIDTH, "bad skin width");
    ENSURE(header->skinheight >= 1 && header->skinheight <= MD2_MAX_SKINHEIGHT, "bad skin height");
    return NULL;
}

static int MOD_LoadMD2(model_t *model, const void *rawdata, size_t length)
{
    dmd2header_t    header;
    dmd2frame_t     *src_frame;
    dmd2trivertx_t  *src_vert;
    dmd2triangle_t  *src_tri;
    dmd2stvert_t    *src_tc;
    char            *src_skin;
    maliasframe_t   *dst_frame;
    maliasvert_t    *dst_vert;
    maliastc_t      *dst_tc;
    maliasmesh_t    *mesh;
    int             i, j, k, val, ret;
    uint16_t        remap[TESS_MAX_INDICES];
    uint16_t        vertIndices[TESS_MAX_INDICES];
    uint16_t        tcIndices[TESS_MAX_INDICES];
    uint16_t        finalIndices[TESS_MAX_INDICES];
    int             numverts, numindices;
    char            skinname[MAX_QPATH];
    vec_t           scale_s, scale_t;
    vec3_t          mins, maxs;
    const char      *err;

    if (length < sizeof(header)) {
        return Q_ERR_FILE_TOO_SMALL;
    }

    // byte swap the header
    LittleBlock(&header, rawdata, sizeof(header));

    // check ident and version
    if (header.ident != MD2_IDENT)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header.version != MD2_VERSION)
        return Q_ERR_UNKNOWN_FORMAT;

    // empty models draw nothing
    if (header.num_tris < 1 || header.num_st < 3 || header.num_xyz < 3 || header.num_frames < 1) {
        model->type = MOD_EMPTY;
        return Q_ERR_SUCCESS;
    }

    // validate the header
    err = MOD_ValidateMD2(&header, length);
    if (err) {
        Com_SetLastError(err);
        return Q_ERR_INVALID_FORMAT;
    }

    // load all triangle indices
    numindices = 0;
    src_tri = (dmd2triangle_t *)((byte *)rawdata + header.ofs_tris);
    for (i = 0; i < header.num_tris; i++) {
        for (j = 0; j < 3; j++) {
            uint16_t idx_xyz = LittleShort(src_tri->index_xyz[j]);
            uint16_t idx_st = LittleShort(src_tri->index_st[j]);

            // some broken models have 0xFFFF indices
            if (idx_xyz >= header.num_xyz || idx_st >= header.num_st) {
                break;
            }

            vertIndices[numindices + j] = idx_xyz;
            tcIndices[numindices + j] = idx_st;
        }
        if (j == 3) {
            // only count good triangles
            numindices += 3;
        }
        src_tri++;
    }

    if (numindices < 3) {
        Com_SetLastError("too few valid indices");
        return Q_ERR_INVALID_FORMAT;
    }

    for (i = 0; i < numindices; i++) {
        remap[i] = 0xFFFF;
    }

    // remap all triangle indices
    numverts = 0;
    src_tc = (dmd2stvert_t *)((byte *)rawdata + header.ofs_st);
    for (i = 0; i < numindices; i++) {
        if (remap[i] != 0xFFFF) {
            continue; // already remapped
        }

        for (j = i + 1; j < numindices; j++) {
            if (vertIndices[i] == vertIndices[j] &&
                (src_tc[tcIndices[i]].s == src_tc[tcIndices[j]].s &&
                 src_tc[tcIndices[i]].t == src_tc[tcIndices[j]].t)) {
                // duplicate vertex
                remap[j] = i;
                finalIndices[j] = numverts;
            }
        }

        // new vertex
        remap[i] = i;
        finalIndices[i] = numverts++;
    }

    if (numverts > TESS_MAX_VERTICES) {
        Com_SetLastError("too many verts");
        return Q_ERR_INVALID_FORMAT;
    }

    Hunk_Begin(&model->hunk, 0x400000);
    model->type = MOD_ALIAS;
    model->nummeshes = 1;
    model->numframes = header.num_frames;
    CHECK(model->meshes = MOD_Malloc(sizeof(model->meshes[0])));
    CHECK(model->frames = MOD_Malloc(header.num_frames * sizeof(model->frames[0])));

    mesh = model->meshes;
    mesh->numtris = numindices / 3;
    mesh->numindices = numindices;
    mesh->numverts = numverts;
    mesh->numskins = header.num_skins;
    CHECK(mesh->verts = MOD_Malloc(numverts * header.num_frames * sizeof(mesh->verts[0])));
    CHECK(mesh->tcoords = MOD_Malloc(numverts * sizeof(mesh->tcoords[0])));
    CHECK(mesh->indices = MOD_Malloc(numindices * sizeof(mesh->indices[0])));
    CHECK(mesh->skins = MOD_Malloc(header.num_skins * sizeof(mesh->skins[0])));

    if (mesh->numtris != header.num_tris) {
        Com_DPrintf("%s has %d bad triangles\n", model->name, header.num_tris - mesh->numtris);
    }

    // store final triangle indices
    for (i = 0; i < numindices; i++) {
        mesh->indices[i] = finalIndices[i];
    }

    // load all skins
    src_skin = (char *)rawdata + header.ofs_skins;
    for (i = 0; i < header.num_skins; i++) {
        if (!Q_memccpy(skinname, src_skin, 0, sizeof(skinname))) {
            ret = Q_ERR_STRING_TRUNCATED;
            goto fail;
        }
        FS_NormalizePath(skinname);
        mesh->skins[i] = IMG_Find(skinname, IT_SKIN, IF_NONE);
        src_skin += MD2_MAX_SKINNAME;
    }

    // load all tcoords
    src_tc = (dmd2stvert_t *)((byte *)rawdata + header.ofs_st);
    dst_tc = mesh->tcoords;
    scale_s = 1.0f / header.skinwidth;
    scale_t = 1.0f / header.skinheight;
    for (i = 0; i < numindices; i++) {
        if (remap[i] != i) {
            continue;
        }
        dst_tc[finalIndices[i]].st[0] =
            (int16_t)LittleShort(src_tc[tcIndices[i]].s) * scale_s;
        dst_tc[finalIndices[i]].st[1] =
            (int16_t)LittleShort(src_tc[tcIndices[i]].t) * scale_t;
    }

    // load all frames
    src_frame = (dmd2frame_t *)((byte *)rawdata + header.ofs_frames);
    dst_frame = model->frames;
    for (j = 0; j < header.num_frames; j++) {
        LittleVector(src_frame->scale, dst_frame->scale);
        LittleVector(src_frame->translate, dst_frame->translate);

        // load frame vertices
        ClearBounds(mins, maxs);
        for (i = 0; i < numindices; i++) {
            if (remap[i] != i) {
                continue;
            }
            src_vert = &src_frame->verts[vertIndices[i]];
            dst_vert = &mesh->verts[j * numverts + finalIndices[i]];

            dst_vert->pos[0] = src_vert->v[0];
            dst_vert->pos[1] = src_vert->v[1];
            dst_vert->pos[2] = src_vert->v[2];

            val = src_vert->lightnormalindex;
            if (val >= NUMVERTEXNORMALS) {
                dst_vert->norm[0] = 0;
                dst_vert->norm[1] = 0;
            } else {
                dst_vert->norm[0] = gl_static.latlngtab[val][0];
                dst_vert->norm[1] = gl_static.latlngtab[val][1];
            }

            for (k = 0; k < 3; k++) {
                val = dst_vert->pos[k];
                if (val < mins[k])
                    mins[k] = val;
                if (val > maxs[k])
                    maxs[k] = val;
            }
        }

        VectorVectorScale(mins, dst_frame->scale, mins);
        VectorVectorScale(maxs, dst_frame->scale, maxs);

        dst_frame->radius = RadiusFromBounds(mins, maxs);

        VectorAdd(mins, dst_frame->translate, dst_frame->bounds[0]);
        VectorAdd(maxs, dst_frame->translate, dst_frame->bounds[1]);

        src_frame = (dmd2frame_t *)((byte *)src_frame + header.framesize);
        dst_frame++;
    }

    Hunk_End(&model->hunk);
    return Q_ERR_SUCCESS;

fail:
    Hunk_Free(&model->hunk);
    return ret;
}

#if USE_MD3
static const char *MOD_ValidateMD3Mesh(const model_t *model, const dmd3mesh_t *header, size_t length)
{
    ENSURE(header->meshsize >= sizeof(*header) && header->meshsize <= length, "bad mesh size");
    ENSURE(!(header->meshsize % q_alignof(dmd3mesh_t)), "odd mesh size");

    ENSURE(header->num_verts >= 3, "too few verts");
    ENSURE(header->num_verts <= TESS_MAX_VERTICES, "too many verts");
    ENSURE(header->num_tris >= 1, "too few tris");
    ENSURE(header->num_tris <= TESS_MAX_INDICES / 3, "too many tris");
    ENSURE(header->num_skins <= MD3_MAX_SKINS, "too many skins");

    ENSURE(header->ofs_skins + (uint64_t)header->num_skins * sizeof(dmd3skin_t) <= length, "bad skins offset");
    ENSURE(header->ofs_verts + (uint64_t)header->num_verts * model->numframes * sizeof(dmd3vertex_t) <= length, "bad verts offset");
    ENSURE(header->ofs_tcs + (uint64_t)header->num_verts * sizeof(dmd3coord_t) <= length, "bad tcs offset");
    ENSURE(header->ofs_indexes + (uint64_t)header->num_tris * 3 * sizeof(uint32_t) <= length, "bad indexes offset");

    ENSURE(!(header->ofs_skins % q_alignof(dmd3skin_t)), "odd skins offset");
    ENSURE(!(header->ofs_verts % q_alignof(dmd3vertex_t)), "odd verts offset");
    ENSURE(!(header->ofs_tcs % q_alignof(dmd3coord_t)), "odd tcs offset");
    ENSURE(!(header->ofs_indexes & 3), "odd indexes offset");
    return NULL;
}

static int MOD_LoadMD3Mesh(model_t *model, maliasmesh_t *mesh,
                           const byte *rawdata, size_t length, size_t *offset_p)
{
    dmd3mesh_t      header;
    dmd3vertex_t    *src_vert;
    dmd3coord_t     *src_tc;
    dmd3skin_t      *src_skin;
    uint32_t        *src_idx;
    maliasvert_t    *dst_vert;
    maliastc_t      *dst_tc;
    QGL_INDEX_TYPE  *dst_idx;
    uint32_t        index;
    char            skinname[MAX_QPATH];
    int             i, j, k, ret;
    const char      *err;

    if (length < sizeof(header))
        return Q_ERR_UNEXPECTED_EOF;

    // byte swap the header
    LittleBlock(&header, rawdata, sizeof(header));

    // validate the header
    err = MOD_ValidateMD3Mesh(model, &header, length);
    if (err) {
        Com_SetLastError(err);
        return Q_ERR_INVALID_FORMAT;
    }

    mesh->numtris = header.num_tris;
    mesh->numindices = header.num_tris * 3;
    mesh->numverts = header.num_verts;
    mesh->numskins = header.num_skins;
    CHECK(mesh->verts = MOD_Malloc(sizeof(mesh->verts[0]) * header.num_verts * model->numframes));
    CHECK(mesh->tcoords = MOD_Malloc(sizeof(mesh->tcoords[0]) * header.num_verts));
    CHECK(mesh->indices = MOD_Malloc(sizeof(mesh->indices[0]) * header.num_tris * 3));
    CHECK(mesh->skins = MOD_Malloc(sizeof(mesh->skins[0]) * header.num_skins));

    // load all skins
    src_skin = (dmd3skin_t *)(rawdata + header.ofs_skins);
    for (i = 0; i < header.num_skins; i++) {
        if (!Q_memccpy(skinname, src_skin->name, 0, sizeof(skinname)))
            return Q_ERR_STRING_TRUNCATED;
        FS_NormalizePath(skinname);
        mesh->skins[i] = IMG_Find(skinname, IT_SKIN, IF_NONE);
        src_skin++;
    }

    // load all vertices
    src_vert = (dmd3vertex_t *)(rawdata + header.ofs_verts);
    dst_vert = mesh->verts;
    for (i = 0; i < model->numframes; i++) {
        maliasframe_t *f = &model->frames[i];

        for (j = 0; j < header.num_verts; j++) {
            dst_vert->pos[0] = (int16_t)LittleShort(src_vert->point[0]);
            dst_vert->pos[1] = (int16_t)LittleShort(src_vert->point[1]);
            dst_vert->pos[2] = (int16_t)LittleShort(src_vert->point[2]);

            dst_vert->norm[0] = src_vert->norm[0];
            dst_vert->norm[1] = src_vert->norm[1];

            for (k = 0; k < 3; k++) {
                f->bounds[0][k] = min(f->bounds[0][k], dst_vert->pos[k]);
                f->bounds[1][k] = max(f->bounds[1][k], dst_vert->pos[k]);
            }

            src_vert++; dst_vert++;
        }
    }

    // load all texture coords
    src_tc = (dmd3coord_t *)(rawdata + header.ofs_tcs);
    dst_tc = mesh->tcoords;
    for (i = 0; i < header.num_verts; i++) {
        dst_tc->st[0] = LittleFloat(src_tc->st[0]);
        dst_tc->st[1] = LittleFloat(src_tc->st[1]);
        src_tc++; dst_tc++;
    }

    // load all triangle indices
    src_idx = (uint32_t *)(rawdata + header.ofs_indexes);
    dst_idx = mesh->indices;
    for (i = 0; i < header.num_tris * 3; i++) {
        index = LittleLong(*src_idx++);
        if (index >= header.num_verts) {
            Com_SetLastError("bad triangle index");
            return Q_ERR_INVALID_FORMAT;
        }
        *dst_idx++ = index;
    }

    *offset_p = header.meshsize;
    return Q_ERR_SUCCESS;

fail:
    return ret;
}

static const char *MOD_ValidateMD3(const dmd3header_t *header, size_t length)
{
    ENSURE(header->num_frames >= 1, "too few frames");
    ENSURE(header->num_frames <= MD3_MAX_FRAMES, "too many frames");
    ENSURE(header->ofs_frames + (uint64_t)header->num_frames * sizeof(dmd3frame_t) <= length, "bad frames offset");
    ENSURE(!(header->ofs_frames % q_alignof(dmd3frame_t)), "odd frames offset");
    ENSURE(header->num_meshes >= 1, "too few meshes");
    ENSURE(header->num_meshes <= MD3_MAX_MESHES, "too many meshes");
    ENSURE(header->ofs_meshes <= length, "bad meshes offset");
    ENSURE(!(header->ofs_meshes % q_alignof(dmd3mesh_t)), "odd meshes offset");
    return NULL;
}

static int MOD_LoadMD3(model_t *model, const void *rawdata, size_t length)
{
    dmd3header_t    header;
    size_t          offset, remaining;
    dmd3frame_t     *src_frame;
    maliasframe_t   *dst_frame;
    const byte      *src_mesh;
    int             i, ret;
    const char      *err;

    if (length < sizeof(header))
        return Q_ERR_FILE_TOO_SMALL;

    // byte swap the header
    LittleBlock(&header, rawdata, sizeof(header));

    // check ident and version
    if (header.ident != MD3_IDENT)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header.version != MD3_VERSION)
        return Q_ERR_UNKNOWN_FORMAT;

    // validate the header
    err = MOD_ValidateMD3(&header, length);
    if (err) {
        Com_SetLastError(err);
        return Q_ERR_INVALID_FORMAT;
    }

    Hunk_Begin(&model->hunk, 0x400000);
    model->type = MOD_ALIAS;
    model->numframes = header.num_frames;
    model->nummeshes = header.num_meshes;
    CHECK(model->meshes = MOD_Malloc(sizeof(model->meshes[0]) * header.num_meshes));
    CHECK(model->frames = MOD_Malloc(sizeof(model->frames[0]) * header.num_frames));

    // load all frames
    src_frame = (dmd3frame_t *)((byte *)rawdata + header.ofs_frames);
    dst_frame = model->frames;
    for (i = 0; i < header.num_frames; i++) {
        LittleVector(src_frame->translate, dst_frame->translate);
        VectorSet(dst_frame->scale, MD3_XYZ_SCALE, MD3_XYZ_SCALE, MD3_XYZ_SCALE);

        ClearBounds(dst_frame->bounds[0], dst_frame->bounds[1]);

        src_frame++; dst_frame++;
    }

    // load all meshes
    src_mesh = (const byte *)rawdata + header.ofs_meshes;
    remaining = length - header.ofs_meshes;
    for (i = 0; i < header.num_meshes; i++) {
        ret = MOD_LoadMD3Mesh(model, &model->meshes[i], src_mesh, remaining, &offset);
        if (ret)
            goto fail;
        src_mesh += offset;
        remaining -= offset;
    }

    // calculate frame bounds
    dst_frame = model->frames;
    for (i = 0; i < header.num_frames; i++) {
        VectorScale(dst_frame->bounds[0], MD3_XYZ_SCALE, dst_frame->bounds[0]);
        VectorScale(dst_frame->bounds[1], MD3_XYZ_SCALE, dst_frame->bounds[1]);

        dst_frame->radius = RadiusFromBounds(dst_frame->bounds[0], dst_frame->bounds[1]);

        VectorAdd(dst_frame->bounds[0], dst_frame->translate, dst_frame->bounds[0]);
        VectorAdd(dst_frame->bounds[1], dst_frame->translate, dst_frame->bounds[1]);

        dst_frame++;
    }

    Hunk_End(&model->hunk);
    return Q_ERR_SUCCESS;

fail:
    Hunk_Free(&model->hunk);
    return ret;
}
#endif

static void MOD_Reference(model_t *model)
{
    int i, j;

    // register any images used by the models
    switch (model->type) {
    case MOD_ALIAS:
        for (i = 0; i < model->nummeshes; i++) {
            maliasmesh_t *mesh = &model->meshes[i];
            for (j = 0; j < mesh->numskins; j++) {
                mesh->skins[j]->registration_sequence = registration_sequence;
            }
        }
        break;
    case MOD_SPRITE:
        for (i = 0; i < model->numframes; i++) {
            model->spriteframes[i].image->registration_sequence = registration_sequence;
        }
        break;
    case MOD_EMPTY:
        break;
    default:
        Q_assert(!"bad model type");
    }

    model->registration_sequence = registration_sequence;
}

qhandle_t R_RegisterModel(const char *name)
{
    char normalized[MAX_QPATH];
    qhandle_t index;
    size_t namelen;
    int filelen;
    model_t *model;
    byte *rawdata;
    uint32_t ident;
    int (*load)(model_t *, const void *, size_t);
    int ret;

    Q_assert(name);

    // empty names are legal, silently ignore them
    if (!*name)
        return 0;

    if (*name == '*') {
        // inline bsp model
        index = atoi(name + 1);
        return ~index;
    }

    // normalize the path
    namelen = FS_NormalizePathBuffer(normalized, name, MAX_QPATH);

    // this should never happen
    if (namelen >= MAX_QPATH) {
        ret = Q_ERR(ENAMETOOLONG);
        goto fail1;
    }

    // normalized to empty name?
    if (namelen == 0) {
        Com_DPrintf("%s: empty name\n", __func__);
        return 0;
    }

    // see if it's already loaded
    model = MOD_Find(normalized);
    if (model) {
        MOD_Reference(model);
        goto done;
    }

    filelen = FS_LoadFile(normalized, (void **)&rawdata);
    if (!rawdata) {
        // don't spam about missing models
        if (filelen == Q_ERR(ENOENT)) {
            return 0;
        }

        ret = filelen;
        goto fail1;
    }

    if (filelen < 4) {
        ret = Q_ERR_FILE_TOO_SMALL;
        goto fail2;
    }

    // check ident
    ident = LittleLong(*(uint32_t *)rawdata);
    switch (ident) {
    case MD2_IDENT:
        load = MOD_LoadMD2;
        break;
#if USE_MD3
    case MD3_IDENT:
        load = MOD_LoadMD3;
        break;
#endif
    case SP2_IDENT:
        load = MOD_LoadSP2;
        break;
    default:
        ret = Q_ERR_UNKNOWN_FORMAT;
        goto fail2;
    }

    model = MOD_Alloc();
    if (!model) {
        ret = Q_ERR_OUT_OF_SLOTS;
        goto fail2;
    }

    memcpy(model->name, normalized, namelen + 1);
    model->registration_sequence = registration_sequence;

    ret = load(model, rawdata, filelen);

    FS_FreeFile(rawdata);

    if (ret) {
        memset(model, 0, sizeof(*model));
        goto fail1;
    }

done:
    index = (model - r_models) + 1;
    return index;

fail2:
    FS_FreeFile(rawdata);
fail1:
    Com_EPrintf("Couldn't load %s: %s\n", normalized,
                ret == Q_ERR_INVALID_FORMAT ?
                Com_GetLastError() : Q_ErrorString(ret));
    return 0;
}

model_t *MOD_ForHandle(qhandle_t h)
{
    model_t *model;

    if (!h) {
        return NULL;
    }

    Q_assert(h > 0 && h <= r_numModels);
    model = &r_models[h - 1];
    if (!model->type) {
        return NULL;
    }

    return model;
}

void MOD_Init(void)
{
    Q_assert(!r_numModels);
    Cmd_AddCommand("modellist", MOD_List_f);
}

void MOD_Shutdown(void)
{
    MOD_FreeAll();
    Cmd_RemoveCommand("modellist");
}
