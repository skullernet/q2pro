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

#define MOD_GpuMalloc(size) \
    Hunk_TryAlloc(&model->hunk, size, gl_static.hunk_align)

#define MOD_CpuMalloc(size) \
    (gl_static.use_gpu_lerp ? R_Mallocz(size) : MOD_GpuMalloc(size))

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

    for (i = 0, model = r_models; i < r_numModels; i++, model++)
        if (!model->type)
            break;

    if (i == r_numModels) {
        if (r_numModels == MAX_RMODELS)
            return NULL;
        r_numModels++;
    }

    return model;
}

static model_t *MOD_Find(const char *name)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type)
            continue;
        if (!FS_pathcmp(model->name, name))
            return model;
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
        if (!model->type)
            continue;

        size_t model_size = model->hunk.mapped;
        int flag = ' ';
#if USE_MD5
        if (model->skeleton)
            flag = '*';
#endif
        Com_Printf("%c%c %8zu : %s\n", types[model->type],
                   flag, model_size, model->name);
        bytes += model_size;
        count++;
    }
    Com_Printf("Total models: %d (out of %d slots)\n", count, r_numModels);
    Com_Printf("Total resident: %zu\n", bytes);
}

#if USE_MD5
static void MD5_Free(md5_model_t *mdl);
#endif

static void MOD_FreeAlias(model_t *model)
{
    Hunk_Free(&model->hunk);

    GL_DeleteBuffer(model->buffer);

    // all memory is allocated on hunk if not using GPU lerp
    if (!gl_static.use_gpu_lerp)
        return;

#if USE_MD5
    MD5_Free(model->skeleton);
#endif

    for (int i = 0; i < model->nummeshes; i++) {
        Z_Free(model->meshes[i].skins);
#if USE_MD5
        Z_Free(model->meshes[i].skinnames);
#endif
    }

    Z_Free(model->meshes);
    Z_Free(model->frames);
}

static void MOD_Free(model_t *model)
{
    switch (model->type) {
    case MOD_SPRITE:
        Z_Free(model->spriteframes);
        break;
    case MOD_ALIAS:
        MOD_FreeAlias(model);
        break;
    case MOD_EMPTY:
    case MOD_FREE:
        break;
    default:
        Q_assert(!"bad model type");
    }

    memset(model, 0, sizeof(*model));
}

void MOD_FreeUnused(void)
{
    model_t *model;
    int i, count = 0;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type)
            continue;

        if (model->registration_sequence == r_registration_sequence) {
            // make sure it is paged in
            Com_PageInMemory(model->hunk.base, model->hunk.cursize);
        } else {
            // don't need this model
            MOD_Free(model);
            count++;
        }
    }

    if (count)
        Com_DPrintf("%s: %i models freed\n", __func__, count);
}

void MOD_FreeAll(void)
{
    model_t *model;
    int i, count = 0;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (model->type) {
            MOD_Free(model);
            count++;
        }
    }

    if (count)
        Com_DPrintf("%s: %i models freed\n", __func__, count);

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
        Com_SetLastError("Too many frames");
        return Q_ERR_INVALID_FORMAT;
    }
    if (sizeof(dsp2header_t) + sizeof(dsp2frame_t) * header.numframes > length) {
        Com_SetLastError("Frames out of bounds");
        return Q_ERR_INVALID_FORMAT;
    }

    model->type = MOD_SPRITE;
    model->spriteframes = R_Malloc(sizeof(model->spriteframes[0]) * header.numframes);
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
            dst_frame->image = IMG_Find(buffer, IT_SPRITE, IF_NONE);
        }

        src_frame++;
        dst_frame++;
    }

    return Q_ERR_SUCCESS;
}

static const char *MOD_ValidateMD2(const dmd2header_t *header, size_t length)
{
    ENSURE(header->num_tris <= TESS_MAX_INDICES / 3, "Too many tris");
    ENSURE(header->num_st <= INT_MAX / sizeof(dmd2stvert_t), "Too many st");
    ENSURE(header->num_xyz <= MD2_MAX_VERTS, "Too many xyz");
    ENSURE(header->num_frames <= MD2_MAX_FRAMES, "Too many frames");
    ENSURE(header->num_skins <= MD2_MAX_SKINS, "Too many skins");

    ENSURE(header->framesize >= sizeof(dmd2frame_t) + (header->num_xyz - 1) * sizeof(dmd2trivertx_t), "Too small frame size");
    ENSURE(header->framesize <= MD2_MAX_FRAMESIZE, "Too big frame size");

    ENSURE((uint64_t)header->ofs_tris + header->num_tris * sizeof(dmd2triangle_t) <= length, "Bad tris offset");
    ENSURE((uint64_t)header->ofs_st + header->num_st * sizeof(dmd2stvert_t) <= length, "Bad st offset");
    ENSURE((uint64_t)header->ofs_frames + header->num_frames * header->framesize <= length, "Bad frames offset");
    ENSURE((uint64_t)header->ofs_skins + MD2_MAX_SKINNAME * header->num_skins <= length, "Bad skins offset");

    ENSURE(!(header->ofs_tris % q_alignof(dmd2triangle_t)), "Odd tris offset");
    ENSURE(!(header->ofs_st % q_alignof(dmd2stvert_t)), "Odd st offset");
    ENSURE(!(header->ofs_frames % q_alignof(dmd2frame_t)), "Odd frames offset");
    ENSURE(!(header->framesize % q_alignof(dmd2frame_t)), "Odd frame size");

    ENSURE(header->skinwidth >= 1 && header->skinwidth <= MAX_TEXTURE_SIZE, "Bad skin width");
    ENSURE(header->skinheight >= 1 && header->skinheight <= MAX_TEXTURE_SIZE, "Bad skin height");
    return NULL;
}

