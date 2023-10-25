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

#pragma once

// flags that determine conditionals for nodes
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

typedef struct nav_link_s nav_link_t;

// cached node data
typedef struct {
    nav_node_flags_t	flags;
    int16_t             num_links;
    nav_link_t          *links;
    int16_t             id;
    int16_t             radius;
    vec3_t              origin;
} nav_node_t;

// link type
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

// link flags
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

// cached traversal data
typedef struct {
    vec3_t  funnel;
    vec3_t  start;
    vec3_t  end;
    vec3_t  ladder_plane;
} nav_traversal_t;

typedef struct nav_edict_s nav_edict_t;

// cached link data
typedef struct nav_link_s {
    nav_node_t          *target;
    nav_link_type_t     type;
    nav_link_flags_t    flags;
    nav_traversal_t     *traversal;
    nav_edict_t         *edict;
} nav_link_t;

// cached entity data
typedef struct nav_edict_s {
    nav_link_t       *link;
    int32_t          model;
    vec3_t           mins;
    vec3_t           maxs;
    const edict_t    *game_edict;
} nav_edict_t;

// navigation context; holds data specific to pathing.
// these can be re-used between level loads, but are
// automatically freed when the level changes.
// a NULL context can be passed to any functions expecting
// one, which will refer to a built-in context instead.
typedef struct nav_ctx_s nav_ctx_t;

nav_ctx_t *Nav_AllocCtx(void);
void Nav_FreeCtx(nav_ctx_t *ctx);

// pathing functions
typedef struct nav_path_s nav_path_t;

typedef float (*nav_heuristic_func_t) (const nav_path_t *path, const nav_node_t *node);
typedef float (*nav_weight_func_t) (const nav_path_t *path, const nav_node_t *node, const nav_link_t *link);
typedef bool (*nav_link_accessible_func_t) (const nav_path_t *path, const nav_node_t *node, const nav_link_t *link);

// wrapper for PathRequest that includes our
// additional data
typedef struct nav_path_s {
    // if any of these are null, it uses the built-in defaults
    // in, may be null
    nav_heuristic_func_t        heuristic;
    nav_weight_func_t           weight;
    nav_link_accessible_func_t  link_accessible;
    nav_ctx_t                   *context;

    // in, non-null
    const PathRequest           *request;

    // out
    const nav_node_t            *start, *goal;
} nav_path_t;

PathInfo Nav_Path(nav_path_t *path);

// life cycle stuff
void Nav_Load(const char *map_name);
void Nav_Unload(void);
void Nav_Frame(void);
void Nav_Init(void);
void Nav_Shutdown(void);

// entity stuff
void Nav_RegisterEdict(const edict_t *edict);
void Nav_UnRegisterEdict(const edict_t *edict);