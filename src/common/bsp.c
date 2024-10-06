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

// bsp.c -- model loading

#include "shared/shared.h"
#include "shared/list.h"
#include "common/bsp.h"
#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/intreadwrite.h"
#include "common/math.h"
#include "common/mdfour.h"
#include "common/sizebuf.h"
#include "common/utils.h"
#include "system/hunk.h"

extern mtexinfo_t nulltexinfo;

static cvar_t *map_visibility_patch;

/*
===============================================================================

                    LUMP LOADING

===============================================================================
*/

#define BSP_ALIGN   64

#define BSP_ALLOC(size) \
    Hunk_Alloc(&bsp->hunk, size, BSP_ALIGN)

#define BSP_ERROR(msg) \
    Com_SetLastError(va("%s: %s", __func__, msg))

#define BSP_ENSURE(cond, msg) \
    do { if (!(cond)) { BSP_ERROR(msg); return Q_ERR_INVALID_FORMAT; } } while (0)

#define BSP_EXTENDED 0
#include "bsp_template.c"

#define BSP_EXTENDED 1
#include "bsp_template.c"

/*
===============================================================================

                    MAP LOADING

===============================================================================
*/

typedef struct {
    const char *name;
    void (*load)(bsp_t *, const byte *, size_t);
    size_t (*parse_header)(bsp_t *, const byte *, size_t);
} xlump_info_t;

typedef struct {
    int (*load[2])(bsp_t *const, const byte *, const size_t);
    const char *name;
    uint8_t lump;
    uint8_t disksize[2];
    uint32_t memsize;
} lump_info_t;

typedef struct {
    int ofs;
    const char *name;
} bsp_stat_t;