static bool MOD_AllocMesh(model_t *model, maliasmesh_t *mesh)
{
    if (!(mesh->verts = MOD_GpuMalloc(sizeof(mesh->verts[0]) * mesh->numverts * model->numframes)))
        return false;
    if (!(mesh->tcoords = MOD_GpuMalloc(sizeof(mesh->tcoords[0]) * mesh->numverts)))
        return false;
    if (!(mesh->indices = MOD_GpuMalloc(sizeof(mesh->indices[0]) * mesh->numindices)))
        return false;
    if (!mesh->numskins)
        return true;
    if (!(mesh->skins = MOD_CpuMalloc(sizeof(mesh->skins[0]) * mesh->numskins)))
        return false;
#if USE_MD5
    if (!(mesh->skinnames = MOD_CpuMalloc(sizeof(mesh->skinnames[0]) * mesh->numskins)))
        return false;
#endif
    return true;
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
    int             i, j, k, val;
    uint16_t        remap[TESS_MAX_INDICES];
    uint16_t        vertIndices[TESS_MAX_INDICES];
    uint16_t        tcIndices[TESS_MAX_INDICES];
    uint16_t        finalIndices[TESS_MAX_INDICES];
    int             numverts, numindices;
    vec_t           scale_s, scale_t;
    vec3_t          mins, maxs;
    const char      *err;

    if (length < sizeof(header))
        return Q_ERR_FILE_TOO_SMALL;

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
            if (idx_xyz >= header.num_xyz || idx_st >= header.num_st)
                break;

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
        Com_SetLastError("Too few valid indices");
        return Q_ERR_INVALID_FORMAT;
    }

    for (i = 0; i < numindices; i++)
        remap[i] = 0xFFFF;

    // remap all triangle indices
    numverts = 0;
    src_tc = (dmd2stvert_t *)((byte *)rawdata + header.ofs_st);
    for (i = 0; i < numindices; i++) {
        if (remap[i] != 0xFFFF)
            continue; // already remapped

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
        Com_SetLastError("Too many verts");
        return Q_ERR_INVALID_FORMAT;
    }

    Hunk_Begin(&model->hunk, gl_static.hunk_maxsize);
    model->type = MOD_ALIAS;
    model->nummeshes = 1;
    model->numframes = header.num_frames;
    model->meshes = MOD_CpuMalloc(sizeof(model->meshes[0]));
    model->frames = MOD_CpuMalloc(sizeof(model->frames[0]) * header.num_frames);
    if (!model->meshes || !model->frames)
        return Q_ERR(ENOMEM);

    mesh = model->meshes;
    mesh->numtris = numindices / 3;
    mesh->numindices = numindices;
    mesh->numverts = numverts;
    mesh->numskins = header.num_skins;
    if (!MOD_AllocMesh(model, mesh))
        return Q_ERR(ENOMEM);

    if (mesh->numtris != header.num_tris)
        Com_DPrintf("%s has %d bad triangles\n", model->name, header.num_tris - mesh->numtris);

    // store final triangle indices
    for (i = 0; i < numindices; i++)
        mesh->indices[i] = finalIndices[i];

    // load all skins
    src_skin = (char *)rawdata + header.ofs_skins;
    for (i = 0; i < header.num_skins; i++) {
#if USE_MD5
        char *skinname = mesh->skinnames[i];
#else
        maliasskinname_t skinname;
#endif
        if (!Q_memccpy(skinname, src_skin, 0, sizeof(maliasskinname_t)))
            return Q_ERR_STRING_TRUNCATED;
        mesh->skins[i] = IMG_Find(skinname, IT_SKIN, IF_NONE);
        src_skin += MD2_MAX_SKINNAME;
    }

    // load all tcoords
    src_tc = (dmd2stvert_t *)((byte *)rawdata + header.ofs_st);
    dst_tc = mesh->tcoords;
    scale_s = 1.0f / header.skinwidth;
    scale_t = 1.0f / header.skinheight;
    for (i = 0; i < numindices; i++) {
        if (remap[i] != i)
            continue;
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
            if (remap[i] != i)
                continue;

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

    return Q_ERR_SUCCESS;
}

