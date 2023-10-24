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
// nav.c -- Kex navigation node support

#include "server.h"
#include "common/error.h"
#if USE_REF
#include "refresh/refresh.h"
#endif

enum {
    NodeFlag_Normal             = 0,
    NodeFlag_Teleporter         = BIT(0),
    NodeFlag_Pusher             = BIT(1),
    NodeFlag_Elevator           = BIT(2),
    NodeFlag_Ladder             = BIT(3),
    NodeFlag_UnderWater         = BIT(4),
    NodeFlag_CheckForHazard     = BIT(5),
    NodeFlag_CheckHasFloor      = BIT(6),
    NodeFlag_CheckInSolid       = BIT(7),
    NodeFlag_NoMonsters         = BIT(8),
    NodeFlag_Crouch             = BIT(9),
    NodeFlag_NoPOI              = BIT(10),
    NodeFlag_CheckInLiquid      = BIT(11),
    NodeFlag_CheckDoorLinks     = BIT(12),
    NodeFlag_Disabled           = BIT(13)
};

typedef uint16_t nav_node_flags_t;

typedef struct {
    nav_node_flags_t	flags;
    int16_t             num_links;
    int16_t             first_link;
    int16_t             radius;
    vec3_t              origin;
} nav_node_t;

enum {
    NavLinkType_Walk,
    NavLinkType_LongJump,
    NavLinkType_Teleport,
    NavLinkType_WalkOffLedge,
    NavLinkType_Pusher,
    NavLinkType_BarrierJump,
    NavLinkType_Elevator,
    NavLinkType_Train,
    NavLinkType_Manual_LongJump,
    NavLinkType_Crouch,
    NavLinkType_Ladder,
    NavLinkType_Manual_BarrierJump,
    NavLinkType_PivotAndJump,
    NavLinkType_RocketJump,
    NavLinkType_Unknown
};

typedef uint8_t nav_link_type_t;

enum {
    NavLinkFlag_TeamRed         = BIT(0),
    NavLinkFlag_TeamBlue        = BIT(1),
    NavLinkFlag_ExitAtTarget    = BIT(2),
    NavLinkFlag_WalkOnly        = BIT(3),
    NavLinkFlag_EaseIntoTarget  = BIT(4),
    NavLinkFlag_InstantTurn     = BIT(5),
    NavLinkFlag_Disabled        = BIT(6)
};

typedef uint8_t nav_link_flags_t;

typedef struct {
    int16_t             target;
    nav_link_type_t     type;
    nav_link_flags_t    flags;
    int16_t             traversal;
} nav_link_t;

typedef struct {
    vec3_t  funnel;
    vec3_t  start;
    vec3_t  end;
    vec3_t  ladder_plane;
} nav_traversal_t;

typedef struct {
    int16_t     link;
    int32_t     model;
    vec3_t      mins;
    vec3_t      maxs;
} nav_edict_t;

static struct {
    bool	loaded;
    char	filename[MAX_QPATH];

    int32_t     num_nodes;
    int32_t     num_links;
    int32_t     num_traversals;
    int32_t     num_edicts;
    float       heuristic;

    // for quick node lookups
    int32_t     node_link_bitmap_size;
    byte        *node_link_bitmap;
    
    nav_node_t      *nodes;
    nav_link_t      *links;
    nav_traversal_t *traversals;
    nav_edict_t     *edicts;
} nav_data;

// invalid value used for most of the system
const int32_t INVALID_ID = -1;

// magic file header
const int32_t NAV_MAGIC = MakeLittleLong('N', 'A', 'V', '3');

// last nav version we support
const int32_t NAV_VERSION = 6;

#define NAV_VERIFY(condition, error) \
    if (!(condition)) { Com_SetLastError("bad data"); goto fail; }

#define NAV_VERIFY_READ(v) \
    NAV_VERIFY(FS_Read(&v, sizeof(v), f) == sizeof(v), "bad data")

/**
HEADER

i32 magic (NAV3)
i32 version (6)

V6 NAV DATA

i32 num_nodes
i32 num_links
i32 num_traversals
f32 heuristic

struct node
{
    u16 flags
    i16 num_links
    i16 first_link
    i16 radius
}

node[num_nodes] nodes
vec3[num_nodes] node_origins

struct link
{
    i16 target
    u8 type
    u8 flags
    i16 traversal
}

link[num_links] links

struct traversal
{
    vec3 funnel
    vec3 start
    vec3 end
    vec3 ladder_plane
}

traversal[num_traversals] traversals

i32 num_edicts

struct edict
{
    i16 link
    i32 model
    vec3 mins
    vec3 maxs
}

edict[num_edicts] edicts
*/


