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
#if USE_MD5
#include "common/hash_map.h"
#define JSMN_STATIC
#include "common/jsmn.h"
#endif

#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)

#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }

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
    size_t end;

    // check triangles
    if (header->num_tris < 1)
        return "too few tris";
    if (header->num_tris > TESS_MAX_INDICES / 3)
        return "too many tris";

    end = header->ofs_tris + sizeof(dmd2triangle_t) * header->num_tris;
    if (header->ofs_tris < sizeof(*header) || end < header->ofs_tris || end > length)
        return "bad tris offset";
    if (header->ofs_tris % q_alignof(dmd2triangle_t))
        return "odd tris offset";

    // check st
    if (header->num_st < 3)
        return "too few st";
    if (header->num_st > INT_MAX / sizeof(dmd2stvert_t))
        return "too many st";

    end = header->ofs_st + sizeof(dmd2stvert_t) * header->num_st;
    if (header->ofs_st < sizeof(*header) || end < header->ofs_st || end > length)
        return "bad st offset";
    if (header->ofs_st % q_alignof(dmd2stvert_t))
        return "odd st offset";

    // check xyz and frames
    if (header->num_xyz < 3)
        return "too few xyz";
    if (header->num_xyz > MD2_MAX_VERTS)
        return "too many xyz";
    if (header->num_frames < 1)
        return "too few frames";
    if (header->num_frames > MD2_MAX_FRAMES)
        return "too many frames";

    end = sizeof(dmd2frame_t) + (header->num_xyz - 1) * sizeof(dmd2trivertx_t);
    if (header->framesize < end || header->framesize > MD2_MAX_FRAMESIZE)
        return "bad frame size";
    if (header->framesize % q_alignof(dmd2frame_t))
        return "odd frame size";

    end = header->ofs_frames + (size_t)header->framesize * header->num_frames;
    if (header->ofs_frames < sizeof(*header) || end < header->ofs_frames || end > length)
        return "bad frames offset";
    if (header->ofs_frames % q_alignof(dmd2frame_t))
        return "odd frames offset";

    // check skins
    if (header->num_skins) {
        if (header->num_skins > MD2_MAX_SKINS)
            return "too many skins";

        end = header->ofs_skins + (size_t)MD2_MAX_SKINNAME * header->num_skins;
        if (header->ofs_skins < sizeof(*header) || end < header->ofs_skins || end > length)
            return "bad skins offset";
    }

    if (header->skinwidth < 1 || header->skinwidth > MD2_MAX_SKINWIDTH)
        return "bad skin width";
    if (header->skinheight < 1 || header->skinheight > MD2_MAX_SKINHEIGHT)
        return "bad skin height";

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

    // validate the header
    err = MOD_ValidateMD2(&header, length);
    if (err) {
        if (!strncmp(err, CONST_STR_LEN("too few"))) {
            // empty models draw nothing
            model->type = MOD_EMPTY;
            return Q_ERR_SUCCESS;
        }
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
#if USE_MD5
    CHECK(mesh->skinnames = MOD_Malloc(header.num_skins * sizeof(mesh->skinnames[0])));
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
        char skinname[MAX_QPATH];
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
    size_t end;

    if (header->meshsize < sizeof(header) || header->meshsize > length)
        return "bad mesh size";
    if (header->meshsize % q_alignof(dmd3mesh_t))
        return "odd mesh size";
    if (header->num_verts < 3)
        return "too few verts";
    if (header->num_verts > TESS_MAX_VERTICES)
        return "too many verts";
    if (header->num_tris < 1)
        return "too few tris";
    if (header->num_tris > TESS_MAX_INDICES / 3)
        return "too many tris";
    if (header->num_skins > MD3_MAX_SKINS)
        return "too many skins";
    end = header->ofs_skins + header->num_skins * sizeof(dmd3skin_t);
    if (end < header->ofs_skins || end > length)
        return "bad skins offset";
    if (header->ofs_skins % q_alignof(dmd3skin_t))
        return "odd skins offset";
    end = header->ofs_verts + header->num_verts * model->numframes * sizeof(dmd3vertex_t);
    if (end < header->ofs_verts || end > length)
        return "bad verts offset";
    if (header->ofs_verts % q_alignof(dmd3vertex_t))
        return "odd verts offset";
    end = header->ofs_tcs + header->num_verts * sizeof(dmd3coord_t);
    if (end < header->ofs_tcs || end > length)
        return "bad tcs offset";
    if (header->ofs_tcs % q_alignof(dmd3coord_t))
        return "odd tcs offset";
    end = header->ofs_indexes + header->num_tris * 3 * sizeof(uint32_t);
    if (end < header->ofs_indexes || end > length)
        return "bad indexes offset";
    if (header->ofs_indexes & 3)
        return "odd indexes offset";
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
    CHECK(mesh->verts = MOD_Malloc(sizeof(mesh->verts[0]) * header.num_verts * model->numframes));
    CHECK(mesh->tcoords = MOD_Malloc(sizeof(mesh->tcoords[0]) * header.num_verts));
    CHECK(mesh->indices = MOD_Malloc(sizeof(mesh->indices[0]) * header.num_tris * 3));
    CHECK(mesh->skins = MOD_Malloc(sizeof(mesh->skins[0]) * header.num_skins));
#if USE_MD5
    CHECK(mesh->skinnames = MOD_Malloc(sizeof(mesh->skinnames[0]) * header.num_skins));
#endif

    // load all skins
    src_skin = (dmd3skin_t *)(rawdata + header.ofs_skins);
    for (i = 0; i < header.num_skins; i++) {
#if USE_MD5
        char *skinname = mesh->skinnames[i];
#else
        char skinname[MAX_QPATH];
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
    size_t end;

    if (header->num_frames < 1)
        return "too few frames";
    if (header->num_frames > MD3_MAX_FRAMES)
        return "too many frames";
    end = header->ofs_frames + sizeof(dmd3frame_t) * header->num_frames;
    if (end < header->ofs_frames || end > length)
        return "bad frames offset";
    if (header->ofs_frames % q_alignof(dmd3frame_t))
        return "odd frames offset";
    if (header->num_meshes < 1)
        return "too few meshes";
    if (header->num_meshes > MD3_MAX_MESHES)
        return "too many meshes";
    if (header->ofs_meshes > length)
        return "bad meshes offset";
    if (header->ofs_meshes % q_alignof(dmd3mesh_t))
        return "odd meshes offset";
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

#if USE_MD5
// splits an input filename into file name and path components
static void COM_SplitPath(const char *path, char *file_name, size_t file_name_len,
                          char *path_name, size_t path_name_len, bool strip_extension)
{
    // split path (models/objects/gibs/tris.md2)
    // - name (tris)
    // - path (models/objects/gibs/)
    const char *name_ptr = COM_SkipPath(path);

    if (strip_extension)
        COM_StripExtension(file_name, name_ptr, file_name_len);
    else
        Q_strlcpy(file_name, name_ptr, file_name_len);

    Q_strnlcpy(path_name, path, name_ptr - path, path_name_len);
}

#define MD5_Malloc(size)    Hunk_TryAlloc(&model->skeleton_hunk, size)

// parse a token from the buffer, move the buffer ahead, and
// expect that the token matches the given token text.
static inline bool MD5_ParseExpect(const char **buffer, const char *expect_token)
{
    const char *token = COM_Parse(buffer);
    return !strcmp(token, expect_token);
}

// parse a token from the buffer, move the buffer ahead, and
// expect that the token is a valid float; the result will
// be stored in `output`.
static inline bool MD5_ParseFloat(const char **buffer, float *output)
{
    const char *token = COM_Parse(buffer);
    const char *endptr;
    *output = strtof(token, &endptr);
    return endptr != token;
}

// parse a token from the buffer, move the buffer ahead, and
// expect that the token is a valid int32; the result will
// be stored in `output`.
static inline bool MD5_ParseInt32(const char **buffer, int32_t *output)
{
    const char *token = COM_Parse(buffer);
    const char *endptr;
    *output = strtol(token, &endptr, 10);
    return endptr != token;
}

#define MD5_CHECK(x) \
    if (!(x)) { ret = Q_ERR_INVALID_FORMAT; goto fail; }

static bool MOD_LoadMD5Header(model_t *model, const char **file_buffer, int *out_ret)
{
    int ret = 0;

    // parse header
    {
        int32_t version;

        MD5_CHECK(MD5_ParseExpect(file_buffer, "MD5Version"));
        MD5_CHECK(MD5_ParseInt32(file_buffer, &version));
    
	    if (version != 10) {
fail:
            *out_ret = ret;
		    return false;
	    }
    }

    return true;
}

/**
 * Load an MD5 model from file.
 */
static int MOD_LoadMD5Mesh(model_t *model, const char *file_buffer, maliasskinname_t **joint_names)
{
    int ret = 0;

    md5_vertex_t *vertex_data = NULL;
    QGL_INDEX_TYPE *index_data = NULL;
    md5_weight_t *weight_data = NULL;

    // parse header
    if (!MOD_LoadMD5Header(model, &file_buffer, &ret)) {
        goto fail;
    }

    // allocate data storage, now that we're definitely an MD5
    Hunk_Begin(&model->skeleton_hunk, 0x800000);

    CHECK(model->skeleton = MD5_Malloc(sizeof(md5_model_t)));

    md5_model_t *mdl = model->skeleton;

    // data that we parse but don't keep after parsing
    int32_t num_meshes = -1, total_meshes = 0;

    // MD5 meshes are collapsed into a single data group
    size_t total_vertices = 0, total_indices = 0, total_weights = 0;
    size_t vertex_offset = 0, index_offset = 0, weight_offset = 0;

    // set to -1 to catch invalid writes
    mdl->num_joints = -1;

    // anything after this point can technically come in any order
    while (*file_buffer) {
        const char *token = COM_Parse(&file_buffer);

        // ignored
        if (!strcmp(token, "commandline")) {
            COM_Parse(&file_buffer);
        } else if (!strcmp(token, "numJoints")) {
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &mdl->num_joints));

            if (mdl->num_joints > MAX_MD5_JOINTS || mdl->num_joints < 0) {
				ret = Q_ERR_INVALID_FORMAT;
                goto fail;
            }

			if (mdl->num_joints > 0) {
				CHECK(mdl->base_skeleton = (md5_joint_t *) MD5_Malloc (mdl->num_joints * sizeof (md5_joint_t)));
                CHECK(*joint_names = (maliasskinname_t *) Z_Malloc (mdl->num_joints * sizeof (maliasskinname_t)));
            }
        } else if (!strcmp(token, "numMeshes")) {
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &num_meshes));
        } else if (!strcmp(token, "joints")) {
            MD5_CHECK(MD5_ParseExpect(&file_buffer, "{"));

			for (int32_t i = 0; i < mdl->num_joints; ++i)
			{
				md5_joint_t *joint = &mdl->base_skeleton[i];
                char *joint_name = ((*joint_names)[i]);

                Q_strlcpy(joint_name, COM_Parse(&file_buffer), sizeof(maliasskinname_t));
                MD5_CHECK(MD5_ParseInt32(&file_buffer, &joint->parent));
                
                MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &joint->pos[0]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &joint->pos[1]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &joint->pos[2]));
                MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));

                MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &joint->orient[0]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &joint->orient[1]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &joint->orient[2]));
                MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));

				Quat_ComputeW (joint->orient);

                joint->scale = 1.0f;
			}

            MD5_CHECK(MD5_ParseExpect(&file_buffer, "}"));
        } else if (!strcmp(token, "mesh")) {
            total_meshes++;

            if (total_meshes > num_meshes) {
				ret = Q_ERR_INVALID_FORMAT;
                goto fail;
            }

            MD5_CHECK(MD5_ParseExpect(&file_buffer, "{"));
            
            // start these at -1 so that we can check for invalid indexing
            int32_t num_verts = -1, num_tris = -1, num_weights = -1;

            // mesh has its own mini-parser
            while (*file_buffer) {
                token = COM_Parse(&file_buffer);

                if (!strcmp(token, "}")) {
                    // mesh finished, update offsets
                    vertex_offset += num_verts;
                    index_offset += num_tris * 3;
                    weight_offset += num_weights;
                    break;
                } else if (!strcmp(token, "shader")) {
                    // ignored
                    COM_Parse(&file_buffer);
                } else if (!strcmp(token, "numverts")) {
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &num_verts));

                    if (num_verts < 0) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    } else if (num_verts) {
                        total_vertices += num_verts;
                        /* Allocate memory for vertices */
						CHECK(vertex_data = (md5_vertex_t *) Z_Realloc (vertex_data, sizeof (md5_vertex_t) * total_vertices));
                    }
                } else if (!strcmp(token, "numtris")) {
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &num_tris));

                    if (num_tris < 0) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    } else if (num_tris) {
                        total_indices += num_tris * 3;
                        /* Allocate memory for triangles */
						CHECK(index_data = (QGL_INDEX_TYPE *) Z_Realloc (index_data, sizeof (QGL_INDEX_TYPE) * total_indices));
                    }
                } else if (!strcmp(token, "numweights")) {
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &num_weights));

                    if (num_weights < 0) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    } else if (num_weights) {
                        total_weights += num_weights;
                        /* Allocate memory for vertex weights */
						CHECK(weight_data = (md5_weight_t *) Z_Realloc (weight_data, sizeof (md5_weight_t) * total_weights));
                    }
                } else if (!strcmp(token, "vert")) {
                    int32_t vert_index;

                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &vert_index));

                    if (vert_index < 0 || vert_index >= num_verts) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    }

                    md5_vertex_t *vert = &vertex_data[vert_index + vertex_offset];

                    MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                    MD5_CHECK(MD5_ParseFloat(&file_buffer, &vert->st[0]));
                    MD5_CHECK(MD5_ParseFloat(&file_buffer, &vert->st[1]));
                    MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));
                    
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &vert->start));
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &vert->count));

                    vert->start += weight_offset;
                } else if (!strcmp(token, "tri")) {
                    int32_t tri_index;

                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &tri_index));

                    if (tri_index < 0 || tri_index >= num_tris) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    }

                    QGL_INDEX_TYPE *triangle = &index_data[(tri_index * 3) + index_offset];
                    
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &triangle[0]));
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &triangle[1]));
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &triangle[2]));

                    for (int32_t i = 0; i < 3; i++) {
                        triangle[i] += vertex_offset;
                    }
                } else if (!strcmp(token, "weight")) {
                    int32_t weight_index;

                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &weight_index));

                    if (weight_index < 0 || weight_index >= num_weights) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    }

                    md5_weight_t *weight = &weight_data[weight_index + weight_offset];
                    
                    MD5_CHECK(MD5_ParseInt32(&file_buffer, &weight->joint));

                    if (weight->joint < 0 || weight->joint >= mdl->num_joints) {
				        ret = Q_ERR_INVALID_FORMAT;
                        goto fail;
                    }

                    MD5_CHECK(MD5_ParseFloat(&file_buffer, &weight->bias));
                
                    MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                    MD5_CHECK(MD5_ParseFloat(&file_buffer, &weight->pos[0]));
                    MD5_CHECK(MD5_ParseFloat(&file_buffer, &weight->pos[1]));
                    MD5_CHECK(MD5_ParseFloat(&file_buffer, &weight->pos[2]));
                    MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));
                } else if (!*token) {
                    // assume EOF; invalid tokens will just be ignored
				    ret = Q_ERR_INVALID_FORMAT;
                    goto fail;
                }
            }
        } else if (!*token) {
            // assume EOF; invalid tokens will just be ignored
            break;
        }
    }

    // check integrity of data; this has to be done last
    // because of circular data dependencies
    for (size_t i = 0; i < mdl->num_verts; i++) {
        md5_vertex_t *vert = &vertex_data[i];

        if (vert->start < 0 || vert->start >= total_weights ||
            vert->count < 0 || (vert->start + vert->count) > total_weights) {
			ret = Q_ERR_INVALID_FORMAT;
            goto fail;
        }
    }
    
    for (size_t i = 0; i < mdl->num_indices; i++) {
        QGL_INDEX_TYPE *index = &index_data[i];

        if (*index < 0 || *index >= total_vertices) {
			ret = Q_ERR_INVALID_FORMAT;
            goto fail;
        }
    }

    // copy out the final data
    CHECK(mdl->indices = MD5_Malloc(sizeof(QGL_INDEX_TYPE) * total_indices));
    memcpy(mdl->indices, index_data, sizeof(QGL_INDEX_TYPE) * total_indices);

    CHECK(mdl->vertices = MD5_Malloc(sizeof(md5_vertex_t) * total_vertices));
    memcpy(mdl->vertices, vertex_data, sizeof(md5_vertex_t) * total_vertices);

    CHECK(mdl->weights = MD5_Malloc(sizeof(md5_weight_t) * total_weights));
    memcpy(mdl->weights, weight_data, sizeof(md5_weight_t) * total_weights);

    mdl->num_verts = total_vertices;
    mdl->num_indices = total_indices;
    mdl->num_weights = total_weights;