#if USE_MD3
static const char *MOD_ValidateMD3Mesh(const model_t *model, const dmd3mesh_t *header, size_t length)
{
    ENSURE(header->meshsize >= sizeof(*header) && header->meshsize <= length, "Bad mesh size");
    ENSURE(!(header->meshsize % q_alignof(dmd3mesh_t)), "Odd mesh size");

    ENSURE(header->num_verts >= 3, "Too few verts");
    ENSURE(header->num_verts <= TESS_MAX_VERTICES, "Too many verts");
    ENSURE(header->num_tris >= 1, "Too few tris");
    ENSURE(header->num_tris <= TESS_MAX_INDICES / 3, "Too many tris");
    ENSURE(header->num_skins <= MD3_MAX_SKINS, "Too many skins");

    ENSURE((uint64_t)header->ofs_skins + header->num_skins * sizeof(dmd3skin_t) <= length, "Bad skins offset");
    ENSURE((uint64_t)header->ofs_verts + header->num_verts * model->numframes * sizeof(dmd3vertex_t) <= length, "Bad verts offset");
    ENSURE((uint64_t)header->ofs_tcs + header->num_verts * sizeof(dmd3coord_t) <= length, "Bad tcs offset");
    ENSURE((uint64_t)header->ofs_indexes + header->num_tris * 3 * sizeof(uint32_t) <= length, "Bad indexes offset");

    ENSURE(!(header->ofs_skins % q_alignof(dmd3skin_t)), "Odd skins offset");
    ENSURE(!(header->ofs_verts % q_alignof(dmd3vertex_t)), "Odd verts offset");
    ENSURE(!(header->ofs_tcs % q_alignof(dmd3coord_t)), "Odd tcs offset");
    ENSURE(!(header->ofs_indexes & 3), "Odd indexes offset");
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
    uint16_t        *dst_idx;
    uint32_t        index;
    int             i, j, k;
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
    if (!MOD_AllocMesh(model, mesh))
        return Q_ERR(ENOMEM);

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
            Com_SetLastError("Bad triangle index");
            return Q_ERR_INVALID_FORMAT;
        }
        *dst_idx++ = index;
    }

    *offset_p = header.meshsize;
    return Q_ERR_SUCCESS;
}

static const char *MOD_ValidateMD3(const dmd3header_t *header, size_t length)
{
    ENSURE(header->num_frames >= 1, "Too few frames");
    ENSURE(header->num_frames <= MD3_MAX_FRAMES, "Too many frames");
    ENSURE((uint64_t)header->ofs_frames + header->num_frames * sizeof(dmd3frame_t) <= length, "Bad frames offset");
    ENSURE(!(header->ofs_frames % q_alignof(dmd3frame_t)), "Odd frames offset");
    ENSURE(header->num_meshes >= 1, "Too few meshes");
    ENSURE(header->num_meshes <= MD3_MAX_MESHES, "Too many meshes");
    ENSURE(header->ofs_meshes <= length, "Bad meshes offset");
    ENSURE(!(header->ofs_meshes % q_alignof(dmd3mesh_t)), "Odd meshes offset");
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

    Hunk_Begin(&model->hunk, gl_static.hunk_maxsize);
    model->type = MOD_ALIAS;
    model->numframes = header.num_frames;
    model->nummeshes = header.num_meshes;
    model->meshes = MOD_CpuMalloc(sizeof(model->meshes[0]) * header.num_meshes);
    model->frames = MOD_CpuMalloc(sizeof(model->frames[0]) * header.num_frames);
    if (!model->meshes || !model->frames)
        return Q_ERR(ENOMEM);

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
            return ret;
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

    return Q_ERR_SUCCESS;
}
#endif

static void MOD_PrintError(const char *path, int err)
{
    Com_EPrintf("Couldn't load %s: %s\n", Com_MakePrintable(path),
                err == Q_ERR_INVALID_FORMAT ?
                Com_GetLastError() : Q_ErrorString(err));
}

#if USE_MD5

#include <setjmp.h>

#define JSMN_STATIC
#define JSMN_PARENT_LINKS
#include "jsmn.h"

static jmp_buf md5_jmpbuf;

q_noreturn
static void MD5_ParseError(const char *text)
{
    Com_SetLastError(va("Line %u: %s", com_linenum, text));
    longjmp(md5_jmpbuf, -1);
}

static void *MD5_GpuMalloc(model_t *model, size_t size)
{
    void *ptr = MOD_GpuMalloc(size);
    if (!ptr) {
        Com_SetLastError("Out of memory");
        longjmp(md5_jmpbuf, -1);
    }
    return ptr;
}

static void *MD5_CpuMalloc(model_t *model, size_t size)
{
    return gl_static.use_gpu_lerp ? R_Mallocz(size) : MD5_GpuMalloc(model, size);
}

static void MD5_ParseExpect(const char **buffer, const char *expect)
{
    char *token = COM_Parse(buffer);

    if (strcmp(token, expect))
        MD5_ParseError(va("Expected \"%s\", got \"%s\"", expect, Com_MakePrintable(token)));
}

static float MD5_ParseFloat(const char **buffer)
{
    char *token = COM_Parse(buffer);
    char *endptr;

    float v = strtof(token, &endptr);
    if (endptr == token || *endptr)
        MD5_ParseError(va("Expected float, got \"%s\"", Com_MakePrintable(token)));

    return v;
}

static uint32_t MD5_ParseUint(const char **buffer, uint32_t min_v, uint32_t max_v)
{
    char *token = COM_Parse(buffer);
    char *endptr;

    unsigned long v = strtoul(token, &endptr, 10);
    if (endptr == token || *endptr)
        MD5_ParseError(va("Expected uint, got \"%s\"", Com_MakePrintable(token)));
    if (v < min_v || v > max_v)
        MD5_ParseError(va("Value out of range: %lu", v));

    return v;
}