void Nav_Load(const char *map_name)
{
    Q_assert(!nav_data.loaded);

    nav_data.loaded = true;

    Q_snprintf(nav_data.filename, sizeof(nav_data.filename), "bots/navigation/%s.nav", map_name);

    qhandle_t f;
    int64_t l = FS_OpenFile(nav_data.filename, &f, FS_MODE_READ);

    if (l < 0)
        return;

    int v;

    NAV_VERIFY_READ(v);
    NAV_VERIFY(v == NAV_MAGIC, "bad magic");

    NAV_VERIFY_READ(v);
    NAV_VERIFY(v == NAV_VERSION, "bad version");

    // TODO: support versions 5 to 1 which we may have used in some earlier maps
    NAV_VERIFY_READ(nav_data.num_nodes);
    NAV_VERIFY_READ(nav_data.num_links);
    NAV_VERIFY_READ(nav_data.num_traversals);
    NAV_VERIFY_READ(nav_data.heuristic);

    NAV_VERIFY(nav_data.nodes = Z_TagMalloc(sizeof(nav_node_t) * nav_data.num_nodes, TAG_NAV), "out of memory");

    for (int i = 0; i < nav_data.num_nodes; i++) {
        nav_node_t *node = nav_data.nodes + i;
        
        NAV_VERIFY_READ(node->flags);
        NAV_VERIFY_READ(node->num_links);
        NAV_VERIFY_READ(node->first_link);
        NAV_VERIFY_READ(node->radius);
    }

    for (int i = 0; i < nav_data.num_nodes; i++) {
        nav_node_t *node = nav_data.nodes + i;

        NAV_VERIFY_READ(node->origin);
    }

    NAV_VERIFY(nav_data.links = Z_TagMalloc(sizeof(nav_link_t) * nav_data.num_links, TAG_NAV), "out of memory");

    for (int i = 0; i < nav_data.num_links; i++) {
        nav_link_t *link = nav_data.links + i;
        
        NAV_VERIFY_READ(link->target);
        NAV_VERIFY_READ(link->type);
        NAV_VERIFY_READ(link->flags);
        NAV_VERIFY_READ(link->traversal);
    }

    NAV_VERIFY(nav_data.traversals = Z_TagMalloc(sizeof(nav_traversal_t) * nav_data.num_traversals, TAG_NAV), "out of memory");

    for (int i = 0; i < nav_data.num_traversals; i++) {
        nav_traversal_t *traversal = nav_data.traversals + i;
        
        NAV_VERIFY_READ(traversal->funnel);
        NAV_VERIFY_READ(traversal->start);
        NAV_VERIFY_READ(traversal->end);
        NAV_VERIFY_READ(traversal->ladder_plane);
    }
    
    NAV_VERIFY_READ(nav_data.num_edicts);
    
    NAV_VERIFY(nav_data.edicts = Z_TagMalloc(sizeof(nav_traversal_t) * nav_data.num_traversals, TAG_NAV), "out of memory");

    for (int i = 0; i < nav_data.num_edicts; i++) {
        nav_edict_t *edict = nav_data.edicts + i;
        
        NAV_VERIFY_READ(edict->link);
        NAV_VERIFY_READ(edict->model);
        NAV_VERIFY_READ(edict->mins);
        NAV_VERIFY_READ(edict->maxs);
    }

    nav_data.node_link_bitmap_size = nav_data.num_nodes / CHAR_BIT;
    NAV_VERIFY(nav_data.node_link_bitmap = Z_TagMallocz(nav_data.node_link_bitmap_size * nav_data.num_nodes, TAG_NAV), "out of memory");

    for (int i = 0; i < nav_data.num_nodes; i++) {
        nav_node_t *node = nav_data.nodes + i;
        byte *bits = nav_data.node_link_bitmap + (nav_data.node_link_bitmap_size * i);

        for (int l = node->first_link; l < node->first_link + node->num_links; l++) {
            nav_link_t *link = &nav_data.links[l];

            Q_assert(link->target >= 0);
            
            Q_SetBit(bits, link->target);
        }
    }

    Com_DPrintf("Bot navigation file (%s) loaded:\n %i nodes\n %i links\n %i traversals\n %i edicts\n",
        nav_data.filename, nav_data.num_nodes, nav_data.num_links, nav_data.num_traversals, nav_data.num_edicts);
    return;

fail:
    Com_EPrintf("Couldn't load bot navigation file (%s): %s\n", nav_data.filename, Com_GetLastError());
    Nav_Unload();
}

void Nav_Unload(void)
{
    if (!nav_data.loaded)
        return;

    Z_FreeTags(TAG_NAV);

    memset(&nav_data, 0, sizeof(nav_data));
}

void Nav_Frame(void)
{
#if USE_REF
    for (int i = 0; i < nav_data.num_nodes; i++) {
        R_AddDebugCircle(nav_data.nodes[i].origin, nav_data.nodes[i].radius, (color_t) { .u32 = U32_CYAN }, SV_FRAMETIME, true);

        vec3_t s;
        VectorCopy(nav_data.nodes[i].origin, s);
        s[2] += 24;

        R_AddDebugLine(nav_data.nodes[i].origin, s, (color_t) { .u32 = U32_CYAN }, SV_FRAMETIME, true);
        
        for (int l = nav_data.nodes[i].first_link; l < nav_data.nodes[i].first_link + nav_data.nodes[i].num_links; l++) {
            nav_link_t *link = &nav_data.links[l];
            byte *bits = nav_data.node_link_bitmap + (nav_data.node_link_bitmap_size * i);

            vec3_t e;
            VectorCopy(nav_data.nodes[link->target].origin, e);
            e[2] += 24;

            // two-way link
            byte *target_bits = nav_data.node_link_bitmap + (nav_data.node_link_bitmap_size * link->target);

            if (Q_IsBitSet(target_bits, i)) {
                if (i < link->target) {
                    continue;
                }

                R_AddDebugLine(s, e, (color_t) { .u32 = U32_WHITE }, SV_FRAMETIME, true);
            } else {
                R_AddDebugArrow(s, e, 8.0f, (color_t) { .u32 = U32_CYAN }, (color_t) { .u32 = U32_RED }, SV_FRAMETIME, true);
            }
        }
    }
#endif
}

void Nav_Init(void)
{
}

void Nav_Shutdown(void)
{
}