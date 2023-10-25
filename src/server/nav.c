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
// ugly but necessary to hook into nav system without
// exposing this into a mess of spaghetti
#include "../refresh/gl.h"

static cvar_t *nav_debug;
static cvar_t *nav_debug_range;
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

typedef struct nav_ctx_s nav_ctx_t;

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

    nav_ctx_t   *ctx;
} nav_data;

// invalid value used for most of the system
const int32_t INVALID_ID = -1;

// magic file header
const int32_t NAV_MAGIC = MakeLittleLong('N', 'A', 'V', '3');

// last nav version we support
const int32_t NAV_VERSION = 6;

#define NAV_VERIFY(condition, error) \
    if (!(condition)) { Com_SetLastError(error); goto fail; }

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

typedef float (*nav_heuristic_func_t) (const nav_ctx_t *ctx, const nav_node_t *node);
typedef float (*nav_weight_func_t) (const nav_ctx_t *ctx, const nav_node_t *node, const nav_link_t *link);
typedef bool (*nav_link_accessible_func_t) (const nav_ctx_t *ctx, const nav_node_t *node, const nav_link_t *link);

typedef struct nav_ctx_s {
    const PathRequest           *request;

    const nav_node_t            *start, *end;

    nav_heuristic_func_t        heuristic;
    nav_weight_func_t           weight;
    nav_link_accessible_func_t  link_accessible;

    // TODO: min-heap or priority queue ordered by f_score
    int16_t     open_set[1024];

    // TODO: figure out a way to get rid of "came_from"
    // and track start -> end off the bat
    int16_t     *came_from, *went_to;
    float       *g_score, *f_score;
} nav_ctx_t;

static nav_ctx_t *Nav_AllocCtx(void)
{
    size_t size = sizeof(nav_ctx_t) +
        (sizeof(float) * nav_data.num_nodes) +
        (sizeof(float) * nav_data.num_nodes) +
        sizeof(int16_t) * nav_data.num_nodes +
        sizeof(int16_t) * nav_data.num_nodes;
    nav_ctx_t *ctx = Z_TagMalloc(size, TAG_NAV);
    ctx->g_score = (float *) (ctx + 1);
    ctx->f_score = (float *) (ctx->g_score + nav_data.num_nodes);
    ctx->came_from = (int16_t *) (ctx->f_score + nav_data.num_nodes);
    ctx->went_to = (int16_t *) (ctx->came_from + nav_data.num_nodes);

    return ctx;
}

static void Nav_FreeCtx(nav_ctx_t *ctx)
{
    Z_Free(ctx);
}

static float Nav_Heuristic(const nav_ctx_t *ctx, const nav_node_t *node)
{
    vec3_t v;
    VectorSubtract(ctx->end->origin, node->origin, v);
    return VectorLengthSquared(v);
}

static float Nav_Weight(const nav_ctx_t *ctx, const nav_node_t *node, const nav_link_t *link)
{
    if (link->type == NavLinkType_Teleport)
        return 0;

    const nav_node_t *target = &nav_data.nodes[link->target];
    vec3_t v;
    VectorSubtract(node->origin, target->origin, v);
    return VectorLengthSquared(v);
}

static bool Nav_NodeAccessible(const nav_node_t *node)
{
    return !(node->flags & NodeFlag_Disabled);
}

static bool Nav_LinkAccessible(const nav_ctx_t *ctx, const nav_node_t *node, const nav_link_t *link)
{
    return Nav_NodeAccessible(&nav_data.nodes[link->target]);
}

static nav_node_t *Nav_ClosestNodeTo(nav_ctx_t *ctx, const vec3_t p)
{
    float w = INFINITY;
    nav_node_t *c = NULL;

    for (int i = 0; i < nav_data.num_nodes; i++) {
        vec3_t v;
        VectorSubtract(nav_data.nodes[i].origin, p, v);
        float l = VectorLengthSquared(v);

        if (l < w) {
            w = l;
            c = &nav_data.nodes[i];
        }
    }

    return c;
}