static int32_t MD5_ParseInt(const char **buffer, int32_t min_v, int32_t max_v)
{
    char *token = COM_Parse(buffer);
    char *endptr;

    long v = strtol(token, &endptr, 10);
    if (endptr == token || *endptr)
        MD5_ParseError(va("Expected int, got \"%s\"", Com_MakePrintable(token)));
    if (v < min_v || v > max_v)
        MD5_ParseError(va("Value out of range: %ld", v));

    return v;
}

static void MD5_ParseVector(const char **buffer, vec3_t output)
{
    MD5_ParseExpect(buffer, "(");
    output[0] = MD5_ParseFloat(buffer);
    output[1] = MD5_ParseFloat(buffer);
    output[2] = MD5_ParseFloat(buffer);
    MD5_ParseExpect(buffer, ")");
}

typedef struct {
    vec3_t pos;
    quat_t orient;
} baseframe_joint_t;

static void MD5_ComputeNormals(md5_mesh_t *mesh, const baseframe_joint_t *base_skeleton)
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
            const baseframe_joint_t *joint = &base_skeleton[mesh->jointnums[vert->start + j]];

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

        float angle = acosf(DotProduct(d1, d2));
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
            // so the animated normal can be computed faster later.
            // Done by transforming the vertex normal by the inverse
            // joint's orientation quaternion of the weight.
            for (j = 0; j < vert->count; j++) {
                const md5_weight_t *weight = &mesh->weights[vert->start + j];
                const baseframe_joint_t *joint = &base_skeleton[mesh->jointnums[vert->start + j]];
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

static bool MD5_ParseMesh(model_t *model, const char *s, const char *path)
{
    baseframe_joint_t base_skeleton[MD5_MAX_JOINTS];
    md5_model_t *mdl;
    int i, j, k;

    if (setjmp(md5_jmpbuf))
        return false;

    com_linenum = 1;

    // parse header
    MD5_ParseExpect(&s, "MD5Version");
    MD5_ParseExpect(&s, "10");

    model->skeleton = mdl = MD5_CpuMalloc(model, sizeof(*mdl));

    MD5_ParseExpect(&s, "commandline");
    COM_SkipToken(&s);

    MD5_ParseExpect(&s, "numJoints");
    mdl->num_joints = MD5_ParseUint(&s, 1, MD5_MAX_JOINTS);

    MD5_ParseExpect(&s, "numMeshes");
    mdl->num_meshes = MD5_ParseUint(&s, 1, MD5_MAX_MESHES);

    MD5_ParseExpect(&s, "joints");
    MD5_ParseExpect(&s, "{");

    for (i = 0; i < mdl->num_joints; i++) {
        baseframe_joint_t *joint = &base_skeleton[i];

        // skip name
        COM_SkipToken(&s);

        // skip parent
        COM_SkipToken(&s);

        MD5_ParseVector(&s, joint->pos);
        MD5_ParseVector(&s, joint->orient);

        Quat_ComputeW(joint->orient);
    }

    MD5_ParseExpect(&s, "}");

    mdl->meshes = MD5_CpuMalloc(model, mdl->num_meshes * sizeof(mdl->meshes[0]));
    for (i = 0; i < mdl->num_meshes; i++) {
        md5_mesh_t *mesh = &mdl->meshes[i];

        MD5_ParseExpect(&s, "mesh");
        MD5_ParseExpect(&s, "{");

        MD5_ParseExpect(&s, "shader");
        COM_SkipToken(&s);

        MD5_ParseExpect(&s, "numverts");
        mesh->num_verts = MD5_ParseUint(&s, 0, TESS_MAX_VERTICES);
        mesh->vertices  = MD5_GpuMalloc(model, mesh->num_verts * sizeof(mesh->vertices[0]));
        mesh->tcoords   = MD5_GpuMalloc(model, mesh->num_verts * sizeof(mesh->tcoords [0]));

        for (j = 0; j < mesh->num_verts; j++) {
            MD5_ParseExpect(&s, "vert");

            uint32_t vert_index = MD5_ParseUint(&s, 0, mesh->num_verts - 1);

            maliastc_t *tc = &mesh->tcoords[vert_index];
            MD5_ParseExpect(&s, "(");
            tc->st[0] = MD5_ParseFloat(&s);
            tc->st[1] = MD5_ParseFloat(&s);
            MD5_ParseExpect(&s, ")");

            md5_vertex_t *vert = &mesh->vertices[vert_index];
            vert->start = MD5_ParseUint(&s, 0, UINT16_MAX);
            vert->count = MD5_ParseUint(&s, 0, UINT16_MAX);
        }

        MD5_ParseExpect(&s, "numtris");
        uint32_t num_tris = MD5_ParseUint(&s, 0, TESS_MAX_INDICES / 3);
        mesh->indices = MD5_GpuMalloc(model, num_tris * 3 * sizeof(mesh->indices[0]));
        mesh->num_indices = num_tris * 3;

        for (j = 0; j < num_tris; j++) {
            MD5_ParseExpect(&s, "tri");
            uint32_t tri_index = MD5_ParseUint(&s, 0, num_tris - 1);
            for (k = 0; k < 3; k++)
                mesh->indices[tri_index * 3 + k] = MD5_ParseUint(&s, 0, mesh->num_verts - 1);
        }

        MD5_ParseExpect(&s, "numweights");
        mesh->num_weights = MD5_ParseUint(&s, 0, MD5_MAX_WEIGHTS);
        mesh->weights     = MD5_GpuMalloc(model, mesh->num_weights * sizeof(mesh->weights  [0]));
        mesh->jointnums   = MD5_GpuMalloc(model, mesh->num_weights * sizeof(mesh->jointnums[0]));

        for (j = 0; j < mesh->num_weights; j++) {
            MD5_ParseExpect(&s, "weight");

            uint32_t weight_index = MD5_ParseUint(&s, 0, mesh->num_weights - 1);
            mesh->jointnums[weight_index] = MD5_ParseUint(&s, 0, mdl->num_joints - 1);

            md5_weight_t *weight = &mesh->weights[weight_index];
            weight->bias = MD5_ParseFloat(&s);
            MD5_ParseVector(&s, weight->pos);
        }

        MD5_ParseExpect(&s, "}");

        // check integrity of data; this has to be done last
        // because of circular data dependencies
        for (j = 0; j < mesh->num_verts; j++) {
            md5_vertex_t *vert = &mesh->vertices[j];
            if (vert->start + vert->count > mesh->num_weights)
                MD5_ParseError("Bad vert start/count");
        }

        MD5_ComputeNormals(mesh, base_skeleton);
    }

    return true;
}

typedef struct {
    char name[MD5_MAX_JOINTNAME];
    int parent, flags, start_index;
    bool scale_pos;
} joint_info_t;

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

        for (int c = 0, j = 0; c < MD5_NUM_ANIMATED_COMPONENT_BITS; c++)
            if (joint_infos[i].flags & BIT(c))
                components[c] = anim_frame_data[joint_infos[i].start_index + j++];

        Quat_ComputeW(animated_quat);

        md5_joint_t *thisJoint = &skeleton_frame[i];

        if (joint_infos[i].scale_pos)
            VectorScale(animated_position, thisJoint->scale, animated_position);

        int parent = joint_infos[i].parent;
        if (parent < 0) {
            VectorCopy(animated_position, thisJoint->pos);
            Vector4Copy(animated_quat, thisJoint->orient);
            Quat_ToAxis(thisJoint->orient, thisJoint->axis);
            continue;
        }

        // parent should already be calculated
        Q_assert(parent < i);
        const md5_joint_t *parentJoint = &skeleton_frame[parent];

        // add positions
        vec3_t rotated_pos;
        Quat_RotatePoint(parentJoint->orient, animated_position, rotated_pos);
        VectorAdd(rotated_pos, parentJoint->pos, thisJoint->pos);

        // concat rotations
        Quat_MultiplyQuat(parentJoint->orient, animated_quat, thisJoint->orient);
        Quat_Normalize(thisJoint->orient);

        Quat_ToAxis(thisJoint->orient, thisJoint->axis);
    }
}

