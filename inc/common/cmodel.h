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

#pragma once

#include "common/bsp.h"

// bitmasks communicated by server
#define MAX_MAP_AREA_BYTES      (MAX_MAP_AREAS / 8)
#define MAX_MAP_PORTAL_BYTES    128

typedef struct {
    bsp_t       *cache;
    int         *floodnums;     // if two areas have equal floodnums,
                                // they are connected
    bool        *portalopen;
    int         override_bits;
    int         checksum;
    char        *entitystring;
} cm_t;

extern const mleaf_t    nullleaf;

void        CM_Init(void);

void        CM_FreeMap(cm_t *cm);
int         CM_LoadMap(cm_t *cm, const char *name);
void        CM_LoadOverrides(cm_t *cm, char *server, size_t server_size);

const mnode_t   *CM_NodeNum(const cm_t *cm, int number);
const mleaf_t   *CM_LeafNum(const cm_t *cm, int number);

#define CM_InlineModel(cm, name) BSP_InlineModel((cm)->cache, name)

#define CM_NumNode(cm, node) ((node) ? ((node) - (cm)->cache->nodes) : -1)
#define CM_NumLeaf(cm, leaf) ((cm)->cache ? ((leaf) - (cm)->cache->leafs) : 0)

// creates a clipping hull for an arbitrary box
const mnode_t   *CM_HeadnodeForBox(const vec3_t mins, const vec3_t maxs);

// returns an ORed contents mask
static inline int CM_PointContents(const vec3_t p, const mnode_t *headnode)
{
    if (!headnode)
        return 0;   // map not loaded
    return BSP_PointLeaf(headnode, p)->contents;
}

int         CM_TransformedPointContents(const vec3_t p, const mnode_t *headnode,
                                        const vec3_t origin, const vec3_t angles);

void        CM_BoxTrace(trace_t *trace,
                        const vec3_t start, const vec3_t end,
                        const vec3_t mins, const vec3_t maxs,
                        const mnode_t *headnode, int brushmask);
void        CM_TransformedBoxTrace(trace_t *trace,
                                   const vec3_t start, const vec3_t end,
                                   const vec3_t mins, const vec3_t maxs,
                                   const mnode_t *headnode, int brushmask,
                                   const vec3_t origin, const vec3_t angles);
void        CM_ClipEntity(trace_t *dst, const trace_t *src, struct edict_s *ent);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int CM_BoxLeafs_headnode(const vec3_t mins, const vec3_t maxs,
                         const mleaf_t **list, int listsize,
                         const mnode_t *headnode, const mnode_t **topnode);

static inline int CM_BoxLeafs(const cm_t *cm, const vec3_t mins, const vec3_t maxs,
                              const mleaf_t **list, int listsize, const mnode_t **topnode)
{
    if (!cm->cache)
        return 0;   // map not loaded
    return CM_BoxLeafs_headnode(mins, maxs, list, listsize, cm->cache->nodes, topnode);
}

static inline const mleaf_t *CM_PointLeaf(const cm_t *cm, const vec3_t p)
{
    if (!cm->cache)
        return &nullleaf;   // map not loaded
    return BSP_PointLeaf(cm->cache->nodes, p);
}

byte        *CM_FatPVS(const cm_t *cm, byte *mask, const vec3_t org);

void        CM_SetAreaPortalState(const cm_t *cm, int portalnum, bool open);
bool        CM_AreasConnected(const cm_t *cm, int area1, int area2);

int         CM_WriteAreaBits(const cm_t *cm, byte *buffer, int area);
int         CM_WritePortalBits(const cm_t *cm, byte *buffer);
void        CM_SetPortalStates(const cm_t *cm, const byte *buffer, int bytes);
bool        CM_HeadnodeVisible(const mnode_t *headnode, const byte *visbits);