fail:
    Z_Free(index_data);
    Z_Free(vertex_data);

	return ret;
}

/* Joint info */
typedef struct {
	int32_t parent;
	int32_t flags;
	int32_t start_index;
} joint_info_t;

/* Base frame joint */
typedef struct {
	vec3_t pos;
	quat_t orient;
} baseframe_joint_t;
        
#define MD5_COMPONENT_TX BIT(0)
#define MD5_COMPONENT_TY BIT(1)
#define MD5_COMPONENT_TZ BIT(2)
#define MD5_COMPONENT_QX BIT(3)
#define MD5_COMPONENT_QY BIT(4)
#define MD5_COMPONENT_QZ BIT(5)

#define MD5_NUM_ANIMATED_COMPONENT_BITS 6

/**
 * Build skeleton for a given frame data.
 */
static inline void MD5_BuildFrameSkeleton(const joint_info_t *joint_infos,
	const baseframe_joint_t *base_frame, const float *anim_frame_data,
	md5_joint_t *skeleton_frame, int num_joints)
{
	for (int32_t i = 0; i < num_joints; ++i) {
		const baseframe_joint_t *baseJoint = &base_frame[i];
        float components[7];

        float *animated_position = components + 0;
        float *animated_quat = components + 3;

        VectorCopy(baseJoint->pos, animated_position);
        VectorCopy(baseJoint->orient, animated_quat); // W will be re-calculated below

        for (int32_t c = 0, j = 0; c < MD5_NUM_ANIMATED_COMPONENT_BITS; c++) {
            if (joint_infos[i].flags & BIT(c)) {
                components[c] = anim_frame_data[joint_infos[i].start_index + j++];
            }
        }

		Quat_ComputeW(animated_quat);

        // parent should already be calculated
		md5_joint_t *thisJoint = &skeleton_frame[i];
        thisJoint->scale = 1.0f;

		int parent = thisJoint->parent = joint_infos[i].parent;

		if (thisJoint->parent < 0)
		{
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

/*
================
MD5_ComputeNormals
================
*/
static void MD5_ComputeNormals (md5_weight_t *weights, md5_joint_t *base, md5_vertex_t *vert, size_t numverts, QGL_INDEX_TYPE *indexes, size_t numindexes)
{
	hash_map_t *pos_to_normal_map = HashMap_Create (vec3_t, vec3_t, &HashVec3, NULL);
	HashMap_Reserve (pos_to_normal_map, numverts);

	for (size_t v = 0; v < numverts; v++) {
        VectorClear(vert[v].normal);
    }

	for (size_t t = 0; t < numindexes; t += 3) {
		md5_vertex_t *verts[3] = {&vert[indexes[t + 0]], &vert[indexes[t + 1]], &vert[indexes[t + 2]]};

        vec3_t xyz[3];

        for (size_t i = 0; i < 3; i++) {
		    vec3_t finalVertex = { 0.0f, 0.0f, 0.0f };

		    /* Calculate final vertex to draw with weights */
		    for (size_t j = 0; j < verts[i]->count; ++j) {
			    const md5_weight_t *weight = &weights[verts[i]->start + j];
			    const md5_joint_t *joint = &base[weight->joint];

			      /* Calculate transformed vertex for this weight */
			    vec3_t wv;
			    Quat_RotatePoint(joint->orient, weight->pos, wv);

			    /* The sum of all weight->bias should be 1.0 */
                VectorAdd(joint->pos, wv, wv);
                VectorMA(finalVertex, weight->bias, wv, finalVertex);
		    }

            VectorCopy(finalVertex, xyz[i]);
        }

		vec3_t d1, d2;
		VectorSubtract (xyz[2], xyz[0], d1);
		VectorSubtract (xyz[1], xyz[0], d2);
		VectorNormalize (d1);
		VectorNormalize (d2);

		vec3_t norm;
		CrossProduct (d1, d2, norm);
		VectorNormalize (norm);

		const float angle = acos (DotProduct (d1, d2));
		VectorScale (norm, angle, norm);

		vec3_t *found_normal;
		for (int i = 0; i < 3; ++i) {
			if ((found_normal = HashMap_Lookup (vec3_t, pos_to_normal_map, &xyz[i])))
				VectorAdd (norm, *found_normal, *found_normal);
			else
				HashMap_Insert (pos_to_normal_map, &xyz[i], &norm);
		}
	}

	const uint32_t map_size = HashMap_Size (pos_to_normal_map);
	for (uint32_t i = 0; i < map_size; ++i) {
		vec3_t *norm = HashMap_GetValue (vec3_t, pos_to_normal_map, i);
		VectorNormalize (*norm);
	}

	for (size_t v = 0; v < numverts; v++) {
		vec3_t finalVertex = { 0.0f, 0.0f, 0.0f };

		/* Calculate final vertex to draw with weights */
		for (size_t j = 0; j < vert[v].count; ++j) {
			const md5_weight_t *weight = &weights[vert[v].start + j];
			const md5_joint_t *joint = &base[weight->joint];

			    /* Calculate transformed vertex for this weight */
			vec3_t wv;
			Quat_RotatePoint(joint->orient, weight->pos, wv);

			/* The sum of all weight->bias should be 1.0 */
            VectorAdd(joint->pos, wv, wv);
            VectorMA(finalVertex, weight->bias, wv, finalVertex);
		}

		vec3_t *norm = HashMap_Lookup (vec3_t, pos_to_normal_map, &finalVertex);
		if (norm) {
            // Put the bind-pose normal into joint-local space
            // so the animated normal can be computed faster later
            for ( int j = 0; j < vert[v].count; ++j ) {
			    const md5_weight_t *weight = &weights[vert[v].start + j];
			    const md5_joint_t *joint = &base[weight->joint];
			    vec3_t wv;
			    Quat_RotatePoint(joint->orient, *norm, wv);
                VectorMA(vert[v].normal, weight->bias, wv, vert[v].normal);
            }
        }
	}

	HashMap_Destroy (pos_to_normal_map);
}

/**
 * Load an MD5 animation from file.
 */
static int MOD_LoadMD5Anim(model_t *model, const char *anim_path, const char *file_buffer)
{
    int ret = 0;

    // parse header
    if (!MOD_LoadMD5Header(model, &file_buffer, &ret)) {
        goto fail;
    }
    
    md5_model_t *anim = model->skeleton;
    
    // temporary info for building skeleton frames
	joint_info_t *joint_infos = NULL;
	baseframe_joint_t *base_frame = NULL;
	float *anim_frame_data = NULL;

    int32_t num_animated_components = -1;

    // initial invalid value, to catch out of bounds errors
    anim->num_frames = -1;

    // anything after this point can technically come in any order
    while (*file_buffer) {
        const char *token = COM_Parse(&file_buffer);

        // ignored
        if (!strcmp(token, "commandline")) {
            COM_Parse(&file_buffer);
        } else if (!strcmp(token, "numFrames")) {
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &anim->num_frames));

            // md5 replacements need at least 1 frame, because the
            // pose frame isn't used
            if (anim->num_frames <= 0) {
				ret = Q_ERR_INVALID_FORMAT;
                goto fail;
            }

            // warn on mismatched frame counts (not fatal)
            if (anim->num_frames != model->numframes) {
                Com_WPrintf("%s doesn't match frame count for %s (%i vs %i)\n", anim_path, model->name, model->skeleton->num_frames, model->numframes);
            }

            if (anim->num_frames > 0 && anim->num_joints > 0 && !anim->skeleton_frames) {
    			CHECK(anim->skeleton_frames = (md5_joint_t *) MD5_Malloc (sizeof (md5_joint_t) * anim->num_frames * anim->num_joints));
			    CHECK(joint_infos = (joint_info_t *) Z_Malloc (sizeof (joint_info_t) * anim->num_joints));
			    CHECK(base_frame = (baseframe_joint_t *) Z_Malloc (sizeof (baseframe_joint_t) * anim->num_joints));
            }
        } else if (!strcmp(token, "numJoints")) {
            int32_t num_joints;
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &num_joints));

            if (anim->num_joints != num_joints) {
                ret = Q_ERR_INVALID_FORMAT;
                goto fail;
            }

            if (anim->num_frames > 0 && anim->num_joints > 0 && !anim->skeleton_frames) {
    			CHECK(anim->skeleton_frames = (md5_joint_t *) MD5_Malloc (sizeof (md5_joint_t) * anim->num_frames * anim->num_joints));
			    CHECK(joint_infos = (joint_info_t *) Z_Malloc (sizeof (joint_info_t) * anim->num_joints));
			    CHECK(base_frame = (baseframe_joint_t *) Z_Malloc (sizeof (baseframe_joint_t) * anim->num_joints));
            }
        } else if (!strcmp(token, "frameRate")) {
            // ignored
            int32_t frame_rate;
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &frame_rate));
        } else if (!strcmp(token, "numAnimatedComponents")) {
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &num_animated_components));

            if (num_animated_components < 0) {
                ret = Q_ERR_INVALID_FORMAT;
                goto fail;
            }

			if (num_animated_components > 0) {
				CHECK(anim_frame_data = (float *) Z_Malloc (sizeof (float) * num_animated_components));
			}
		} else if (!strcmp(token, "hierarchy")) {
            MD5_CHECK(MD5_ParseExpect(&file_buffer, "{"));

			for (int32_t i = 0; i < anim->num_joints; ++i) {
                joint_info_t *joint_info = &joint_infos[i];

                COM_Parse(&file_buffer); // ignore name
                MD5_CHECK(MD5_ParseInt32(&file_buffer, &joint_info->parent));
                MD5_CHECK(MD5_ParseInt32(&file_buffer, &joint_info->flags));
                MD5_CHECK(MD5_ParseInt32(&file_buffer, &joint_info->start_index));

                // validate animated components
                int32_t num_components = 0;

                for (int32_t f = 0; f < MD5_NUM_ANIMATED_COMPONENT_BITS; f++) {
                    if (joint_info->flags & BIT(f)) {
                        num_components++;
                    }
                }

                if (joint_info->start_index < 0 || joint_info->start_index >= num_animated_components ||
                    (joint_info->start_index + num_components) < 0 || (joint_info->start_index + num_components) > num_animated_components) {
                    ret = Q_ERR_INVALID_FORMAT;
                    goto fail;
                }

                // validate parents; they need to match the base skeleton
                if (joint_info->parent != anim->base_skeleton[i].parent) {
                    ret = Q_ERR_INVALID_FORMAT;
                    goto fail;
                }
			}

            MD5_CHECK(MD5_ParseExpect(&file_buffer, "}"));
		} else if (!strcmp(token, "bounds")) {
            // bounds are ignored and are apparently usually wrong anyways
            // so we'll just rely on them being "replacement" md2s/md3s.
            // the md2/md3 ones are used instead.
            MD5_CHECK(MD5_ParseExpect(&file_buffer, "{"));

			for (int32_t i = 0; i < anim->num_frames; ++i) {
                for (int32_t b = 0; b < 2; b++) {
                    MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                    for (int32_t v = 0; v < 3; v++) {
                        float x;
                        MD5_CHECK(MD5_ParseFloat(&file_buffer, &x));
                    }
                    MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));
                }
			}

            MD5_CHECK(MD5_ParseExpect(&file_buffer, "}"));
		} else if (!strcmp(token, "baseframe")) {
            MD5_CHECK(MD5_ParseExpect(&file_buffer, "{"));

			for (int32_t i = 0; i < anim->num_joints; ++i) {
                baseframe_joint_t *base_joint = &base_frame[i];
                
                MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &base_joint->pos[0]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &base_joint->pos[1]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &base_joint->pos[2]));
                MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));

                MD5_CHECK(MD5_ParseExpect(&file_buffer, "("));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &base_joint->orient[0]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &base_joint->orient[1]));
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &base_joint->orient[2]));
                MD5_CHECK(MD5_ParseExpect(&file_buffer, ")"));

                Quat_ComputeW(base_joint->orient);
			}

            MD5_CHECK(MD5_ParseExpect(&file_buffer, "}"));
		} else if (!strcmp(token, "frame")) {
            int32_t frame_index = -1;
            MD5_CHECK(MD5_ParseInt32(&file_buffer, &frame_index));

            if (frame_index < 0 || frame_index >= anim->num_frames) {
                ret = Q_ERR_INVALID_FORMAT;
                goto fail;
            }
            
            MD5_CHECK(MD5_ParseExpect(&file_buffer, "{"));
			for (int32_t i = 0; i < num_animated_components; ++i) {
                MD5_CHECK(MD5_ParseFloat(&file_buffer, &anim_frame_data[i]));
            }
            MD5_CHECK(MD5_ParseExpect(&file_buffer, "}"));

            /* Build frame skeleton from the collected data */
			MD5_BuildFrameSkeleton(joint_infos, base_frame, anim_frame_data,
				&anim->skeleton_frames[frame_index * anim->num_joints], anim->num_joints);
		} else if (!*token) {
            // assume EOF; invalid tokens will just be ignored
            break;
        }
    }
    