/**
 * Parse some JSON vomit. Don't ask.
 */
static void MD5_LoadScales(const md5_model_t *model, const char *path, joint_info_t *joint_infos)
{
    const jsmntok_t *tok, *end;
    jsmn_parser parser;
    jsmntok_t tokens[4096];
    char *data;
    int len, ret;

    len = FS_LoadFile(path, (void **)&data);
    if (!data) {
        if (len != Q_ERR(ENOENT))
            MOD_PrintError(path, len);
        return;
    }

    jsmn_init(&parser);
    ret = jsmn_parse(&parser, data, len, tokens, q_countof(tokens));
    if (ret < 0)
        goto fail;
    if (ret == 0)
        goto skip;

    tok = &tokens[0];
    if (tok->type != JSMN_OBJECT)
        goto fail;

    end = tokens + ret;
    tok++;

    while (tok < end) {
        if (tok->type != JSMN_STRING)
            goto fail;

        int joint_id = -1;
        const char *joint_name = data + tok->start;

        data[tok->end] = 0;
        for (int i = 0; i < model->num_joints; i++) {
            if (!strcmp(joint_name, joint_infos[i].name)) {
                joint_id = i;
                break;
            }
        }

        if (joint_id == -1)
            Com_WPrintf("No such joint \"%s\" in %s\n", Com_MakePrintable(joint_name), path);

        if (++tok == end || tok->type != JSMN_OBJECT)
            goto fail;

        int num_keys = tok->size;
        if (end - ++tok < num_keys * 2)
            goto fail;

        for (int i = 0; i < num_keys; i++) {
            const jsmntok_t *key = tok++;
            const jsmntok_t *val = tok++;
            if (key->type != JSMN_STRING || val->type != JSMN_PRIMITIVE)
                goto fail;

            if (joint_id == -1)
                continue;

            data[key->end] = 0;
            if (!strcmp(data + key->start, "scale_positions")) {
                joint_infos[joint_id].scale_pos = data[val->start] == 't';
            } else {
                unsigned frame_id = Q_atoi(data + key->start);
                if (frame_id < model->num_frames)
                    model->skeleton_frames[frame_id * model->num_joints + joint_id].scale = Q_atof(data + val->start);
                else
                    Com_WPrintf("No such frame %d in %s\n", frame_id, path);
            }
        }
    }

skip:
    FS_FreeFile(data);
    return;

fail:
    Com_EPrintf("Couldn't load %s: Invalid JSON data\n", path);
    FS_FreeFile(data);
}

