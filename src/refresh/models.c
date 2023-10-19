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

#define OOM_CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }
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
        size_t model_size = model->hunk.mapped;
#if USE_MD5
        model_size += model->skeleton_hunk.mapped;
#endif
        Com_Printf("%c %8zu : %s\n", types[model->type],
                   model_size, model->name);
        bytes += model_size;
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
#if USE_MD5
            if (model->skeleton_hunk.base)
                Com_PageInMemory(model->skeleton_hunk.base, model->skeleton_hunk.cursize);
#endif
        } else {
            // don't need this model
            Hunk_Free(&model->hunk);
#if USE_MD5
            Hunk_Free(&model->skeleton_hunk);
#endif
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
#if USE_MD5
        Hunk_Free(&model->skeleton_hunk);
#endif
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
    OOM_CHECK(model->meshes = MOD_Malloc(sizeof(model->meshes[0])));
    OOM_CHECK(model->frames = MOD_Malloc(header.num_frames * sizeof(model->frames[0])));

    mesh = model->meshes;
    mesh->numtris = numindices / 3;
    mesh->numindices = numindices;
    mesh->numverts = numverts;
    mesh->numskins = header.num_skins;
    OOM_CHECK(mesh->verts = MOD_Malloc(numverts * header.num_frames * sizeof(mesh->verts[0])));
    OOM_CHECK(mesh->tcoords = MOD_Malloc(numverts * sizeof(mesh->tcoords[0])));
    OOM_CHECK(mesh->indices = MOD_Malloc(numindices * sizeof(mesh->indices[0])));
    OOM_CHECK(mesh->skins = MOD_Malloc(header.num_skins * sizeof(mesh->skins[0])));
#if USE_MD5
    OOM_CHECK(mesh->skinnames = MOD_Malloc(header.num_skins * sizeof(mesh->skinnames[0])));
#endif

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
#if USE_MD5
        char *skinname = mesh->skinnames[i];
#else
        maliasskinname_t skinname;
#endif
        if (!Q_memccpy(skinname, src_skin, 0, sizeof(maliasskinname_t))) {
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
    OOM_CHECK(mesh->verts = MOD_Malloc(sizeof(mesh->verts[0]) * header.num_verts * model->numframes));
    OOM_CHECK(mesh->tcoords = MOD_Malloc(sizeof(mesh->tcoords[0]) * header.num_verts));
    OOM_CHECK(mesh->indices = MOD_Malloc(sizeof(mesh->indices[0]) * header.num_tris * 3));
    OOM_CHECK(mesh->skins = MOD_Malloc(sizeof(mesh->skins[0]) * header.num_skins));
#if USE_MD5
    OOM_CHECK(mesh->skinnames = MOD_Malloc(sizeof(mesh->skinnames[0]) * header.num_skins));
#endif

    // load all skins
    src_skin = (dmd3skin_t *)(rawdata + header.ofs_skins);
    for (i = 0; i < header.num_skins; i++) {
#if USE_MD5
        char *skinname = mesh->skinnames[i];
#else
        maliasskinname_t skinname;
#endif
        if (!Q_memccpy(skinname, src_skin->name, 0, sizeof(maliasskinname_t)))
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
    OOM_CHECK(model->meshes = MOD_Malloc(sizeof(model->meshes[0]) * header.num_meshes));
    OOM_CHECK(model->frames = MOD_Malloc(sizeof(model->frames[0]) * header.num_frames));

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

static void MOD_PrintError(const char *path, int err)
{
    Com_EPrintf("Couldn't load %s: %s\n", path,
                err == Q_ERR_INVALID_FORMAT ?
                Com_GetLastError() : Q_ErrorString(err));
}

#if USE_MD5

#define MD5_Malloc(size)    Hunk_TryAlloc(&model->skeleton_hunk, size)

static bool MD5_ParseExpect(const char **buffer, const char *expect)
{
    char *token = COM_Parse(buffer);

    if (strcmp(token, expect)) {
        Com_SetLastError(va("expected %s, got %s", expect, token));
        return false;
    }

    return true;
}

static bool MD5_ParseFloat(const char **buffer, float *output)
{
    char *token = COM_Parse(buffer);
    char *endptr;

    *output = strtof(token, &endptr);
    if (endptr == token || *endptr) {
        Com_SetLastError(va("expected float, got %s", token));
        return false;
    }

    return true;
}

static bool MD5_ParseUint(const char **buffer, uint32_t *output)
{
    char *token = COM_Parse(buffer);
    char *endptr;

    *output = strtoul(token, &endptr, 10);
    if (endptr == token || *endptr) {
        Com_SetLastError(va("expected int, got %s", token));
        return false;
    }

    return true;
}

static bool MD5_ParseVector(const char **buffer, vec3_t output)
{
    return
        MD5_ParseExpect(buffer, "(") &&
        MD5_ParseFloat(buffer, &output[0]) &&
        MD5_ParseFloat(buffer, &output[1]) &&
        MD5_ParseFloat(buffer, &output[2]) &&
        MD5_ParseExpect(buffer, ")");
}

#define MD5_CHECK(x) \
    if (!(x)) { ret = Q_ERR_INVALID_FORMAT; goto fail; }

#define MD5_ENSURE(x, e) \
    if (!(x)) { Com_SetLastError(e); ret = Q_ERR_INVALID_FORMAT; goto fail; }

#define MD5_EXPECT(x)   MD5_CHECK(MD5_ParseExpect(&s, x))
#define MD5_UINT(x)     MD5_CHECK(MD5_ParseUint(&s, x))
#define MD5_FLOAT(x)    MD5_CHECK(MD5_ParseFloat(&s, x))
#define MD5_VECTOR(x)   MD5_CHECK(MD5_ParseVector(&s, x))

static void MD5_ComputeNormals(md5_mesh_t *mesh, const md5_joint_t *base_skeleton)
{
    vec3_t finalVerts[TESS_MAX_VERTICES];
    md5_vertex_t *vert;
    int i, j;

    hash_map_t *pos_to_normal_map = HashMap_Create(vec3_t, vec3_t, &HashVec3, NULL);
    HashMap_Reserve(pos_to_normal_map, mesh->num_verts);

    for (i = 0, vert = mesh->vertices; i < mesh->num_verts; i++, vert++) {
        /* Calculate final vertex to draw with weights */
        VectorClear(finalVerts[i]);

        for (j = 0; j < vert->count; j++) {
            const md5_weight_t *weight = &mesh->weights[vert->start + j];
            const md5_joint_t *joint = &base_skeleton[weight->joint];

            /* Calculate transformed vertex for this weight */
            vec3_t wv;
            Quat_RotatePoint(joint->orient, weight->pos, wv);

            /* The sum of all weight->bias should be 1.0 */
            VectorAdd(joint->pos, wv, wv);
            VectorMA(finalVerts[i], weight->bias, wv, finalVerts[i]);
        }
    }

    for (i = 0; i < mesh->num_indices; i += 3) {
        vec3_t xyz[3];

        for (j = 0; j < 3; j++)
            VectorCopy(finalVerts[mesh->indices[i + j]], xyz[j]);

        vec3_t d1, d2;
        VectorSubtract(xyz[2], xyz[0], d1);
        VectorSubtract(xyz[1], xyz[0], d2);
        VectorNormalize(d1);
        VectorNormalize(d2);

        vec3_t norm;
        CrossProduct(d1, d2, norm);
        VectorNormalize(norm);

        float angle = acos(DotProduct(d1, d2));
        VectorScale(norm, angle, norm);

        for (j = 0; j < 3; j++) {
            vec3_t *found_normal;
            if ((found_normal = HashMap_Lookup(vec3_t, pos_to_normal_map, &xyz[j])))
                VectorAdd(*found_normal, norm, *found_normal);
            else
                HashMap_Insert(pos_to_normal_map, &xyz[j], &norm);
        }
    }

    uint32_t map_size = HashMap_Size(pos_to_normal_map);
    for (i = 0; i < map_size; i++) {
        vec3_t *norm = HashMap_GetValue(vec3_t, pos_to_normal_map, i);
        VectorNormalize(*norm);
    }

    for (i = 0, vert = mesh->vertices; i < mesh->num_verts; i++, vert++) {
        VectorClear(vert->normal);
        vec3_t *norm = HashMap_Lookup(vec3_t, pos_to_normal_map, &finalVerts[i]);
        if (norm) {
            // Put the bind-pose normal into joint-local space
            // so the animated normal can be computed faster later
            // Done by transforming the vertex normal by the inverse joint's orientation quaternion of the weight
            for (j = 0; j < vert->count; j++) {
                const md5_weight_t *weight = &mesh->weights[vert->start + j];
                const md5_joint_t *joint = &base_skeleton[weight->joint];
                vec3_t wv;
                quat_t orient_inv;
                Quat_Conjugate(joint->orient, orient_inv);
                Quat_RotatePoint(orient_inv, *norm, wv);
                VectorMA(vert->normal, weight->bias, wv, vert->normal);
            }
        }
    }

    HashMap_Destroy(pos_to_normal_map);
}

static bool MOD_LoadMD5Mesh(model_t *model, const char *path)
{
    md5_model_t *mdl;
    int i, j, k, ret;
    uint32_t version, num_joints, num_meshes;
    void *buffer;
    const char *s;

    ret = FS_LoadFile(path, &buffer);
    if (!buffer)
        goto fail;
    s = buffer;

    // parse header
    MD5_EXPECT("MD5Version");
    MD5_UINT(&version);
    MD5_ENSURE(version == 10, "bad version");

    // allocate data storage, now that we're definitely an MD5
    Hunk_Begin(&model->skeleton_hunk, 0x800000);

    OOM_CHECK(model->skeleton = mdl = MD5_Malloc(sizeof(*mdl)));

    MD5_EXPECT("commandline");
    COM_Parse(&s);

    MD5_EXPECT("numJoints");
    MD5_UINT(&num_joints);
    MD5_ENSURE(num_joints, "no joints");
    MD5_ENSURE(num_joints <= MD5_MAX_JOINTS, "too many joints");
    OOM_CHECK(mdl->base_skeleton = MD5_Malloc(num_joints * sizeof(mdl->base_skeleton[0])));
    OOM_CHECK(mdl->jointnames = MD5_Malloc(num_joints * sizeof(mdl->jointnames[0])));
    mdl->num_joints = num_joints;

    MD5_EXPECT("numMeshes");
    MD5_UINT(&num_meshes);
    MD5_ENSURE(num_meshes, "no meshes");
    MD5_ENSURE(num_meshes <= MD5_MAX_MESHES, "too many meshes");
    OOM_CHECK(mdl->meshes = MD5_Malloc(num_meshes * sizeof(mdl->meshes[0])));
    mdl->num_meshes = num_meshes;

    MD5_EXPECT("joints");
    MD5_EXPECT("{");

    for (i = 0; i < num_joints; i++) {
        md5_joint_t *joint = &mdl->base_skeleton[i];

        Q_strlcpy(mdl->jointnames[i], COM_Parse(&s), sizeof(mdl->jointnames[0]));

        uint32_t parent;
        MD5_UINT(&parent);
        MD5_ENSURE(parent == -1 || parent < num_joints, "bad parent joint");
        joint->parent = parent;

        MD5_VECTOR(joint->pos);
        MD5_VECTOR(joint->orient);

        Quat_ComputeW(joint->orient);
        joint->scale = 1.0f;
    }

    MD5_EXPECT("}");

    for (i = 0; i < num_meshes; i++) {
        md5_mesh_t *mesh = &mdl->meshes[i];
        uint32_t num_verts, num_tris, num_weights;

        MD5_EXPECT("mesh");
        MD5_EXPECT("{");

        MD5_EXPECT("shader");
        COM_Parse(&s);

        MD5_EXPECT("numverts");
        MD5_UINT(&num_verts);
        MD5_ENSURE(num_verts <= TESS_MAX_VERTICES, "too many verts");
        OOM_CHECK(mesh->vertices = MD5_Malloc(num_verts * sizeof(mesh->vertices[0])));
        mesh->num_verts = num_verts;

        for (j = 0; j < num_verts; j++) {
            MD5_EXPECT("vert");

            uint32_t vert_index;
            MD5_UINT(&vert_index);
            MD5_ENSURE(vert_index < num_verts, "bad vert index");

            md5_vertex_t *vert = &mesh->vertices[vert_index];

            MD5_EXPECT("(");
            MD5_FLOAT(&vert->st[0]);
            MD5_FLOAT(&vert->st[1]);
            MD5_EXPECT(")");

            MD5_UINT(&vert->start);
            MD5_UINT(&vert->count);
        }

        MD5_EXPECT("numtris");
        MD5_UINT(&num_tris);
        MD5_ENSURE(num_tris <= TESS_MAX_INDICES / 3, "too many tris");
        OOM_CHECK(mesh->indices = MD5_Malloc(num_tris * 3 * sizeof(mesh->indices[0])));
        mesh->num_indices = num_tris * 3;

        for (j = 0; j < num_tris; j++) {
            MD5_EXPECT("tri");

            uint32_t tri_index;
            MD5_UINT(&tri_index);
            MD5_ENSURE(tri_index < num_tris, "bad tri index");

            for (k = 0; k < 3; k++) {
                uint32_t vert_index;
                MD5_UINT(&vert_index);
                MD5_ENSURE(vert_index < mesh->num_verts, "bad tri vert");
                mesh->indices[tri_index * 3 + k] = vert_index;
            }
        }

        MD5_EXPECT("numweights");
        MD5_UINT(&num_weights);
        MD5_ENSURE(num_weights <= MD5_MAX_WEIGHTS, "too many weights");
        OOM_CHECK(mesh->weights = MD5_Malloc(num_weights * sizeof(mesh->weights[0])));
        mesh->num_weights = num_weights;

        for (j = 0; j < num_weights; j++) {
            MD5_EXPECT("weight");

            uint32_t weight_index;
            MD5_UINT(&weight_index);
            MD5_ENSURE(weight_index < num_weights, "bad weight index");

            md5_weight_t *weight = &mesh->weights[weight_index];

            uint32_t joint;
            MD5_UINT(&joint);
            MD5_ENSURE(joint < mdl->num_joints, "bad weight joint");
            weight->joint = joint;

            MD5_FLOAT(&weight->bias);
            MD5_VECTOR(weight->pos);
        }

        MD5_EXPECT("}");

        // check integrity of data; this has to be done last
        // because of circular data dependencies
        for (j = 0; j < num_verts; j++) {
            md5_vertex_t *vert = &mesh->vertices[j];
            MD5_ENSURE((uint64_t)vert->start + vert->count <= num_weights, "bad start/count");
        }

        MD5_ComputeNormals(mesh, mdl->base_skeleton);
    }

    FS_FreeFile(buffer);
    return true;

fail:
    MOD_PrintError(path, ret);
    FS_FreeFile(buffer);
    return false;
}

typedef struct {
    uint32_t parent;
    uint32_t flags;
    uint32_t start_index;
} joint_info_t;

typedef struct {
    vec3_t pos;
    quat_t orient;
} baseframe_joint_t;

#define MD5_NUM_ANIMATED_COMPONENT_BITS 6

/**
 * Build skeleton for a given frame data.
 */
static void MD5_BuildFrameSkeleton(const joint_info_t *joint_infos,
                                   const baseframe_joint_t *base_frame,
                                   const float *anim_frame_data,
                                   md5_joint_t *skeleton_frame,
                                   int num_joints)
{
    for (int i = 0; i < num_joints; i++) {
        const baseframe_joint_t *baseJoint = &base_frame[i];
        float components[7];

        float *animated_position = components + 0;
        float *animated_quat = components + 3;

        VectorCopy(baseJoint->pos, animated_position);
        VectorCopy(baseJoint->orient, animated_quat); // W will be re-calculated below

        for (int c = 0, j = 0; c < MD5_NUM_ANIMATED_COMPONENT_BITS; c++) {
            if (joint_infos[i].flags & BIT(c)) {
                components[c] = anim_frame_data[joint_infos[i].start_index + j++];
            }
        }

        Quat_ComputeW(animated_quat);

        // parent should already be calculated
        md5_joint_t *thisJoint = &skeleton_frame[i];
        thisJoint->scale = 1.0f;

        int parent = thisJoint->parent = (int32_t)joint_infos[i].parent;
        if (parent < 0) {
            VectorCopy(animated_position, thisJoint->pos);
            Vector4Copy(animated_quat, thisJoint->orient);
            continue;
        }

        md5_joint_t *parentJoint = &skeleton_frame[parent];

        // add positions
        vec3_t rotated_pos;
        Quat_RotatePoint(parentJoint->orient, animated_position, rotated_pos);
        VectorAdd(rotated_pos, parentJoint->pos, thisJoint->pos);

        // concat rotations
        Quat_MultiplyQuat(parentJoint->orient, animated_quat, thisJoint->orient);
        Quat_Normalize(thisJoint->orient);
    }
}

/**
 * Load an MD5 animation from file.
 */
static bool MOD_LoadMD5Anim(model_t *model, const char *path)
{
    joint_info_t joint_infos[MD5_MAX_JOINTS];
    baseframe_joint_t base_frame[MD5_MAX_JOINTS];
    float anim_frame_data[MD5_MAX_JOINTS * MD5_NUM_ANIMATED_COMPONENT_BITS];
    uint32_t version, num_frames, num_joints, num_animated_components;
    md5_model_t *mdl = model->skeleton;
    int i, j, ret;
    void *buffer;
    const char *s;

    ret = FS_LoadFile(path, &buffer);
    if (!buffer)
        goto fail;
    s = buffer;

    // parse header
    MD5_EXPECT("MD5Version");
    MD5_UINT(&version);
    MD5_ENSURE(version == 10, "bad version");

    MD5_EXPECT("commandline");
    COM_Parse(&s);

    MD5_EXPECT("numFrames");
    MD5_UINT(&num_frames);
    // md5 replacements need at least 1 frame, because the
    // pose frame isn't used
    MD5_ENSURE(num_frames, "no frames");
    MD5_ENSURE(num_frames <= MD5_MAX_FRAMES, "too many frames");
    mdl->num_frames = num_frames;

    // warn on mismatched frame counts (not fatal)
    if (mdl->num_frames != model->numframes) {
        Com_WPrintf("%s doesn't match frame count for %s (%i vs %i)\n",
                    path, model->name, mdl->num_frames, model->numframes);
    }

    MD5_EXPECT("numJoints");
    MD5_UINT(&num_joints);
    MD5_ENSURE(num_joints == mdl->num_joints, "bad numJoints");

    MD5_EXPECT("frameRate");
    COM_Parse(&s);

    MD5_EXPECT("numAnimatedComponents");
    MD5_UINT(&num_animated_components);
    MD5_ENSURE(num_animated_components <= q_countof(anim_frame_data), "bad numAnimatedComponents");

    MD5_EXPECT("hierarchy");
    MD5_EXPECT("{");

    for (i = 0; i < mdl->num_joints; ++i) {
        joint_info_t *joint_info = &joint_infos[i];

        COM_Parse(&s); // ignore name

        MD5_UINT(&joint_info->parent);
        MD5_UINT(&joint_info->flags);
        MD5_UINT(&joint_info->start_index);

        // validate animated components
        int num_components = 0;

        for (j = 0; j < MD5_NUM_ANIMATED_COMPONENT_BITS; j++) {
            if (joint_info->flags & BIT(j)) {
                num_components++;
            }
        }

        MD5_ENSURE((uint64_t)joint_info->start_index + num_components <= num_animated_components, "bad joint info");

        // validate parents; they need to match the base skeleton
        MD5_ENSURE(joint_info->parent == mdl->base_skeleton[i].parent, "bad parent");
    }

    MD5_EXPECT("}");

    // bounds are ignored and are apparently usually wrong anyways
    // so we'll just rely on them being "replacement" md2s/md3s.
    // the md2/md3 ones are used instead.
    MD5_EXPECT("bounds");
    MD5_EXPECT("{");

    for (i = 0; i < mdl->num_frames * 2; i++) {
        vec3_t dummy;
        MD5_VECTOR(dummy);
    }

    MD5_EXPECT("}");

    MD5_EXPECT("baseframe");
    MD5_EXPECT("{");

    for (i = 0; i < mdl->num_joints; i++) {
        baseframe_joint_t *base_joint = &base_frame[i];

        MD5_VECTOR(base_joint->pos);
        MD5_VECTOR(base_joint->orient);

        Quat_ComputeW(base_joint->orient);
    }

    MD5_EXPECT("}");

    OOM_CHECK(mdl->skeleton_frames = MD5_Malloc(sizeof(mdl->skeleton_frames[0]) * mdl->num_frames * mdl->num_joints));

    for (i = 0; i < mdl->num_frames; i++) {
        MD5_EXPECT("frame");

        uint32_t frame_index;
        MD5_UINT(&frame_index);
        MD5_ENSURE(frame_index < mdl->num_frames, "bad frame index");

        MD5_EXPECT("{");
        for (j = 0; j < num_animated_components; j++) {
            MD5_FLOAT(&anim_frame_data[j]);
        }
        MD5_EXPECT("}");

        /* Build frame skeleton from the collected data */
        MD5_BuildFrameSkeleton(joint_infos, base_frame, anim_frame_data,
                               &mdl->skeleton_frames[frame_index * mdl->num_joints], mdl->num_joints);
    }

    FS_FreeFile(buffer);
    return true;

fail:
    MOD_PrintError(path, ret);
    FS_FreeFile(buffer);
    return false;
}

// icky icky ""JSON"" parser. it works for re-release *.md5scale files, and I
// don't care about anything else...
static void MOD_LoadMD5Scale(md5_model_t *model, const char *path)
{
    void *buffer;
    const char *s;
    int ret;

    ret = FS_LoadFile(path, &buffer);
    if (!buffer)
        goto fail;
    s = buffer;

    MD5_EXPECT("{");
    while (s) {
        int joint_id = -1;
        char *tok;

        tok = COM_Parse(&s);
        if (!strcmp(tok, "}"))
            break;

        for (int i = 0; i < model->num_joints; i++) {
            if (!strcmp(tok, model->jointnames[i])) {
                joint_id = i;
                break;
            }
        }

        if (joint_id == -1)
            Com_WPrintf("No such joint %s in %s\n", tok, path);

        MD5_EXPECT(":");
        MD5_EXPECT("{");

        while (s) {
            tok = COM_Parse(&s);
            if (!strcmp(tok, "}") || !strcmp(tok, "},"))
                break;
            MD5_EXPECT(":");

            unsigned frame_id = strtoul(tok, NULL, 10);
            float scale = strtof(COM_Parse(&s), NULL);

            if (joint_id == -1)
                continue;

            if (frame_id >= model->num_frames) {
                Com_WPrintf("No such frame %d in %s\n", frame_id, path);
                continue;
            }

            model->skeleton_frames[(frame_id * model->num_joints) + joint_id].scale = scale;
        }
    }

    FS_FreeFile(buffer);
    return;

fail:
    if (ret != Q_ERR(ENOENT))
        MOD_PrintError(path, ret);
    FS_FreeFile(buffer);
}

static void MOD_LoadMD5(model_t *model)
{
    char model_name[MAX_QPATH], base_path[MAX_QPATH];
    char mesh_path[MAX_QPATH], anim_path[MAX_QPATH];
    char scale_path[MAX_QPATH];

    COM_SplitPath(model->name, model_name, sizeof(model_name), base_path, sizeof(base_path), true);

    if (Q_concat(mesh_path, sizeof(mesh_path), base_path, "md5/", model_name, ".md5mesh") >= sizeof(mesh_path) ||
        Q_concat(anim_path, sizeof(anim_path), base_path, "md5/", model_name, ".md5anim") >= sizeof(anim_path))
        return;

    // don't bother if we don't have both
    if (!FS_FileExists(mesh_path) || !FS_FileExists(anim_path))
        return;

    if (!MOD_LoadMD5Mesh(model, mesh_path))
        goto fail;

    if (!MOD_LoadMD5Anim(model, anim_path))
        goto fail;

    model->skeleton->num_skins = model->meshes[0].numskins;
    if (!(model->skeleton->skins = MD5_Malloc(sizeof(model->skeleton->skins[0]) * model->meshes[0].numskins)))
        goto fail;

    for (int i = 0; i < model->meshes[0].numskins; i++) {
        // because skins are actually absolute and not always relative to the
        // model being used, we have to stick to the same behavior.
        char skin_name[MAX_QPATH], skin_path[MAX_QPATH];

        COM_SplitPath(model->meshes[0].skinnames[i], skin_name, sizeof(skin_name), skin_path, sizeof(skin_path), false);

        // build md5 path
        if (Q_strlcat(skin_path, "md5/", sizeof(skin_path)) < sizeof(skin_path) &&
            Q_strlcat(skin_path, skin_name, sizeof(skin_path)) < sizeof(skin_path))
            model->skeleton->skins[i] = IMG_Find(skin_path, IT_SKIN, IF_NONE);
    }

    Hunk_End(&model->skeleton_hunk);

    if (Q_concat(scale_path, sizeof(scale_path), base_path, "md5/", model_name, ".md5scale") < sizeof(scale_path))
        MOD_LoadMD5Scale(model->skeleton, scale_path);

    return;

fail:
    model->skeleton = NULL;
    Hunk_Free(&model->skeleton_hunk);
}

#endif  // USE_MD5

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
#if USE_MD5
        if (model->skeleton) {
            for (j = 0; j < model->skeleton->num_skins; j++) {
                model->skeleton->skins[j]->registration_sequence = registration_sequence;
            }
        }
#endif
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

#if USE_MD5
    // check for an MD5; this requires the MD2/MD3
    // to have loaded first, since we need it for skin names
    if (model->type == MOD_ALIAS && gl_md5_load->integer)
        MOD_LoadMD5(model);
#endif

done:
    index = (model - r_models) + 1;
    return index;

fail2:
    FS_FreeFile(rawdata);
fail1:
    MOD_PrintError(normalized, ret);
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