fail:

    Z_Free (anim_frame_data);
	Z_Free (base_frame);
	Z_Free (joint_infos);

	return ret;
}

// skips the current token entirely, making sure that
// current_token will point to the actual next logical
// token to be parsed.
static void MOD_MD5ScaleSkipToken(jsmntok_t *tokens, jsmntok_t **current_token, size_t num_tokens)
{
#if 0
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT = 1,
  JSMN_ARRAY = 2,
  JSMN_STRING = 3,
  JSMN_PRIMITIVE = 4
#endif
    // just in case...
    if ((*current_token - tokens) >= num_tokens) {
        return;
    }

    switch ((*current_token)->type) {
    case JSMN_UNDEFINED:
    case JSMN_STRING:
    case JSMN_PRIMITIVE:
        (*current_token)++;
        break;
    case JSMN_ARRAY: {
        size_t num_to_parse = (*current_token)->size;
        (*current_token)++;
        for (size_t i = 0; i < num_to_parse; i++) {
            MOD_MD5ScaleSkipToken(tokens, current_token, num_tokens);
        }
        break;
    }
    case JSMN_OBJECT: {
        size_t num_to_parse = (*current_token)->size;
        (*current_token)++;
        for (size_t i = 0; i < num_to_parse; i++) {
            (*current_token)++;
            MOD_MD5ScaleSkipToken(tokens, current_token, num_tokens);
        }
    }
    }
}