/**
 * Load an MD5 animation from file.
 */
static bool MD5_ParseAnim(model_t *model, const char *s, const char *path)
{
    joint_info_t joint_infos[MD5_MAX_JOINTS];
    baseframe_joint_t base_frame[MD5_MAX_JOINTS];
    float anim_frame_data[MD5_MAX_JOINTS * MD5_NUM_ANIMATED_COMPONENT_BITS];
    int num_joints, num_animated_components;
    md5_model_t *mdl = model->skeleton;
    int i, j;

    if (setjmp(md5_jmpbuf))
        return false;

    com_linenum = 1;

    // parse header
    MD5_ParseExpect(&s, "MD5Version");
    MD5_ParseExpect(&s, "10");

    MD5_ParseExpect(&s, "commandline");
    COM_SkipToken(&s);

    MD5_ParseExpect(&s, "numFrames");
    // MD5 replacements need at least 1 frame, because the
    // pose frame isn't used
    mdl->num_frames = MD5_ParseUint(&s, 1, MD5_MAX_FRAMES);

    // warn on mismatched frame counts (not fatal)
    if (mdl->num_frames < model->numframes)
        Com_WPrintf("%s has less frames than %s (%i < %i)\n", path,
                    model->name, mdl->num_frames, model->numframes);

    MD5_ParseExpect(&s, "numJoints");
    num_joints = MD5_ParseUint(&s, 1, MD5_MAX_JOINTS);
    if (num_joints != mdl->num_joints)
        MD5_ParseError("Bad numJoints");

    MD5_ParseExpect(&s, "frameRate");
    COM_SkipToken(&s);

    MD5_ParseExpect(&s, "numAnimatedComponents");
    num_animated_components = MD5_ParseUint(&s, 0, q_countof(anim_frame_data));

    MD5_ParseExpect(&s, "hierarchy");
    MD5_ParseExpect(&s, "{");

    for (i = 0; i < mdl->num_joints; i++) {
        joint_info_t *joint_info = &joint_infos[i];

        COM_ParseToken(&s, joint_info->name, sizeof(joint_info->name));

        joint_info->parent      = MD5_ParseInt (&s, -1, mdl->num_joints - 1);
        joint_info->flags       = MD5_ParseUint(&s,  0, UINT32_MAX);
        joint_info->start_index = MD5_ParseUint(&s,  0, num_animated_components);
        joint_info->scale_pos   = false;

        // validate animated components
        int num_components = 0;

        for (j = 0; j < MD5_NUM_ANIMATED_COMPONENT_BITS; j++)
            if (joint_info->flags & BIT(j))
                num_components++;

        if (joint_info->start_index + num_components > num_animated_components)
            MD5_ParseError("Bad joint info");

        // parent must be -1 or already processed joint
        if (joint_info->parent >= i)
            MD5_ParseError("Bad parent joint");
    }

    MD5_ParseExpect(&s, "}");

    // bounds are ignored and are apparently usually wrong anyways
    // so we'll just rely on them being "replacement" MD2s/MD3s.
    // the MD2/MD3 ones are used instead.
    MD5_ParseExpect(&s, "bounds");
    MD5_ParseExpect(&s, "{");

    for (i = 0; i < mdl->num_frames * 2 * 5; i++)
        COM_SkipToken(&s);

    MD5_ParseExpect(&s, "}");

    MD5_ParseExpect(&s, "baseframe");
    MD5_ParseExpect(&s, "{");

    for (i = 0; i < mdl->num_joints; i++) {
        baseframe_joint_t *base_joint = &base_frame[i];

        MD5_ParseVector(&s, base_joint->pos);
        MD5_ParseVector(&s, base_joint->orient);

        Quat_ComputeW(base_joint->orient);
    }

    MD5_ParseExpect(&s, "}");

    mdl->skeleton_frames = MD5_CpuMalloc(model, sizeof(mdl->skeleton_frames[0]) * mdl->num_frames * mdl->num_joints);

    // initialize scales
    for (i = 0; i < mdl->num_frames * mdl->num_joints; i++)
        mdl->skeleton_frames[i].scale = 1.0f;

    // load scales
    char scale_path[MAX_QPATH];
    if (COM_StripExtension(scale_path, path, sizeof(scale_path)) < sizeof(scale_path) &&
        Q_strlcat(scale_path, ".md5scale", sizeof(scale_path)) < sizeof(scale_path))
        MD5_LoadScales(model->skeleton, scale_path, joint_infos);
    else
        Com_WPrintf("MD5 scale path too long: %s\n", scale_path);

    for (i = 0; i < mdl->num_frames; i++) {
        MD5_ParseExpect(&s, "frame");

        uint32_t frame_index = MD5_ParseUint(&s, 0, mdl->num_frames - 1);

        MD5_ParseExpect(&s, "{");
        for (j = 0; j < num_animated_components; j++)
            anim_frame_data[j] = MD5_ParseFloat(&s);
        MD5_ParseExpect(&s, "}");

        /* Build frame skeleton from the collected data */
        MD5_BuildFrameSkeleton(joint_infos, base_frame, anim_frame_data,
                               &mdl->skeleton_frames[frame_index * mdl->num_joints], mdl->num_joints);
    }

    return true;
}