static PathInfo Nav_PathCtx(const PathRequest *request, nav_ctx_t *ctx)
{
    PathInfo info = { 0 };

    if (!nav_data.loaded) {
        info.returnCode = PathReturnCode_NoNavAvailable;
        return info;
    }

    ctx->start = Nav_ClosestNodeTo(ctx, request->start);

    if (!ctx->start) {
        info.returnCode = PathReturnCode_NoStartNode;
        return info;
    }

    ctx->end = Nav_ClosestNodeTo(ctx, request->goal);

    if (!ctx->end) {
        info.returnCode = PathReturnCode_NoGoalNode;
        return info;
    }

    if (ctx->start == ctx->end) {
        info.returnCode = PathReturnCode_ReachedGoal;
        return info;
    }

    ctx->request = request;

    int16_t start_id = ctx->start - nav_data.nodes;
    int16_t end_id = ctx->end - nav_data.nodes;

    memset(&ctx->open_set, 0xFF, sizeof(ctx->open_set));

    for (int i = 0; i < nav_data.num_nodes; i++)
        ctx->g_score[i] = ctx->f_score[i] = INFINITY;
    
    ctx->open_set[0] = start_id;
    ctx->came_from[start_id] = -1;
    ctx->g_score[start_id] = 0;
    ctx->f_score[start_id] = ctx->heuristic(ctx, ctx->start);

    while (true) {
        int16_t current_id = -1;

        for (int i = 0; i < 1024; i++) {
            if (ctx->open_set[i] == -1) {
                continue;
            } else if (current_id == -1) {
                current_id = i;
            } else if (ctx->f_score[ctx->open_set[i]] < ctx->f_score[ctx->open_set[current_id]]) {
                current_id = i;
            }
        }

        if (current_id == -1)
            break;

        int16_t current = ctx->open_set[current_id];

        if (current == end_id) {
            if (request->pathPoints.count) {
                info.numPathPoints = 0;
                int16_t n = current;

                while (ctx->came_from[n] != -1) {
                    info.numPathPoints++;
                    n = ctx->came_from[n];
                }

                n = current;
                int64_t p = 0;

                while (ctx->came_from[n] != -1) {

                    int64_t id = info.numPathPoints - p - 1;
                    p++;

                    if (id < request->pathPoints.count)
                        VectorCopy(nav_data.nodes[ctx->came_from[n]].origin, request->pathPoints.posArray[id]);

                    n = ctx->came_from[n];
                }

                if (info.numPathPoints < request->pathPoints.count)
                    VectorCopy(ctx->end->origin, request->pathPoints.posArray[info.numPathPoints]);

                info.numPathPoints++;

                info.returnCode = PathReturnCode_InProgress;
            }

            return info;
        }

        ctx->open_set[current_id] = -1;

        const nav_node_t *current_node = &nav_data.nodes[current];

        for (int l = current_node->first_link; l < current_node->first_link + current_node->num_links; l++) {
            const nav_link_t *link = &nav_data.links[l];

            if (!ctx->link_accessible(ctx, current_node, link))
                continue;

            const nav_node_t *link_target_node = &nav_data.nodes[link->target];

            float temp_g_score = ctx->g_score[current] + ctx->weight(ctx, current_node, link);

            if (temp_g_score >= ctx->g_score[link->target])
                continue;

            ctx->came_from[link->target] = current;
            ctx->went_to[current] = link->target;
            ctx->g_score[link->target] = temp_g_score;
            ctx->f_score[link->target] = temp_g_score + ctx->heuristic(ctx, link_target_node);

            int i;

            for (i = 0; i < 1024; i++)
                if (ctx->open_set[i] == link->target)
                    break;

            if (i == 1024) {
                for (i = 0; i < 1024; i++) {
                    if (ctx->open_set[i] == -1) {
                        ctx->open_set[i] = link->target;
                        break;
                    }
                }
            }
        }
    }
    
    info.returnCode = PathReturnCode_NoPathFound;
    return info;
}

PathInfo Nav_Path(const PathRequest *request)
{
    nav_data.ctx->weight = Nav_Weight;
    nav_data.ctx->heuristic = Nav_Heuristic;
    nav_data.ctx->link_accessible = Nav_LinkAccessible;

    return Nav_PathCtx(request, nav_data.ctx);
}

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
        NAV_VERIFY(node->first_link >= 0 && node->first_link + node->num_links <= nav_data.num_links, "bad node link extents");
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
        NAV_VERIFY(link->target >= 0 && link->target < nav_data.num_nodes, "bad link target");
        NAV_VERIFY_READ(link->type);
        NAV_VERIFY_READ(link->flags);
        NAV_VERIFY_READ(link->traversal);

        if (link->traversal != -1)
            NAV_VERIFY(link->traversal < nav_data.num_traversals, "bad link traversal");
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
        NAV_VERIFY(edict->link >= 0 && edict->link < nav_data.num_links, "bad edict link");
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

    nav_data.ctx = Nav_AllocCtx();

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

#if USE_REF
static inline color_t ColorFromU32(uint32_t c)
{
    return (color_t) { .u32 = c };
}