static void MOD_LoadMD5Scale(model_t *model, const char *name, maliasskinname_t *joint_names)
{
    int ret = 0;

    char model_name[MAX_QPATH], mesh_path[MAX_QPATH];
    COM_SplitPath(name, model_name, sizeof(model_name), mesh_path, sizeof(mesh_path), true);

    char scale_path[MAX_QPATH];
    Q_strlcpy(scale_path, mesh_path, sizeof(mesh_path));
    Q_strlcat(scale_path, "md5/", sizeof(mesh_path));
    Q_strlcat(scale_path, model_name, sizeof(mesh_path));
    Q_strlcat(scale_path, ".md5scale", sizeof(mesh_path));

    // load md5scale (kex extension)
    char *buffer = NULL;
    jsmntok_t *tokens = NULL;

    int32_t buffer_len = FS_LoadFile(scale_path, &buffer);

    if (!buffer)
        return;

    // calculate the total token size so we can grok all of them.
    jsmn_parser p;
        
    jsmn_init(&p);
    size_t num_tokens = jsmn_parse(&p, buffer, buffer_len, NULL, 0);
        
    CHECK(tokens = Z_Malloc(sizeof(jsmntok_t) * num_tokens));

    // decode all tokens
    jsmn_init(&p);
    jsmn_parse(&p, buffer, buffer_len, tokens, num_tokens);

    jsmntok_t *current_token = tokens;

    // check root validity
    MD5_CHECK(current_token->type == JSMN_OBJECT);

    // move to the first object key
    current_token++;

    // parse all of the bone roots
    while (current_token - tokens < num_tokens) {
        MD5_CHECK(current_token->type == JSMN_STRING);

        int32_t joint_id = -1;
        char joint_name[MAX_QPATH];
        Q_strnlcpy(joint_name, buffer + current_token->start, (current_token->end - current_token->start), sizeof(joint_name));

        // move to the object
        current_token++;
        MD5_CHECK(current_token->type == JSMN_OBJECT);

        for (size_t i = 0; i < model->skeleton->num_joints; i++) {
            if (!strcmp(joint_name, joint_names[i])) {
                joint_id = i;
                break;
            }
        }

        if (joint_id == -1) {
            Com_WPrintf("%s: joint \"%s\" is not valid\n", scale_path, joint_name);
            MOD_MD5ScaleSkipToken(tokens, &current_token, num_tokens);
            continue;
        }

        // valid joint, parse the contents
        size_t num_scales_to_parse = current_token->size;

        for (size_t i = 0; i < num_scales_to_parse; i++) {
            current_token++;
            MD5_CHECK(current_token->type == JSMN_STRING);

            // parse frame ID
            char *end_ptr;
            int32_t frame_id = strtol(buffer + current_token->start, &end_ptr, 10);

            MD5_CHECK(end_ptr != (buffer + current_token->start));
            MD5_CHECK(frame_id >= 0 && frame_id < model->skeleton->num_frames);

            // parse scale
            current_token++;
            MD5_CHECK(current_token->type == JSMN_PRIMITIVE);

            float frame_scale = strtof(buffer + current_token->start, &end_ptr);

            MD5_CHECK(end_ptr != (buffer + current_token->start));

            md5_joint_t *frame_ptr = &model->skeleton->skeleton_frames[(frame_id * model->skeleton->num_joints) + joint_id];

            frame_ptr->scale = frame_scale;
        }

        // move to the next key
        current_token++;
    }

fail:
    Z_Free(tokens);

    FS_FreeFile(buffer);

    if (ret) {
        Com_WPrintf("Couldn't load md5scale for %s: %s\n", name,
                    Q_ErrorString(ret));
    }
}