static bool MD5_LoadFile(model_t *model, const char *path, bool (*parse)(model_t *, const char *, const char *))
{
    void *data;
    int ret = FS_LoadFile(path, &data);
    if (!data) {
        MOD_PrintError(path, ret);
        return false;
    }

    ret = parse(model, data, path);
    FS_FreeFile(data);
    if (!ret) {
        MOD_PrintError(path, Q_ERR_INVALID_FORMAT);
        return false;
    }

    return true;
}

static bool MD5_LoadSkins(model_t *model)
{
    md5_model_t *mdl = model->skeleton;
    const maliasmesh_t *mesh = &model->meshes[0];

    if (!mesh->numskins)
        return true;

    mdl->num_skins = mesh->numskins;
    mdl->skins = MOD_CpuMalloc(sizeof(mdl->skins[0]) * mdl->num_skins);
    if (!mdl->skins) {
        Com_EPrintf("Out of memory for MD5 skins\n");
        return false;
    }

    for (int i = 0; i < mesh->numskins; i++) {
        // because skins are actually absolute and not always relative to the
        // model being used, we have to stick to the same behavior.
        char skin_name[MAX_QPATH], skin_path[MAX_QPATH];

        COM_SplitPath(mesh->skinnames[i], skin_name, sizeof(skin_name), skin_path, sizeof(skin_path), false);

        // build MD5 path
        if (Q_strlcat(skin_path, "md5/", sizeof(skin_path)) < sizeof(skin_path) &&
            Q_strlcat(skin_path, skin_name, sizeof(skin_path)) < sizeof(skin_path)) {
            mdl->skins[i] = IMG_Find(skin_path, IT_SKIN, IF_NONE);
        } else {
            Com_WPrintf("MD5 skin path too long: %s\n", skin_path);
            mdl->skins[i] = R_NOTEXTURE;
        }
    }

    return true;
}

static void MD5_Free(md5_model_t *mdl)
{
    if (!mdl || !gl_static.use_gpu_lerp)
        return;
    Z_Free(mdl->meshes);
    Z_Free(mdl->skeleton_frames);
    Z_Free(mdl->skins);
    Z_Free(mdl);
}

static void MOD_LoadMD5(model_t *model)
{
    char model_name[MAX_QPATH], base_path[MAX_QPATH];
    char mesh_path[MAX_QPATH], anim_path[MAX_QPATH];

    COM_SplitPath(model->name, model_name, sizeof(model_name), base_path, sizeof(base_path), true);

    if (Q_concat(mesh_path, sizeof(mesh_path), base_path, "md5/", model_name, ".md5mesh") >= sizeof(mesh_path) ||
        Q_concat(anim_path, sizeof(anim_path), base_path, "md5/", model_name, ".md5anim") >= sizeof(anim_path))
        return;

    // don't bother if we don't have both
    if (!FS_FileExists(mesh_path) || !FS_FileExists(anim_path))
        return;

    size_t watermark = model->hunk.cursize;

    if (!MD5_LoadFile(model, mesh_path, MD5_ParseMesh))
        goto fail;
    if (!MD5_LoadFile(model, anim_path, MD5_ParseAnim))
        goto fail;
    if (!MD5_LoadSkins(model))
        goto fail;

    return;

fail:
    MD5_Free(model->skeleton);
    model->skeleton = NULL;
    Hunk_FreeToWatermark(&model->hunk, watermark);
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
            for (j = 0; j < mesh->numskins; j++)
                mesh->skins[j]->registration_sequence = r_registration_sequence;
        }
#if USE_MD5
        if (model->skeleton)
            for (j = 0; j < model->skeleton->num_skins; j++)
                model->skeleton->skins[j]->registration_sequence = r_registration_sequence;
#endif
        break;
    case MOD_SPRITE:
        for (i = 0; i < model->numframes; i++)
            model->spriteframes[i].image->registration_sequence = r_registration_sequence;
        break;
    case MOD_EMPTY:
        break;
    default:
        Q_assert(!"bad model type");
    }

    model->registration_sequence = r_registration_sequence;
}

#define FIXUP_OFFSET(ptr) ((ptr) = (void *)((uintptr_t)(ptr) - base))

