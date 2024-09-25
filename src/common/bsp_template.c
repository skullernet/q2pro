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

// This file must be included twice, first with BSP_EXTENDED = 0 and then
// with BSP_EXTENDED = 1 (strictly in this order).
//
// This code doesn't use structs to allow for unaligned lumps reading.

#if BSP_EXTENDED

#undef BSP_LOAD
#undef BSP_ExtFloat
#undef BSP_ExtLong
#undef BSP_ExtNull

#define BSP_LOAD(func) \
    static int BSP_Load##func##Ext(bsp_t *const bsp, const byte *in, const size_t count)

#define BSP_ExtFloat()  BSP_Float()
#define BSP_ExtLong()   BSP_Long()
#define BSP_ExtNull     (uint32_t)-1

#else

#define BSP_Short()     (in += 2, RL16(in - 2))
#define BSP_Long()      (in += 4, RL32(in - 4))
#define BSP_Float()     LongToFloat(BSP_Long())

#define BSP_LOAD(func) \
    static int BSP_Load##func(bsp_t *const bsp, const byte *in, const size_t count)

#define BSP_ExtFloat()  (int16_t)BSP_Short()
#define BSP_ExtLong()   BSP_Short()
#define BSP_ExtNull     (uint16_t)-1

#define BSP_Vector(v) \
    ((v)[0] = BSP_Float(), (v)[1] = BSP_Float(), (v)[2] = BSP_Float())

#define BSP_ExtVector(v) \
    ((v)[0] = BSP_ExtFloat(), (v)[1] = BSP_ExtFloat(), (v)[2] = BSP_ExtFloat())

BSP_LOAD(Visibility)
{
    if (!count)
        return Q_ERR_SUCCESS;

    BSP_ENSURE(count >= 4, "Too small header");

    uint32_t numclusters = BSP_Long();
    BSP_ENSURE(numclusters <= MAX_MAP_CLUSTERS, "Too many clusters");

    uint32_t hdrsize = 4 + numclusters * 8;
    BSP_ENSURE(count >= hdrsize, "Too small header");

    bsp->numvisibility = count;
    bsp->vis = BSP_ALLOC(count);
    bsp->vis->numclusters = numclusters;
    bsp->visrowsize = (numclusters + 7) >> 3;

    for (int i = 0; i < numclusters; i++) {
        for (int j = 0; j < 2; j++) {
            uint32_t bitofs = BSP_Long();
            BSP_ENSURE(bitofs >= hdrsize && bitofs < count, "Bad bitofs");
            bsp->vis->bitofs[i][j] = bitofs;
        }
    }

    memcpy(bsp->vis->bitofs + numclusters, in, count - hdrsize);

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Texinfo)
{
    mtexinfo_t  *out;

    bsp->numtexinfo = count;
    bsp->texinfo = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
#if USE_REF
        for (int j = 0; j < 2; j++) {
            BSP_Vector(out->axis[j]);
            out->offset[j] = BSP_Float();
        }
#else
        in += 32;
#endif
        out->c.flags = BSP_Long();
        out->c.value = BSP_Long();

        memcpy(out->c.name, in, sizeof(out->c.name) - 1);
        memcpy(out->name, in, sizeof(out->name) - 1);
        in += MAX_TEXNAME;

#if USE_REF
        int32_t next = (int32_t)BSP_Long();
        if (next > 0) {
            BSP_ENSURE(next < count, "Bad anim chain");
            out->next = bsp->texinfo + next;
        } else {
            out->next = NULL;
        }
#else
        in += 4;
#endif
    }

#if USE_REF
    // count animation frames
    out = bsp->texinfo;
    for (int i = 0; i < count; i++, out++) {
        out->numframes = 1;
        for (mtexinfo_t *step = out->next; step && step != out; step = step->next) {
            if (out->numframes == count) {
                BSP_ERROR("Infinite anim chain");
                return Q_ERR_INFINITE_LOOP;
            }
            out->numframes++;
        }
    }
#endif

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Planes)
{
    cplane_t    *out;

    bsp->numplanes = count;
    bsp->planes = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, in += 4, out++) {
        BSP_Vector(out->normal);
        out->dist = BSP_Float();
        SetPlaneType(out);
        SetPlaneSignbits(out);
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Brushes)
{
    mbrush_t    *out;

    bsp->numbrushes = count;
    bsp->brushes = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t firstside = BSP_Long();
        uint32_t numsides = BSP_Long();
        BSP_ENSURE((uint64_t)firstside + numsides <= bsp->numbrushsides, "Bad brushsides");
        out->firstbrushside = bsp->brushsides + firstside;
        out->numsides = numsides;
        out->contents = BSP_Long();
        out->checkcount = 0;
    }

    return Q_ERR_SUCCESS;
}

#if USE_REF
BSP_LOAD(Lightmap)
{
    if (count) {
        bsp->numlightmapbytes = count;
        bsp->lightmap = BSP_ALLOC(count);
        memcpy(bsp->lightmap, in, count);
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Vertices)
{
    mvertex_t   *out;

    bsp->numvertices = count;
    bsp->vertices = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++)
        BSP_Vector(out->point);

    return Q_ERR_SUCCESS;
}