static bool MOD_LoadMD5(model_t *model, const char *name, maliasskinname_t **joint_names)
{
    char model_name[MAX_QPATH], mesh_path[MAX_QPATH];
    COM_SplitPath(name, model_name, sizeof(model_name), mesh_path, sizeof(mesh_path), true);

    // build md5 path
    Q_strlcat(mesh_path, "md5/", sizeof(mesh_path));
    Q_strlcat(mesh_path, model_name, sizeof(mesh_path));

    char anim_path[MAX_QPATH];
    Q_strlcpy(anim_path, mesh_path, sizeof(mesh_path));

    Q_strlcat(mesh_path, ".md5mesh", sizeof(mesh_path));
    Q_strlcat(anim_path, ".md5anim", sizeof(mesh_path));

    // don't bother if we don't have both
    if (!FS_FileExists(mesh_path) || !FS_FileExists(anim_path)) {
        return false;
    }

    // load md5
    char *buffer = NULL;
        
    FS_LoadFile(mesh_path, &buffer);

    if (!buffer)
        goto fail;
    
    int ret = 0;

    // md5 exists!
    if (ret = MOD_LoadMD5Mesh(model, buffer, joint_names)) {
        goto fail;
    }

    FS_FreeFile(buffer);
            
    // load md5anim
    FS_LoadFile(anim_path, &buffer);

    if (!buffer)
        goto fail;

    if (ret = MOD_LoadMD5Anim(model, anim_path, buffer)) {
        goto fail;
    }

    FS_FreeFile(buffer);

    model->skeleton->num_skins = model->meshes[0].numskins;
    CHECK(model->skeleton->skins = MD5_Malloc(sizeof(image_t *) * model->meshes[0].numskins));

    for (size_t i = 0; i < model->meshes[0].numskins; i++) {
        // because skins are actually absolute and
        // not always relative to the model being used,
        // we have to stick to the same behavior.
        char skin_name[MAX_QPATH], skin_path[MAX_QPATH];
        COM_SplitPath(model->meshes[0].skinnames[i], skin_name, sizeof(skin_name), skin_path, sizeof(skin_path), false);

        // build md5 path
        Q_strlcat(skin_path, "md5/", sizeof(skin_path));
        Q_strlcat(skin_path, skin_name, sizeof(skin_path));

        model->skeleton->skins[i] = IMG_Find(skin_path, IT_SKIN, IF_NONE);
    }

    Hunk_End(&model->skeleton_hunk);

    MD5_ComputeNormals(model->skeleton->weights, model->skeleton->base_skeleton, model->skeleton->vertices, model->skeleton->num_verts, model->skeleton->indices, model->skeleton->num_indices);

    return true;

fail:
    model->skeleton = NULL;

    Hunk_Free(&model->skeleton_hunk);

    FS_FreeFile(buffer);

    Com_EPrintf("Couldn't load MD5 for %s: %s\n", name,
                Q_ErrorString(ret));

    return false;
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
#if USE_MD5
        if (model->skeleton) {
            for (size_t j = 0; j < model->skeleton->num_skins; j++) {
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

done:
#if USE_MD5
    // check for an MD5; this requires the MD2/MD3
    // to have loaded first, since we need it for skin names
    if (gl_load_md5models->integer && model->type == MOD_ALIAS) {
        maliasskinname_t *joint_names = NULL;

        if (MOD_LoadMD5(model, normalized, &joint_names)) {
            MOD_LoadMD5Scale(model, normalized, joint_names);
        }

        Z_Free(joint_names);
    }
#endif


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