// upload hunk to GPU and free it
static bool MOD_UploadBuffer(model_t *model)
{
    GL_ClearErrors();
    qglGenBuffers(1, &model->buffer);
    GL_BindBuffer(GL_ARRAY_BUFFER, model->buffer);
    qglBufferData(GL_ARRAY_BUFFER, model->hunk.cursize, model->hunk.base, GL_STATIC_DRAW);
    if (GL_ShowErrors(__func__))
        return false;

    const uintptr_t base = (uintptr_t)model->hunk.base;

    for (int i = 0; i < model->nummeshes; i++) {
        maliasmesh_t *mesh = &model->meshes[i];
        FIXUP_OFFSET(mesh->verts);
        FIXUP_OFFSET(mesh->tcoords);
        FIXUP_OFFSET(mesh->indices);
    }

#if USE_MD5
    const md5_model_t *skel = model->skeleton;
    if (skel) {
        for (int i = 0; i < skel->num_meshes; i++) {
            md5_mesh_t *mesh = &skel->meshes[i];
            FIXUP_OFFSET(mesh->vertices);
            FIXUP_OFFSET(mesh->tcoords);
            FIXUP_OFFSET(mesh->indices);
            FIXUP_OFFSET(mesh->weights);
            FIXUP_OFFSET(mesh->jointnums);
        }
    }
#endif

    size_t mapped = model->hunk.mapped;
    Hunk_Free(&model->hunk);
    model->hunk.mapped = mapped;    // for statistics

    return true;
}

qhandle_t R_RegisterModel(const char *name)
{
    char normalized[MAX_QPATH];
    qhandle_t index;
    size_t namelen;
    model_t *model;
    byte *rawdata;
    int (*load)(model_t *, const void *, size_t);
    int ret;

    Q_assert(name);

    // empty names are legal, silently ignore them
    if (!*name)
        return 0;

    if (*name == '*') {
        // inline bsp model
        index = Q_atoi(name + 1);
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

    ret = FS_LoadFile(normalized, (void **)&rawdata);
    if (!rawdata) {
        // don't spam about missing models
        if (ret == Q_ERR(ENOENT))
            return 0;
        goto fail1;
    }

    if (ret < 4) {
        ret = Q_ERR_FILE_TOO_SMALL;
        goto fail2;
    }

    // check ident
    switch (LittleLong(*(uint32_t *)rawdata)) {
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
    model->registration_sequence = r_registration_sequence;

    ret = load(model, rawdata, ret);

    FS_FreeFile(rawdata);

    if (ret < 0) {
        MOD_Free(model);
        goto fail1;
    }

#if USE_MD5
    // check for an MD5; this requires the MD2/MD3
    // to have loaded first, since we need it for skin names
    if (model->type == MOD_ALIAS && gl_md5_load->integer)
        MOD_LoadMD5(model);
#endif

    Hunk_End(&model->hunk);

    if (model->type == MOD_ALIAS && gl_static.use_gpu_lerp && !MOD_UploadBuffer(model)) {
        MOD_Free(model);
        ret = Q_ERR_LIBRARY_ERROR;
        goto fail1;
    }

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

    if (!h)
        return NULL;

    Q_assert(h > 0 && h <= r_numModels);
    model = &r_models[h - 1];
    if (!model->type)
        return NULL;

    return model;
}

void MOD_Init(void)
{
    Q_assert(!r_numModels);

    // set defaults
    gl_static.use_gpu_lerp = false;
    gl_static.hunk_align   = 64;
    gl_static.hunk_maxsize = MOD_MAXSIZE_CPU;

    cvar_t *gl_gpulerp = Cvar_Get("gl_gpulerp", "1", 0);
    gl_gpulerp->flags &= ~CVAR_FILES;

    if (!(gl_config.caps & QGL_CAP_CLIENT_VA)) {
        // MUST use GPU lerp if using core profile
        Q_assert(gl_static.use_shaders);
        gl_static.use_gpu_lerp = true;
    } else if (gl_static.use_shaders) {
        // restrict `auto' to GL 4.3 and higher
        int minval = 1 + !(gl_config.caps & QGL_CAP_SHADER_STORAGE);
        gl_static.use_gpu_lerp = gl_gpulerp->integer >= minval;
        gl_gpulerp->flags |= CVAR_FILES;
    }

    // can reserve more space if using GPU lerp
    if (gl_static.use_gpu_lerp)
        gl_static.hunk_maxsize = MOD_MAXSIZE_GPU;

#if USE_MD5
    // prefer shader storage, but support buffer textures as fallback.
    // if neither are supported and GPU lerp is enabled, disable MD5.
    if (gl_static.use_gpu_lerp && gl_md5_load->integer) {
        if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
            gl_static.hunk_align = max(16, gl_config.ssbo_align);
        } else if (!(gl_config.caps & QGL_CAP_BUFFER_TEXTURE)) {
            Com_WPrintf("Animating MD5 models on GPU is not supported "
                        "on this system. MD5 models will be disabled.\n");
            Cvar_Set("gl_md5_load", "0");
        }
    }
#endif

    Com_DPrintf("GPU lerp %s\n", gl_static.use_gpu_lerp ?
                (gl_config.caps & QGL_CAP_SHADER_STORAGE) ? "enabled (shader storage)" :
                (gl_config.caps & QGL_CAP_BUFFER_TEXTURE) ? "enabled (buffer texture)" :
                "enabled" : "disabled");

    Cmd_AddCommand("modellist", MOD_List_f);
}

void MOD_Shutdown(void)
{
    MOD_FreeAll();
    Cmd_RemoveCommand("modellist");
}