BSP_LOAD(SurfEdges)
{
    msurfedge_t *out;

    bsp->numsurfedges = count;
    bsp->surfedges = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t index = BSP_Long();
        uint32_t vert = index >> 31;
        if (vert)
            index = -index;
        BSP_ENSURE(index < bsp->numedges, "Bad edgenum");
        out->edge = index;
        out->vert = vert;
    }

    return Q_ERR_SUCCESS;
}
#endif

BSP_LOAD(SubModels)
{
    mmodel_t    *out;

    BSP_ENSURE(count > 0, "Map with no models");
    BSP_ENSURE(count <= MAX_MODELS - 2, "Too many models");

    bsp->nummodels = count;
    bsp->models = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        BSP_Vector(out->mins);
        BSP_Vector(out->maxs);
        BSP_Vector(out->origin);

        // spread the mins / maxs by a pixel
        for (int j = 0; j < 3; j++) {
            out->mins[j] -= 1;
            out->maxs[j] += 1;
        }

        uint32_t headnode = BSP_Long();
        if (headnode & BIT(31)) {
            // be careful, some models have no nodes, just a leaf
            headnode = ~headnode;
            BSP_ENSURE(headnode < bsp->numleafs, "Bad headleaf");
            out->headnode = (mnode_t *)(bsp->leafs + headnode);
        } else {
            BSP_ENSURE(headnode < bsp->numnodes, "Bad headnode");
            out->headnode = bsp->nodes + headnode;
        }
#if USE_REF
        if (i == 0) {
            in += 8;
            continue;
        }
        uint32_t firstface = BSP_Long();
        uint32_t numfaces = BSP_Long();
        BSP_ENSURE((uint64_t)firstface + numfaces <= bsp->numfaces, "Bad faces");
        out->firstface = bsp->faces + firstface;
        out->numfaces = numfaces;

        out->radius = RadiusFromBounds(out->mins, out->maxs);
#else
        in += 8;
#endif
    }

    return Q_ERR_SUCCESS;
}