#define L(name, lump, mem_t, disksize1, disksize2) \
    { { BSP_Load##name, BSP_Load##name      }, #name, lump, { disksize1, disksize2 }, sizeof(mem_t) }

#define E(name, lump, mem_t, disksize1, disksize2) \
    { { BSP_Load##name, BSP_Load##name##Ext }, #name, lump, { disksize1, disksize2 }, sizeof(mem_t) }

static const lump_info_t bsp_lumps[] = {
    L(Visibility,    3, byte,            1,  1),
    L(Texinfo,       5, mtexinfo_t,     76, 76),
    L(Planes,        1, cplane_t,       20, 20),
    E(BrushSides,   15, mbrushside_t,    4,  8),
    L(Brushes,      14, mbrush_t,       12, 12),
    E(LeafBrushes,  10, mbrush_t *,      2,  4),
    L(AreaPortals,  18, mareaportal_t,   8,  8),
    L(Areas,        17, marea_t,         8,  8),
#if USE_REF
    L(Lightmap,      7, byte,            1,  1),
    L(Vertices,      2, mvertex_t,      12, 12),
    E(Edges,        11, medge_t,         4,  8),
    L(SurfEdges,    12, msurfedge_t,     4,  4),
    E(Faces,         6, mface_t,        20, 28),
    E(LeafFaces,     9, mface_t *,       2,  4),
#endif
    E(Leafs,         8, mleaf_t,        28, 52),
    E(Nodes,         4, mnode_t,        28, 44),
    L(SubModels,    13, mmodel_t,       48, 48),
    L(EntString,     0, char,            1,  1),
};

#undef L
#undef E

#define F(x) { q_offsetof(bsp_t, num##x), #x }

static const bsp_stat_t bsp_stats[] = {
    F(brushsides),
    F(texinfo),
    F(planes),
    F(nodes),
    F(leafs),
    F(leafbrushes),
    F(models),
    F(brushes),
    F(visibility),
    F(entitychars),
    F(areas),
    F(areaportals),
#if USE_REF
    F(faces),
    F(leaffaces),
    F(lightmapbytes),
    F(vertices),
    F(edges),
    F(surfedges),
#endif
};

#undef F

static list_t   bsp_cache;

static void BSP_PrintStats(const bsp_t *bsp)
{
    bool extended = bsp->extended;

    for (int i = 0; i < q_countof(bsp_stats); i++)
        Com_Printf("%8d : %s\n", *(int *)((byte *)bsp + bsp_stats[i].ofs), bsp_stats[i].name);

    if (bsp->vis)
        Com_Printf("%8u : clusters\n", bsp->vis->numclusters);

#if USE_REF
    const lightgrid_t *grid = &bsp->lightgrid;
    if (grid->numleafs) {
        Com_Printf(
            "%8u : lightgrid styles\n"
            "%8u : lightgrid nodes\n"
            "%8u : lightgrid leafs\n"
            "%8u : lightgrid samples\n",
            grid->numstyles, grid->numnodes, grid->numleafs, grid->numsamples);
        extended = true;
    }
    extended |= bsp->lm_decoupled;
#endif

    if (extended) {
        Com_Printf("Features :");
        if (bsp->extended)
            Com_Printf(" QBSP");
#if USE_REF
        if (bsp->lm_decoupled)
            Com_Printf(" DECOUPLED_LM");
        if (grid->numleafs)
            Com_Printf(" LIGHTGRID_OCTREE");
#endif
        Com_Printf("\n");
    }

    Com_Printf("------------------\n");
}

static void BSP_List_f(void)
{
    bsp_t *bsp;
    size_t bytes;
    bool verbose = Cmd_Argc() > 1;

    if (LIST_EMPTY(&bsp_cache)) {
        Com_Printf("BSP cache is empty\n");
        return;
    }

    Com_Printf("------------------\n");
    bytes = 0;

    LIST_FOR_EACH(bsp_t, bsp, &bsp_cache, entry) {
        Com_Printf("%8zu : %s (%d refs)\n",
                   bsp->hunk.mapped, bsp->name, bsp->refcount);
        if (verbose)
            BSP_PrintStats(bsp);
        bytes += bsp->hunk.mapped;
    }
    Com_Printf("Total resident: %zu\n", bytes);
}

static bsp_t *BSP_Find(const char *name)
{
    bsp_t *bsp;

    LIST_FOR_EACH(bsp_t, bsp, &bsp_cache, entry) {
        if (!FS_pathcmp(bsp->name, name)) {
            return bsp;
        }
    }

    return NULL;
}

static int BSP_SetParent(mnode_t *node, unsigned key)
{
    mnode_t *child;
#if USE_REF
    mface_t *face;
    int i;
#endif

    while (node->plane) {
#if USE_REF
        // a face may never belong to more than one node
        for (i = 0, face = node->firstface; i < node->numfaces; i++, face++) {
            if (face->drawframe) {
                BSP_ERROR("Duplicate face");
                return Q_ERR_INFINITE_LOOP;
            }
            face->drawframe = key;
        }
#endif

        child = node->children[0];
        if (child->parent) {
            BSP_ERROR("Cycle encountered");
            return Q_ERR_INFINITE_LOOP;
        }
        child->parent = node;
        if (BSP_SetParent(child, key)) {
            return Q_ERR_INFINITE_LOOP;
        }

        child = node->children[1];
        if (child->parent) {
            BSP_ERROR("Cycle encountered");
            return Q_ERR_INFINITE_LOOP;
        }
        child->parent = node;
        node = child;
    }

    return Q_ERR_SUCCESS;
}

static int BSP_ValidateTree(bsp_t *bsp)
{
    mmodel_t *mod;
    int i, ret;
#if USE_REF
    mface_t *face;
    int j;
#endif

    for (i = 0, mod = bsp->models; i < bsp->nummodels; i++, mod++) {
        if (i == 0 && mod->headnode != bsp->nodes) {
            BSP_ERROR("Map model 0 headnode is not the first node");
            return Q_ERR_INVALID_FORMAT;
        }

        ret = BSP_SetParent(mod->headnode, ~i);
        if (ret) {
            return ret;
        }

#if USE_REF
        // a face may never belong to more than one model
        for (j = 0, face = mod->firstface; j < mod->numfaces; j++, face++) {
            if (face->drawframe && face->drawframe != ~i) {
                BSP_ERROR("Duplicate face");
                return Q_ERR_INFINITE_LOOP;
            }
            face->drawframe = ~i;
        }
#endif
    }

    return Q_ERR_SUCCESS;
}

// also calculates the last portal number used
// by CM code to allocate portalopen[] array
static int BSP_ValidateAreaPortals(bsp_t *bsp)
{
    mareaportal_t   *p;
    int             i;

    bsp->numportals = 0;
    for (i = 0, p = bsp->areaportals; i < bsp->numareaportals; i++, p++) {
        BSP_ENSURE(p->portalnum < bsp->numareaportals, "Bad portalnum");
        BSP_ENSURE(p->otherarea < bsp->numareas, "Bad otherarea");
        bsp->numportals = max(bsp->numportals, p->portalnum + 1);
    }

    return Q_ERR_SUCCESS;
}

void BSP_Free(bsp_t *bsp)
{
    if (!bsp) {
        return;
    }
    Q_assert(bsp->refcount > 0);
    if (--bsp->refcount == 0) {
        Hunk_Free(&bsp->hunk);
        List_Remove(&bsp->entry);
        Z_Free(bsp);
    }
}

#if USE_CLIENT

int BSP_LoadMaterials(bsp_t *bsp)
{
    char path[MAX_QPATH];
    mtexinfo_t *out, *tex;
    int i, j, step_id = FOOTSTEP_RESERVED_COUNT;
    qhandle_t f;

    for (i = 0, out = bsp->texinfo; i < bsp->numtexinfo; i++, out++) {
        // see if already loaded material for this texinfo
        for (j = i - 1; j >= 0; j--) {
            tex = &bsp->texinfo[j];
            if (!Q_stricmp(tex->name, out->name)) {
                strcpy(out->material, tex->material);
                out->step_id = tex->step_id;
                break;
            }
        }
        if (j != -1)
            continue;

        // load material file
        Q_concat(path, sizeof(path), "textures/", out->name, ".mat");
        FS_OpenFile(path, &f, FS_MODE_READ | FS_FLAG_LOADFILE);
        if (f) {
            FS_Read(out->material, sizeof(out->material) - 1, f);
            FS_CloseFile(f);
        }

        if (out->material[0] && !COM_IsPath(out->material)) {
            Com_WPrintf("Bad material \"%s\" in %s\n", Com_MakePrintable(out->material), path);
            out->material[0] = 0;
        }

        if (!out->material[0] || !Q_stricmp(out->material, "default")) {
            out->step_id = FOOTSTEP_ID_DEFAULT;
            continue;
        }

        if (!Q_stricmp(out->material, "ladder")) {
            out->step_id = FOOTSTEP_ID_LADDER;
            continue;
        }

        // see if already allocated step_id for this material
        for (j = i - 1; j >= 0; j--) {
            tex = &bsp->texinfo[j];
            if (!Q_stricmp(tex->material, out->material)) {
                out->step_id = tex->step_id;
                break;
            }
        }

        // allocate new step_id
        if (j == -1)
            out->step_id = step_id++;
    }

    Com_DPrintf("%s: %d materials loaded\n", __func__, step_id);
    return step_id;
}

#endif

#if USE_REF

#define DECOUPLED_LM_BYTES  40

static void BSP_ParseDecoupledLM(bsp_t *bsp, const byte *in, size_t filelen)
{
    mface_t *out;
    bool errors;

    if (filelen % DECOUPLED_LM_BYTES) {
        Com_WPrintf("DECOUPLED_LM lump has odd size\n");
        return;
    }

    if (bsp->numfaces > filelen / DECOUPLED_LM_BYTES) {
        Com_WPrintf("DECOUPLED_LM lump too short\n");
        return;
    }

    out = bsp->faces;
    errors = false;
    for (int i = 0; i < bsp->numfaces; i++, out++) {
        out->lm_width = BSP_Short();
        out->lm_height = BSP_Short();

        uint32_t offset = BSP_Long();
        if (offset == -1)
            out->lightmap = NULL;
        else if (offset < bsp->numlightmapbytes)
            out->lightmap = bsp->lightmap + offset;
        else {
            out->lightmap = NULL;
            errors = true;
        }

        for (int j = 0; j < 2; j++) {
            BSP_Vector(out->lm_axis[j]);
            out->lm_offset[j] = BSP_Float();
        }
    }

    if (errors)
        Com_WPrintf("DECOUPLED_LM lump possibly corrupted\n");

    bsp->lm_decoupled = true;
}

#define FLAG_OCCLUDED   BIT(30)
#define FLAG_LEAF       BIT(31)

const lightgrid_sample_t *BSP_LookupLightgrid(const lightgrid_t *grid, const uint32_t point[3])
{
    uint32_t nodenum = grid->rootnode;

    while (1) {
        if (nodenum & FLAG_OCCLUDED)
            return NULL;

        if (nodenum & FLAG_LEAF) {
            const lightgrid_leaf_t *leaf = &grid->leafs[nodenum & ~FLAG_LEAF];

            uint32_t pos[3];
            VectorSubtract(point, leaf->mins, pos);

            uint32_t w = leaf->size[0];
            uint32_t h = leaf->size[1];
            uint32_t index = w * (h * pos[2] + pos[1]) + pos[0];
            if (index >= leaf->numsamples)
                return NULL;

            return &grid->samples[leaf->firstsample + index * grid->numstyles];
        }

        const lightgrid_node_t *node = &grid->nodes[nodenum];
        nodenum = node->children[
            (point[0] >= node->point[0]) << 2 |
            (point[1] >= node->point[1]) << 1 |
            (point[2] >= node->point[2]) << 0
        ];
    }
}

// ugh, requires parsing entire thing
static bool BSP_ParseLightgridHeader_(lightgrid_t *grid, sizebuf_t *s)
{
    int i;

    for (i = 0; i < 3; i++)
        grid->scale[i] = 1.0f / SZ_ReadFloat(s);
    for (i = 0; i < 3; i++)
        grid->size[i] = SZ_ReadLong(s);
    for (i = 0; i < 3; i++)
        grid->mins[i] = SZ_ReadFloat(s);

    grid->numstyles = SZ_ReadByte(s);
    if (grid->numstyles - 1 >= MAX_LIGHTMAPS)
        return false;

    grid->rootnode = SZ_ReadLong(s);
    grid->numnodes = SZ_ReadLong(s);
    if (grid->numnodes > SZ_Remaining(s) / 44)
        return false;

    s->readcount += grid->numnodes * 44;
    grid->numleafs = SZ_ReadLong(s);
    if (grid->numleafs - 1 >= SZ_Remaining(s) / 24)
        return false;

    for (i = 0; i < grid->numleafs; i++) {
        uint32_t x, y, z, numsamples;

        s->readcount += 12;
        x = SZ_ReadLong(s);
        y = SZ_ReadLong(s);
        z = SZ_ReadLong(s);

        numsamples = x * y * z;
        grid->numsamples += numsamples;

        while (numsamples--) {
            unsigned numstyles = SZ_ReadByte(s);
            if (numstyles == 255)
                continue;
            if (numstyles > grid->numstyles)
                return false;
            if (!SZ_ReadData(s, sizeof(lightgrid_sample_t) * numstyles))
                return false;
        }
    }

    return true;
}

static size_t BSP_ParseLightgridHeader(bsp_t *bsp, const byte *in, size_t filelen)
{
    lightgrid_t *grid = &bsp->lightgrid;
    sizebuf_t s;

    SZ_InitRead(&s, in, filelen);

    if (!BSP_ParseLightgridHeader_(grid, &s)) {
        Com_WPrintf("Bad LIGHTGRID_OCTREE header\n");
        memset(grid, 0, sizeof(*grid));
        return 0;
    }

    return
        Q_ALIGN(sizeof(grid->nodes[0]) * grid->numnodes, BSP_ALIGN) +
        Q_ALIGN(sizeof(grid->leafs[0]) * grid->numleafs, BSP_ALIGN) +
        Q_ALIGN(sizeof(grid->samples[0]) * grid->numsamples * grid->numstyles, BSP_ALIGN);
}

static bool BSP_ValidateLightgrid_r(const lightgrid_t *grid, uint32_t nodenum)
{
    if (nodenum & FLAG_OCCLUDED)
        return true;

    if (nodenum & FLAG_LEAF)
        return (nodenum & ~FLAG_LEAF) < grid->numleafs;

    if (nodenum >= grid->numnodes)
        return false;

    lightgrid_node_t *node = &grid->nodes[nodenum];

    // until points are loaded use point[0] as visited marker
    if (node->point[0])
        return false;
    node->point[0] = true;

    for (int i = 0; i < 8; i++)
        if (!BSP_ValidateLightgrid_r(grid, node->children[i]))
            return false;

    return true;
}

static void BSP_ParseLightgrid(bsp_t *bsp, const byte *in, size_t filelen)
{
    lightgrid_t *grid = &bsp->lightgrid;
    lightgrid_node_t *node;
    lightgrid_leaf_t *leaf;
    lightgrid_sample_t *sample;
    uint32_t remaining;
    sizebuf_t s;
    byte *data;
    size_t size;
    int i, j;

    if (!grid->numleafs)
        return;

    // ignore if map isn't lit
    if (!bsp->lightmap) {
        Com_WPrintf("Ignoring LIGHTGRID_OCTREE, map isn't lit\n");
        memset(grid, 0, sizeof(*grid));
        return;
    }

    SZ_InitRead(&s, in, filelen);

    grid->nodes = BSP_ALLOC(sizeof(grid->nodes[0]) * grid->numnodes);

    // load children first
    s.readcount = 45;
    for (i = 0, node = grid->nodes; i < grid->numnodes; i++, node++) {
        s.readcount += 12;
        for (j = 0; j < 8; j++)
            node->children[j] = SZ_ReadLong(&s);
    }

    // validate tree
    if (!BSP_ValidateLightgrid_r(grid, grid->rootnode)) {
        Com_WPrintf("Bad LIGHTGRID_OCTREE structure\n");
        memset(grid, 0, sizeof(*grid));
        return;
    }

    // now load points
    s.readcount = 45;
    for (i = 0, node = grid->nodes; i < grid->numnodes; i++, node++) {
        for (j = 0; j < 3; j++)
            node->point[j] = SZ_ReadLong(&s);
        s.readcount += 32;
    }

    grid->leafs = BSP_ALLOC(sizeof(grid->leafs[0]) * grid->numleafs);

    // init samples to fully occluded
    size = sizeof(grid->samples[0]) * grid->numsamples * grid->numstyles;
    grid->samples = sample = memset(BSP_ALLOC(size), 255, size);

    remaining = grid->numsamples;
    s.readcount += 4;
    for (i = 0, leaf = grid->leafs; i < grid->numleafs; i++, leaf++) {
        for (j = 0; j < 3; j++)
            leaf->mins[j] = SZ_ReadLong(&s);
        for (j = 0; j < 3; j++)
            leaf->size[j] = SZ_ReadLong(&s);

        leaf->firstsample = sample - grid->samples;
        leaf->numsamples = leaf->size[0] * leaf->size[1] * leaf->size[2];

        Q_assert(leaf->numsamples <= remaining);
        remaining -= leaf->numsamples;

        for (j = 0; j < leaf->numsamples; j++, sample += grid->numstyles) {
            unsigned numstyles = SZ_ReadByte(&s);
            if (numstyles == 255)
                continue;

            Q_assert(numstyles <= grid->numstyles);
            data = SZ_ReadData(&s, sizeof(*sample) * numstyles);
            Q_assert(data);
            memcpy(sample, data, sizeof(*sample) * numstyles);
        }
    }
}

static const xlump_info_t bspx_lumps[] = {
    { "DECOUPLED_LM", BSP_ParseDecoupledLM },
    { "LIGHTGRID_OCTREE", BSP_ParseLightgrid, BSP_ParseLightgridHeader },
};

// returns amount of extra space to allocate
static size_t BSP_ParseExtensionHeader(bsp_t *bsp, lump_t *out, const byte *buf, uint32_t pos, uint32_t filelen)
{
    pos = Q_ALIGN(pos, 4);
    if (pos > filelen - 8)
        return 0;
    if (RL32(buf + pos) != BSPXHEADER)
        return 0;
    pos += 8;

    uint32_t numlumps = RL32(buf + pos - 4);
    if (numlumps > (filelen - pos) / sizeof(xlump_t)) {
        Com_WPrintf("Bad BSPX header\n");
        return 0;
    }

    size_t extrasize = 0;
    const xlump_t *l = (const xlump_t *)(buf + pos);
    for (int i = 0; i < numlumps; i++, l++) {
        for (int j = 0; j < q_countof(bspx_lumps); j++) {
            const xlump_info_t *e = &bspx_lumps[j];
            uint32_t ofs, len;

            if (strcmp(l->name, e->name))
                continue;

            ofs = LittleLong(l->fileofs);
            len = LittleLong(l->filelen);
            if (len == 0) {
                Com_WPrintf("Ignoring empty %s lump\n", e->name);
                break;
            }

            if ((uint64_t)ofs + len > filelen) {
                Com_WPrintf("Ignoring out of bounds %s lump\n", e->name);
                break;
            }

            if (out[j].filelen) {
                Com_WPrintf("Ignoring duplicate %s lump\n", e->name);
                break;
            }

            Com_DDPrintf("Found %s lump\n", e->name);

            if (e->parse_header)
                extrasize += e->parse_header(bsp, buf + ofs, len);

            out[j].fileofs = ofs;
            out[j].filelen = len;
            break;
        }
    }

    return extrasize;
}

#endif

/*
==================
BSP_Load

Loads in the map and all submodels
==================
*/
int BSP_Load(const char *name, bsp_t **bsp_p)
{
    bsp_t           *bsp;
    byte            *buf;
    dheader_t       *header;
    const lump_info_t *info;
    uint32_t        filelen, ofs, len, count, maxpos;
    int             i, ret;
    uint32_t        lump_ofs[q_countof(bsp_lumps)];
    uint32_t        lump_count[q_countof(bsp_lumps)];
    size_t          memsize;
    bool            extended = false;

    Q_assert(name);
    Q_assert(bsp_p);

    *bsp_p = NULL;

    if (!*name)
        return Q_ERR(ENOENT);

    if ((bsp = BSP_Find(name)) != NULL) {
        Com_PageInMemory(bsp->hunk.base, bsp->hunk.cursize);
        bsp->refcount++;
        *bsp_p = bsp;
        return Q_ERR_SUCCESS;
    }

    //
    // load the file
    //
    filelen = FS_LoadFile(name, (void **)&buf);
    if (!buf) {
        return filelen;
    }

    if (filelen < sizeof(dheader_t)) {
        ret = Q_ERR_FILE_TOO_SMALL;
        goto fail2;
    }

    // byte swap and validate the header
    header = (dheader_t *)buf;
    switch (LittleLong(header->ident)) {
    case IDBSPHEADER:
        break;
    case IDBSPHEADER_EXT:
        extended = true;
        break;
    default:
        ret = Q_ERR_UNKNOWN_FORMAT;
        goto fail2;
    }
    if (LittleLong(header->version) != BSPVERSION) {
        ret = Q_ERR_UNKNOWN_FORMAT;
        goto fail2;
    }

    // byte swap and validate all lumps
    memsize = 0;
    maxpos = 0;
    for (i = 0, info = bsp_lumps; i < q_countof(bsp_lumps); i++, info++) {
        ofs = LittleLong(header->lumps[info->lump].fileofs);
        len = LittleLong(header->lumps[info->lump].filelen);
        if ((uint64_t)ofs + len > filelen) {
            Com_SetLastError(va("%s lump out of bounds", info->name));
            ret = Q_ERR_INVALID_FORMAT;
            goto fail2;
        }
        if (len % info->disksize[extended]) {
            Com_SetLastError(va("%s lump has odd size", info->name));
            ret = Q_ERR_INVALID_FORMAT;
            goto fail2;
        }
        count = len / info->disksize[extended];
        Q_assert(count <= INT_MAX / info->memsize);

        lump_ofs[i] = ofs;
        lump_count[i] = count;

        // account for terminating NUL for EntString lump
        if (!info->lump)
            count++;

        // round to cacheline
        memsize += Q_ALIGN(count * info->memsize, BSP_ALIGN);
        maxpos = max(maxpos, ofs + len);
    }

    // load into hunk
    len = strlen(name);
    bsp = Z_Mallocz(sizeof(*bsp) + len);
    memcpy(bsp->name, name, len + 1);
    bsp->refcount = 1;
    bsp->extended = extended;

#if USE_REF
    lump_t ext[q_countof(bspx_lumps)] = { 0 };
    memsize += BSP_ParseExtensionHeader(bsp, ext, buf, maxpos, filelen);
#endif

    Hunk_Begin(&bsp->hunk, memsize);

    // calculate the checksum
    bsp->checksum = Com_BlockChecksum(buf, filelen);

    // load all lumps
    for (i = 0; i < q_countof(bsp_lumps); i++) {
        ret = bsp_lumps[i].load[extended](bsp, buf + lump_ofs[i], lump_count[i]);
        if (ret) {
            goto fail1;
        }
    }

    ret = BSP_ValidateAreaPortals(bsp);
    if (ret) {
        goto fail1;
    }

    ret = BSP_ValidateTree(bsp);
    if (ret) {
        goto fail1;
    }

#if USE_REF
    // load extension lumps
    for (i = 0; i < q_countof(bspx_lumps); i++) {
        if (ext[i].filelen) {
            bspx_lumps[i].load(bsp, buf + ext[i].fileofs, ext[i].filelen);
        }
    }
#endif

    Hunk_End(&bsp->hunk);

    List_Append(&bsp_cache, &bsp->entry);

    FS_FreeFile(buf);

    *bsp_p = bsp;
    return Q_ERR_SUCCESS;

fail1:
    Hunk_Free(&bsp->hunk);
    Z_Free(bsp);
fail2:
    FS_FreeFile(buf);
    return ret;
}

const char *BSP_ErrorString(int err)
{
    switch (err) {
    case Q_ERR_INVALID_FORMAT:
    case Q_ERR_INFINITE_LOOP:
        return Com_GetLastError();
    default:
        return Q_ErrorString(err);
    }
}

/*
===============================================================================

HELPER FUNCTIONS

===============================================================================
*/

#if USE_REF

static lightpoint_t *light_point;
static int          light_mask;

static bool BSP_RecursiveLightPoint(const mnode_t *node, float p1f, float p2f, const vec3_t p1, const vec3_t p2)
{
    vec_t d1, d2, frac, midf, s, t;
    vec3_t mid;
    int i, side;
    mface_t *surf;

    while (node->plane) {
        // calculate distancies
        d1 = PlaneDiffFast(p1, node->plane);
        d2 = PlaneDiffFast(p2, node->plane);
        side = (d1 < 0);

        if ((d2 < 0) == side) {
            // both points are one the same side
            node = node->children[side];
            continue;
        }

        // find crossing point
        frac = d1 / (d1 - d2);
        midf = p1f + (p2f - p1f) * frac;
        LerpVector(p1, p2, frac, mid);

        // check near side
        if (BSP_RecursiveLightPoint(node->children[side], p1f, midf, p1, mid))
            return true;

        for (i = 0, surf = node->firstface; i < node->numfaces; i++, surf++) {
            if (!surf->lightmap)
                continue;
            if (surf->drawflags & light_mask)
                continue;

            s = DotProduct(surf->lm_axis[0], mid) + surf->lm_offset[0];
            t = DotProduct(surf->lm_axis[1], mid) + surf->lm_offset[1];
            if (s < 0 || s > surf->lm_width - 1)
                continue;
            if (t < 0 || t > surf->lm_height - 1)
                continue;

            light_point->surf = surf;
            light_point->plane = *surf->plane;
            light_point->s = s;
            light_point->t = t;
            light_point->fraction = midf;
            return true;
        }

        // check far side
        return BSP_RecursiveLightPoint(node->children[side ^ 1], midf, p2f, mid, p2);
    }

    return false;
}

void BSP_LightPoint(lightpoint_t *point, const vec3_t start, const vec3_t end, const mnode_t *headnode, int nolm_mask)
{
    light_point = point;
    light_point->surf = NULL;
    light_point->fraction = 1;
    light_mask = nolm_mask;

    BSP_RecursiveLightPoint(headnode, 0, 1, start, end);
}

void BSP_TransformedLightPoint(lightpoint_t *point, const vec3_t start, const vec3_t end,
                               const mnode_t *headnode, int nolm_mask, const vec3_t origin, const vec3_t angles)
{
    vec3_t start_l, end_l;
    vec3_t axis[3];

    light_point = point;
    light_point->surf = NULL;
    light_point->fraction = 1;
    light_mask = nolm_mask;

    // subtract origin offset
    VectorSubtract(start, origin, start_l);
    VectorSubtract(end, origin, end_l);

    // rotate start and end into the models frame of reference
    if (angles) {
        AnglesToAxis(angles, axis);
        RotatePoint(start_l, axis);
        RotatePoint(end_l, axis);
    }

    // sweep the line through the model
    if (!BSP_RecursiveLightPoint(headnode, 0, 1, start_l, end_l))
        return;

    // rotate plane normal into the worlds frame of reference
    if (angles) {
        TransposeAxis(axis);
        RotatePoint(point->plane.normal, axis);
    }

    // offset plane distance
    point->plane.dist += DotProduct(point->plane.normal, origin);
}

#endif

byte *BSP_ClusterVis(const bsp_t *bsp, byte *mask, int cluster, int vis)
{
    byte    *in, *out, *in_end, *out_end;
    int     c;

    Q_assert(vis == DVIS_PVS || vis == DVIS_PHS);

    if (!bsp || !bsp->vis) {
        return memset(mask, 0xff, VIS_MAX_BYTES);
    }
    if (cluster == -1) {
        return memset(mask, 0, bsp->visrowsize);
    }
    if (cluster < 0 || cluster >= bsp->vis->numclusters) {
        Com_Error(ERR_DROP, "%s: bad cluster", __func__);
    }

    // decompress vis
    in_end = (byte *)bsp->vis + bsp->numvisibility;
    in = (byte *)bsp->vis + bsp->vis->bitofs[cluster][vis];
    out_end = mask + bsp->visrowsize;
    out = mask;
    do {
        if (in >= in_end) {
            goto overrun;
        }
        if (*in) {
            *out++ = *in++;
            continue;
        }

        if (in + 1 >= in_end) {
            goto overrun;
        }
        c = in[1];
        in += 2;
        if (c > out_end - out) {
overrun:
            c = out_end - out;
        }
        while (c--) {
            *out++ = 0;
        }
    } while (out < out_end);

    // apply our ugly PVS patches
    if (map_visibility_patch->integer) {
        if (bsp->checksum == 0x1e5b50c5) {
            // q2dm3, pent bridge
            if (cluster == 345 || cluster == 384) {
                Q_SetBit(mask, 466);
                Q_SetBit(mask, 484);
                Q_SetBit(mask, 692);
            }
        } else if (bsp->checksum == 0x04cfa792) {
            // q2dm1, above lower RL
            if (cluster == 395) {
                Q_SetBit(mask, 176);
                Q_SetBit(mask, 183);
            }
        } else if (bsp->checksum == 0x2c3ab9b0) {
            // q2dm8, CG/RG area
            if (cluster == 629 || cluster == 631 ||
                cluster == 633 || cluster == 639) {
                Q_SetBit(mask, 908);
                Q_SetBit(mask, 909);
                Q_SetBit(mask, 910);
                Q_SetBit(mask, 915);
                Q_SetBit(mask, 923);
                Q_SetBit(mask, 924);
                Q_SetBit(mask, 927);
                Q_SetBit(mask, 930);
                Q_SetBit(mask, 938);
                Q_SetBit(mask, 939);
                Q_SetBit(mask, 947);
            }
        } else if (bsp->checksum == 0x2b2ccdd1) {
            // mgu6m2, waterfall
            Q_SetBit(mask, 213);
            Q_SetBit(mask, 214);
            Q_SetBit(mask, 217);
        }
    }

    return mask;
}

const mleaf_t *BSP_PointLeaf(const mnode_t *node, const vec3_t p)
{
    float d;

    while (node->plane) {
        d = PlaneDiffFast(p, node->plane);
        node = node->children[d < 0];
    }

    return (const mleaf_t *)node;
}

/*
==================
BSP_InlineModel
==================
*/
const mmodel_t *BSP_InlineModel(const bsp_t *bsp, const char *name)
{
    int     num;

    Q_assert(bsp);
    Q_assert(name);
    Q_assert(name[0] == '*');

    num = Q_atoi(name + 1);
    if (num < 1 || num >= bsp->nummodels) {
        Com_Error(ERR_DROP, "%s: bad number: %d", __func__, num);
    }

    return &bsp->models[num];
}

void BSP_Init(void)
{
    map_visibility_patch = Cvar_Get("map_visibility_patch", "1", 0);

    Cmd_AddCommand("bsplist", BSP_List_f);

    List_Init(&bsp_cache);
}