static inline color_t ColorFromU32A(uint32_t c, uint8_t alpha)
{
    color_t color = { .u32 = c };
    color.u8[3] = alpha;
    return color;
}

static void Nav_Debug(void)
{
    if (!nav_debug->integer) {
        return;
    }

    static vec3_t path_buffer[128];
    PathRequest request = { 0 };
    VectorCopy(glr.fd.vieworg, request.start);
    VectorCopy(vec3_origin, request.goal);
    request.pathFlags = PathFlags_All;
    request.nodeSearch.ignoreNodeFlags = true;
    request.nodeSearch.radius = 128.f;
    request.pathPoints.posArray = &path_buffer[0];
    request.pathPoints.count = q_countof(path_buffer);

    nav_data.ctx->weight = Nav_Weight;
    nav_data.ctx->heuristic = Nav_Heuristic;
    nav_data.ctx->link_accessible = Nav_LinkAccessible;

    PathInfo info = Nav_Path(&request);

    if (info.returnCode < PathReturnCode_StartPathErrors) {

        for (int64_t i = 0; i < info.numPathPoints - 1; i++) {
            R_AddDebugLine(request.pathPoints.posArray[i], request.pathPoints.posArray[i + 1], ColorFromU32A(U32_RED, 255), SV_FRAMETIME, true);
        }
    }

    for (int i = 0; i < nav_data.num_nodes; i++) {
        const nav_node_t *node = &nav_data.nodes[i];
        float len;
        vec3_t d;
        VectorSubtract(node->origin, glr.fd.vieworg, d);
        len = VectorNormalize(d);

        if (len > nav_debug_range->value) {
            continue;
        }

        uint8_t alpha = constclamp((1.0f - ((len - 32.f) / (nav_debug_range->value - 32.f))), 0.0f, 1.0f) * 255.f;

        R_AddDebugCircle(node->origin, node->radius, ColorFromU32A(U32_CYAN, alpha), SV_FRAMETIME, true);
        
        if (i == nav_data.ctx->start - nav_data.nodes)
            R_AddDebugSphere(node->origin, 32, ColorFromU32A(U32_BLUE, alpha), SV_FRAMETIME, true);
        else if (i == nav_data.ctx->end - nav_data.nodes)
            R_AddDebugSphere(node->origin, 32, ColorFromU32A(U32_RED, alpha), SV_FRAMETIME, true);

        vec3_t mins = { -16, -16, -24 }, maxs = { 16, 16, 32 };

        if (node->flags & NodeFlag_Crouch) {
            maxs[2] = 4.0f;
        }
        
        VectorAdd(mins, node->origin, mins);
        VectorAdd(maxs, node->origin, maxs);
        mins[2] += 24.f;
        maxs[2] += 24.f;

        R_AddDebugBounds(mins, maxs, ColorFromU32A(U32_YELLOW, alpha), SV_FRAMETIME, true);

        vec3_t s;
        VectorCopy(node->origin, s);
        s[2] += 24;

        R_AddDebugLine(node->origin, s, ColorFromU32A(U32_CYAN, alpha), SV_FRAMETIME, true);
        
        for (int l = node->first_link; l < node->first_link + node->num_links; l++) {
            const nav_link_t *link = &nav_data.links[l];
            const nav_node_t *target_node = &nav_data.nodes[link->target];
            const byte *bits = nav_data.node_link_bitmap + (nav_data.node_link_bitmap_size * i);

            vec3_t e;
            VectorCopy(target_node->origin, e);
            e[2] += 24;

            // two-way link
            const byte *target_bits = nav_data.node_link_bitmap + (nav_data.node_link_bitmap_size * link->target);

            if (Q_IsBitSet(target_bits, i)) {
                if (i < link->target) {
                    continue;
                }

                R_AddDebugLine(s, e, ColorFromU32A(U32_WHITE, alpha), SV_FRAMETIME, true);
            } else {
                R_AddDebugArrow(s, e, 8.0f, ColorFromU32A(U32_CYAN, alpha), ColorFromU32A(U32_RED, alpha), SV_FRAMETIME, true);
            }
        }
    }

}
#endif

void Nav_Frame(void)
{
#if USE_REF
    Nav_Debug();
#endif
}

void Nav_Init(void)
{
#if USE_REF
    nav_debug = Cvar_Get("nav_debug", "0", 0);
    nav_debug_range = Cvar_Get("nav_debug_range", "512", 0);
#endif
}

void Nav_Shutdown(void)
{
}