// These are validated after all the areas are loaded
BSP_LOAD(AreaPortals)
{
    mareaportal_t   *out;

    bsp->numareaportals = count;
    bsp->areaportals = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        out->portalnum = BSP_Long();
        out->otherarea = BSP_Long();
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Areas)
{
    marea_t     *out;

    BSP_ENSURE(count <= MAX_MAP_AREAS, "Too many areas");

    bsp->numareas = count;
    bsp->areas = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t numareaportals = BSP_Long();
        uint32_t firstareaportal = BSP_Long();
        BSP_ENSURE((uint64_t)firstareaportal + numareaportals <= bsp->numareaportals, "Bad areaportals");
        out->numareaportals = numareaportals;
        out->firstareaportal = bsp->areaportals + firstareaportal;
        out->floodvalid = 0;
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(EntString)
{
    bsp->numentitychars = count;
    bsp->entitystring = BSP_ALLOC(count + 1);
    memcpy(bsp->entitystring, in, count);
    bsp->entitystring[count] = 0;

    return Q_ERR_SUCCESS;
}

#endif // !BSP_EXTENDED

BSP_LOAD(BrushSides)
{
    mbrushside_t    *out;

    bsp->numbrushsides = count;
    bsp->brushsides = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t planenum = BSP_ExtLong();
        BSP_ENSURE(planenum < bsp->numplanes, "Bad planenum");
        out->plane = bsp->planes + planenum;

        uint32_t texinfo = BSP_ExtLong();
        if (texinfo == BSP_ExtNull) {
            out->texinfo = &nulltexinfo;
        } else {
            BSP_ENSURE(texinfo < bsp->numtexinfo, "Bad texinfo");
            out->texinfo = bsp->texinfo + texinfo;
        }
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(LeafBrushes)
{
    mbrush_t    **out;

    bsp->numleafbrushes = count;
    bsp->leafbrushes = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t brushnum = BSP_ExtLong();
        BSP_ENSURE(brushnum < bsp->numbrushes, "Bad brushnum");
        *out = bsp->brushes + brushnum;
    }

    return Q_ERR_SUCCESS;
}

#if USE_REF
BSP_LOAD(Edges)
{
    medge_t     *out;

    bsp->numedges = count;
    bsp->edges = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        for (int j = 0; j < 2; j++) {
            uint32_t vertnum = BSP_ExtLong();
            BSP_ENSURE(vertnum < bsp->numvertices, "Bad vertnum");
            out->v[j] = vertnum;
        }
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Faces)
{
    mface_t     *out;

    bsp->numfaces = count;
    bsp->faces = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0, j; i < count; i++, out++) {
        uint32_t planenum = BSP_ExtLong();
        BSP_ENSURE(planenum < bsp->numplanes, "Bad planenum");
        out->plane = bsp->planes + planenum;

        out->drawflags = BSP_ExtLong() & DSURF_PLANEBACK;

        uint32_t firstedge = BSP_Long();
        uint32_t numedges = BSP_ExtLong();
        BSP_ENSURE(numedges >= 3 && numedges <= 4096 &&
                   (uint64_t)firstedge + numedges <= bsp->numsurfedges, "Bad surfedges");
        out->firstsurfedge = bsp->surfedges + firstedge;
        out->numsurfedges = numedges;

        uint32_t texinfo = BSP_ExtLong();
        BSP_ENSURE(texinfo < bsp->numtexinfo, "Bad texinfo");
        out->texinfo = bsp->texinfo + texinfo;

        for (j = 0; j < MAX_LIGHTMAPS && in[j] != 255; j++)
            out->styles[j] = in[j];

        for (out->numstyles = j; j < MAX_LIGHTMAPS; j++)
            out->styles[j] = 255;

        in += MAX_LIGHTMAPS;

        uint32_t lightofs = BSP_Long();
        if (lightofs == (uint32_t)-1 || bsp->numlightmapbytes == 0) {
            out->lightmap = NULL;
        } else {
            BSP_ENSURE(lightofs < bsp->numlightmapbytes, "Bad lightofs");
            out->lightmap = bsp->lightmap + lightofs;
        }
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(LeafFaces)
{
    mface_t     **out;

    bsp->numleaffaces = count;
    bsp->leaffaces = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t facenum = BSP_ExtLong();
        BSP_ENSURE(facenum < bsp->numfaces, "Bad facenum");
        *out = bsp->faces + facenum;
    }

    return Q_ERR_SUCCESS;
}
#endif

BSP_LOAD(Leafs)
{
    mleaf_t     *out;

    BSP_ENSURE(count > 0, "Map with no leafs");

    bsp->numleafs = count;
    bsp->leafs = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        out->plane = NULL;
        out->contents = BSP_Long();

        uint32_t cluster = BSP_ExtLong();
        if (cluster == BSP_ExtNull) {
            // solid leafs use special -1 cluster
            out->cluster = -1;
        } else if (bsp->vis == NULL) {
            // map has no vis, use 0 as a default cluster
            out->cluster = 0;
        } else {
            // validate cluster
            BSP_ENSURE(cluster < bsp->vis->numclusters, "Bad cluster");
            out->cluster = cluster;
        }

        uint32_t area = BSP_ExtLong();
        BSP_ENSURE(area < bsp->numareas, "Bad area");
        out->area = area;

#if USE_REF
        BSP_ExtVector(out->mins);
        BSP_ExtVector(out->maxs);
        uint32_t firstleafface = BSP_ExtLong();
        uint32_t numleaffaces = BSP_ExtLong();
        BSP_ENSURE((uint64_t)firstleafface + numleaffaces <= bsp->numleaffaces, "Bad leaffaces");
        out->firstleafface = bsp->leaffaces + firstleafface;
        out->numleaffaces = numleaffaces;

        out->parent = NULL;
        out->visframe = -1;
#else
        in += 16 * (BSP_EXTENDED + 1);
#endif

        uint32_t firstleafbrush = BSP_ExtLong();
        uint32_t numleafbrushes = BSP_ExtLong();
        BSP_ENSURE((uint64_t)firstleafbrush + numleafbrushes <= bsp->numleafbrushes, "Bad leafbrushes");
        out->firstleafbrush = bsp->leafbrushes + firstleafbrush;
        out->numleafbrushes = numleafbrushes;
    }

    BSP_ENSURE(bsp->leafs[0].contents == CONTENTS_SOLID, "Map leaf 0 is not CONTENTS_SOLID");

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Nodes)
{
    mnode_t     *out;

    BSP_ENSURE(count > 0, "Map with no nodes");

    bsp->numnodes = count;
    bsp->nodes = out = BSP_ALLOC(sizeof(*out) * count);

    for (int i = 0; i < count; i++, out++) {
        uint32_t planenum = BSP_Long();
        BSP_ENSURE(planenum < bsp->numplanes, "Bad planenum");
        out->plane = bsp->planes + planenum;

        for (int j = 0; j < 2; j++) {
            uint32_t child = BSP_Long();
            if (child & BIT(31)) {
                child = ~child;
                BSP_ENSURE(child < bsp->numleafs, "Bad leafnum");
                out->children[j] = (mnode_t *)(bsp->leafs + child);
            } else {
                BSP_ENSURE(child < count, "Bad nodenum");
                out->children[j] = bsp->nodes + child;
            }
        }

#if USE_REF
        BSP_ExtVector(out->mins);
        BSP_ExtVector(out->maxs);
        uint32_t firstface = BSP_ExtLong();
        uint32_t numfaces = BSP_ExtLong();
        BSP_ENSURE((uint64_t)firstface + numfaces <= bsp->numfaces, "Bad faces");
        out->firstface = bsp->faces + firstface;
        out->numfaces = numfaces;

        out->parent = NULL;
        out->visframe = -1;
#else
        in += 16 * (BSP_EXTENDED + 1);
#endif
    }

    return Q_ERR_SUCCESS;
}

#undef BSP_EXTENDED
