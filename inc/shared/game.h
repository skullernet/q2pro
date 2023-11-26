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

#include "shared/list.h"
#include "shared/m_flash.h"

#if !defined(GAME3_INCLUDE)
//
// game.h -- game dll information visible to server
//

#define GAME_API_VERSION    2023

#endif // !defined(GAME3_INCLUDE)

// edict->svflags

#define SVF_NOCLIENT            BIT(0)      // don't send entity to clients, even if it has effects
#define SVF_DEADMONSTER         BIT(1)      // treat as CONTENTS_DEADMONSTER for collision
#define SVF_MONSTER             BIT(2)      // treat as CONTENTS_MONSTER for collision

#define SVF_PLAYER              BIT(3)
#define SVF_BOT                 BIT(4)
#define SVF_NOBOTS              BIT(5)
#define SVF_RESPAWNING          BIT(6)
#define SVF_PROJECTILE          BIT(7)
#define SVF_INSTANCED           BIT(8)
#define SVF_DOOR                BIT(9)
#define SVF_NOCULL              BIT(10)
#define SVF_HULL                BIT(11)

typedef uint32_t svflags_t;

// edict->solid values

typedef enum {
    SOLID_NOT,          // no interaction with other objects
    SOLID_TRIGGER,      // only touch when inside, after moving
    SOLID_BBOX,         // touch on edge
    SOLID_BSP           // bsp clip, touch on edge
} solid_t;

// extended features

// R1Q2 and Q2PRO specific
#define GMF_CLIENTNUM               BIT(0)      // game sets clientNum gclient_s field
#define GMF_PROPERINUSE             BIT(1)      // game maintains edict_s inuse field properly
#define GMF_MVDSPEC                 BIT(2)      // game is dummy MVD client aware
#define GMF_WANT_ALL_DISCONNECTS    BIT(3)      // game wants ClientDisconnect() for non-spawned clients

// Q2PRO specific
#define GMF_ENHANCED_SAVEGAMES      BIT(10)     // game supports safe/portable savegames
#define GMF_VARIABLE_FPS            BIT(11)     // game supports variable server FPS
#define GMF_EXTRA_USERINFO          BIT(12)     // game wants extra userinfo after normal userinfo
#define GMF_IPV6_ADDRESS_AWARE      BIT(13)     // game supports IPv6 addresses
#define GMF_ALLOW_INDEX_OVERFLOW    BIT(14)     // game wants PF_FindIndex() to return 0 on overflow
#define GMF_PROTOCOL_EXTENSIONS     BIT(15)     // game supports protocol extensions

//===============================================================

#define MAX_ENT_CLUSTERS    16

#if !defined(GAME3_INCLUDE)

typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;

#ifndef GAME_INCLUDE

struct gclient_s {
    player_state_t  ps;     // communicated by server to clients
    int             ping;

    // set to (client POV entity number) - 1 by game,
    // only valid if g_features has GMF_CLIENTNUM bit
    int             clientNum;

    // the game dll can add anything it wants after
    // this point in the structure
};

enum {
    SVFL_NONE               = 0, // no flags
    SVFL_ONGROUND           = BIT_ULL(0),
    SVFL_HAS_DMG_BOOST      = BIT_ULL(1),
    SVFL_HAS_PROTECTION     = BIT_ULL(2),
    SVFL_HAS_INVISIBILITY   = BIT_ULL(3),
    SVFL_IS_JUMPING         = BIT_ULL(4),
    SVFL_IS_CROUCHING       = BIT_ULL(5),
    SVFL_IS_ITEM            = BIT_ULL(6),
    SVFL_IS_OBJECTIVE       = BIT_ULL(7),
    SVFL_HAS_TELEPORTED     = BIT_ULL(8),
    SVFL_TAKES_DAMAGE       = BIT_ULL(9),
    SVFL_IS_HIDDEN          = BIT_ULL(10),
    SVFL_IS_NOCLIP          = BIT_ULL(11),
    SVFL_IN_WATER           = BIT_ULL(12),
    SVFL_NO_TARGET          = BIT_ULL(13),
    SVFL_GOD_MODE           = BIT_ULL(14),
    SVFL_IS_FLIPPING_OFF    = BIT_ULL(15),
    SVFL_IS_SALUTING        = BIT_ULL(16),
    SVFL_IS_TAUNTING        = BIT_ULL(17),
    SVFL_IS_WAVING          = BIT_ULL(18),
    SVFL_IS_POINTING        = BIT_ULL(19),
    SVFL_ON_LADDER          = BIT_ULL(20),
    SVFL_MOVESTATE_TOP      = BIT_ULL(21),
    SVFL_MOVESTATE_BOTTOM   = BIT_ULL(22),
    SVFL_MOVESTATE_MOVING   = BIT_ULL(23),
    SVFL_IS_LOCKED_DOOR     = BIT_ULL(24),
    SVFL_CAN_GESTURE        = BIT_ULL(25),
    SVFL_WAS_TELEFRAGGED    = BIT_ULL(26),
    SVFL_TRAP_DANGER        = BIT_ULL(27),
    SVFL_ACTIVE             = BIT_ULL(28),
    SVFL_IS_SPECTATOR       = BIT_ULL(29),
    SVFL_IN_TEAM            = BIT_ULL(30)
};

#define MAX_NETNAME         32
#define MAX_ARMOR_TYPES     3
#define MAX_ITEMS           256

// Rerelease: Used by AI/Tools on the engine side...
typedef struct sv_entity_s {
    bool                        init;
    uint64_t                    ent_flags;
    button_t                    buttons;
    uint32_t                    spawnflags;
    int32_t                     item_id;
    int32_t                     armor_type;
    int32_t                     armor_value;
    int32_t                     health;
    int32_t                     max_health;
    int32_t                     starting_health;
    int32_t                     weapon;
    int32_t                     team;
    int32_t                     lobby_usernum;
    int32_t                     respawntime;
    int32_t                     viewheight;
    int32_t                     last_attackertime;
    uint8_t                     waterlevel;
    vec3_t                      viewangles;
    vec3_t                      viewforward;
    vec3_t                      velocity;
    vec3_t                      start_origin;
    vec3_t                      end_origin;
    edict_t *                   enemy;
    edict_t *                   ground_entity;
    const char *                classname;
    const char *                targetname;
    char                        netname[ MAX_NETNAME ];
    int32_t                     inventory[ MAX_ITEMS ];
    int32_t                     armor_info[ MAX_ARMOR_TYPES ][2];
} sv_entity_t;

struct edict_s {
    entity_state_t  s;
    struct gclient_s    *client;
    sv_entity_t sv;
    bool        inuse;
    bool        linked;
    int         linkcount;
    int         areanum, areanum2;

    //================================

    svflags_t   svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    contents_t  clipmask;
    edict_t     *owner;

    //================================

    // the game dll can add anything it wants after
    // this point in the structure
};

#endif      // GAME_INCLUDE

//===============================================================

// [Paril-KEX] max number of arguments (not including the base) for
// localization prints
#define MAX_LOCALIZATION_ARGS   8

typedef enum BoxEdictsResult_e
{
    BoxEdictsResult_Keep, // keep the given entity in the result and keep looping
    BoxEdictsResult_Skip, // skip the given entity

    BoxEdictsResult_End = 64, // stop searching any further
} BoxEdictsResult_t;

typedef BoxEdictsResult_t (*BoxEdictsFilter_t)(edict_t *, void *);

typedef enum {
    GoalReturnCode_Error = 0,
    GoalReturnCode_Started,
    GoalReturnCode_InProgress,
    GoalReturnCode_Finished
} GoalReturnCode;

typedef enum {
    PathReturnCode_ReachedGoal = 0,        // we're at our destination
    PathReturnCode_ReachedPathEnd,         // we're as close to the goal as we can get with a path
    PathReturnCode_TraversalPending,       // the upcoming path segment is a traversal
    PathReturnCode_RawPathFound,           // user wanted ( and got ) just a raw path ( no processing )
    PathReturnCode_InProgress,             // pathing in progress
    PathReturnCode_StartPathErrors,        // any code after this one indicates an error of some kind.
    PathReturnCode_InvalidStart,           // start position is invalid.
    PathReturnCode_InvalidGoal,            // goal position is invalid.
    PathReturnCode_NoNavAvailable,         // no nav file available for this map.
    PathReturnCode_NoStartNode,            // can't find a nav node near the start position
    PathReturnCode_NoGoalNode,             // can't find a nav node near the goal position
    PathReturnCode_NoPathFound,            // can't find a path from the start to the goal
    PathReturnCode_MissingWalkOrSwimFlag   // MUST have at least Walk or Water path flags set!
} PathReturnCode;

typedef enum {
    PathReturnCode_Walk,               // can walk between the path points
    PathReturnCode_WalkOffLedge,       // will walk off a ledge going between path points
    PathReturnCode_LongJump,           // will need to perform a long jump between path points
    PathReturnCode_BarrierJump,        // will need to jump over a low barrier between path points
    PathReturnCode_Elevator            // will need to use an elevator between path points
} PathLinkType;

enum {
    PathFlags_All             = (uint32_t)( -1 ),
    PathFlags_Water           = 1 << 0,  // swim to your goal ( useful for fish/gekk/etc. )
    PathFlags_Walk            = 1 << 1,  // walk to your goal
    PathFlags_WalkOffLedge    = 1 << 2,  // allow walking over ledges
    PathFlags_LongJump        = 1 << 3,  // allow jumping over gaps
    PathFlags_BarrierJump     = 1 << 4,  // allow jumping over low barriers
    PathFlags_Elevator        = 1 << 5   // allow using elevators
};
typedef uint32_t PathFlags;

typedef struct {
    vec3_t     start /*= { 0.0f, 0.0f, 0.0f }*/;
    vec3_t     goal /*= { 0.0f, 0.0f, 0.0f }*/;
    PathFlags   pathFlags /*= PathFlags::Walk*/;
    float       moveDist /*= 0.0f*/;

    struct DebugSettings {
        float   drawTime /*= 0.0f*/; // if > 0, how long ( in seconds ) to draw path in world
    } debugging;

    struct NodeSettings {
        bool    ignoreNodeFlags /*= false*/; // true = ignore node flags when considering nodes
        float   minHeight /*= 0.0f*/; // 0 <= use default values
        float   maxHeight /*= 0.0f*/; // 0 <= use default values
        float   radius /*= 0.0f*/;    // 0 <= use default values
    } nodeSearch;

    struct TraversalSettings {
        float dropHeight /*= 0.0f*/;    // 0 = don't drop down
        float jumpHeight /*= 0.0f*/;    // 0 = don't jump up
    } traversals;

    struct PathArray {
        vec3_t * posArray /*= nullptr*/;  // array to store raw path points
        int64_t           count /*= 0*/;        // number of elements in array
    } pathPoints;
}  PathRequest;

typedef struct {
    int32_t         numPathPoints /*= 0*/;
    float           pathDistSqr /*= 0.0f*/;
    vec3_t          firstMovePoint /*= { 0.0f, 0.0f, 0.0f }*/;
    vec3_t          secondMovePoint /*= { 0.0f, 0.0f, 0.0f }*/;
    PathLinkType    pathLinkType /*= PathLinkType::Walk*/;
    PathReturnCode  returnCode /*= PathReturnCode::StartPathErrors*/;
} PathInfo;

typedef struct
{
    uint8_t r, g, b, a;
} rgba_t;

typedef enum
{
    shadow_light_type_point,
    shadow_light_type_cone
} shadow_light_type_t;

typedef struct
{
    shadow_light_type_t lighttype;
    float       radius;
    int         resolution;
    float       intensity /*= 1*/;
    float       fade_start;
    float       fade_end;
    int         lightstyle /*= -1*/;
    float       coneangle /*= 45*/;
    vec3_t      conedirection;
} shadow_light_data_t;

//
// functions provided by the main engine
//
typedef struct {
    uint32_t    tick_rate;
    float       frame_time_s;
    uint32_t    frame_time_ms;

    // broadcast to all clients
    void (*Broadcast_Print)(int printlevel, const char *message);
    // print to appropriate places (console, log file, etc)
    void (*Com_Print)(const char *msg);
    // print directly to a single client (or nullptr for server console)
    void (*Client_Print)(edict_t *ent, int printlevel, const char *message);
    // center-print to player (legacy function)
    void (*Center_Print)(edict_t *ent, const char *message);

    void (*sound)(edict_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs);
    void (*positioned_sound)(const vec3_t origin, edict_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs);
    void (*local_sound)(edict_t *target, const vec3_t origin, edict_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs, uint32_t dupe_key);

    // config strings hold all the index strings, the lightstyles,
    // and misc data like the sky definition and cdtrack.
    // All of the current configstrings are sent to clients when
    // they connect, and changes are sent to all connected clients.
    void (*configstring)(int num, const char *string);
    const char *(*get_configstring)(int num);

    void (* q_noreturn_ptr q_printf(1, 2) error)(const char *fmt, ...);

    // the *index functions create configstrings and some internal server state
    int (*modelindex)(const char *name);
    int (*soundindex)(const char *name);
    int (*imageindex)(const char *name);

    void (*setmodel)(edict_t *ent, const char *name);

    // collision detection
    trace_t (* q_gameabi trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passent, contents_t contentmask);
    trace_t (* q_gameabi clip)(edict_t *entity, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, contents_t contentmask);
    contents_t (*pointcontents)(const vec3_t point);
    qboolean (*inPVS)(const vec3_t p1, const vec3_t p2, bool portals);
    qboolean (*inPHS)(const vec3_t p1, const vec3_t p2, bool portals);
    void (*SetAreaPortalState)(int portalnum, bool open);
    qboolean (*AreasConnected)(int area1, int area2);

    // an entity will never be sent to a client or used for collision
    // if it is not passed to linkentity.  If the size, position, or
    // solidity changes, it must be relinked.
    void (*linkentity)(edict_t *ent);
    void (*unlinkentity)(edict_t *ent);     // call before removing an interactive edict
    int (*BoxEdicts)(const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype);

    // network messaging
    void (*multicast)(const vec3_t origin, multicast_t to, bool reliable);
    void (*unicast)(edict_t *ent, bool reliable, uint32_t dupe_key);
    void (*WriteChar)(int c);
    void (*WriteByte)(int c);
    void (*WriteShort)(int c);
    void (*WriteLong)(int c);
    void (*WriteFloat)(float f);
    void (*WriteString)(const char *s);
    void (*WritePosition)(const vec3_t pos);    // some fractional bits
    void (*WriteDir)(const vec3_t pos);         // single byte encoded, very coarse
    void (*WriteAngle)(float f);
    void (*WriteEntity)(const edict_t *e);

    // managed memory allocation
    void *(*TagMalloc)(size_t size, int tag);
    void (*TagFree)(void *block);
    void (*FreeTags)(int tag);

    // console variable interaction
    cvar_t *(*cvar)(const char *var_name, const char *value, int flags);
    cvar_t *(*cvar_set)(const char *var_name, const char *value);
    cvar_t *(*cvar_forceset)(const char *var_name, const char *value);

    // ClientCommand and ServerCommand parameter access
    int (*argc)(void);
    const char *(*argv)(int n);
    const char *(*args)(void);     // concatenation of all argv >= 1

    // add commands to the server console as if they were typed in
    // for map changing, etc
    void (*AddCommandString)(const char *text);

    void (*DebugGraph)(float value, int color);

    void *(*GetExtension)(const char *name);

    // === [KEX] Additional APIs ===

    // bots
    void (*Bot_RegisterEdict)(const edict_t * edict);
    void (*Bot_UnRegisterEdict)(const edict_t * edict);
    GoalReturnCode (*Bot_MoveToPoint)(const edict_t * bot, const vec3_t point, const float moveTolerance);
    GoalReturnCode (*Bot_FollowActor)(const edict_t * bot, const edict_t * actor);

    // pathfinding - returns true if a path was found
    bool (*GetPathToGoal)(const PathRequest* request, PathInfo* info);

    // localization
    void (*Loc_Print)(edict_t* ent, print_type_t level, const char* base, const char** args, size_t num_args);

    // drawing
    void (*Draw_Line)(const vec3_t start, const vec3_t end, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_Point)(const vec3_t point, const float size, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_Circle)(const vec3_t origin, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_Bounds)(const vec3_t mins, const vec3_t maxs, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_Sphere)(const vec3_t origin, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_OrientedWorldText)(const vec3_t origin, const char * text, const rgba_t* color, const float size, const float lifeTime, const bool depthTest);
    void (*Draw_StaticWorldText)(const vec3_t origin, const vec3_t angles, const char * text, const rgba_t* color, const float size, const float lifeTime, const bool depthTest);
    void (*Draw_Cylinder)(const vec3_t origin, const float halfHeight, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_Ray)(const vec3_t origin, const vec3_t direction, const float length, const float size, const rgba_t* color, const float lifeTime, const bool depthTest);
    void (*Draw_Arrow)(const vec3_t start, const vec3_t end, const float size, const rgba_t* lineColor, const rgba_t* arrowColor, const float lifeTime, const bool depthTest);

    // scoreboard
    void (*ReportMatchDetails_Multicast)(bool is_end);

    // get server frame #
    uint32_t (*ServerFrame)(void);

    // misc utils
    void (*SendToClipBoard)(const char * text);

    // info string stuff
    size_t (*Info_ValueForKey) (const char *s, const char *key, char *buffer, size_t buffer_len);
    bool (*Info_RemoveKey) (char *s, const char *key);
    bool (*Info_SetValueForKey) (char *s, const char *key, const char *value);
} game_import_t;

//
// functions exported by the game subsystem
//
typedef struct {
    int         apiversion;

    // the init function will only be called when a game starts,
    // not each time a level is loaded.  Persistant data for clients
    // and the server can be allocated in init
    void (*PreInit)(void); // [Paril-KEX] called before InitGame, to potentially change maxclients
    void (*Init)(void);
    void (*Shutdown)(void);

    // each new level entered will cause a call to SpawnEntities
    void (*SpawnEntities)(const char *mapname, const char *entstring, const char *spawnpoint);

    // Read/Write Game is for storing persistant cross level information
    // about the world state and the clients.
    // WriteGame is called every time a level is exited.
    // ReadGame is called on a loadgame.
    // returns pointer to tagmalloc'd allocated string.
    // tagfree after use
    char *(*WriteGameJson)(bool autosave, size_t *out_size);
    void (*ReadGameJson)(const char *json);

    // ReadLevel is called after the default map information has been
    // loaded with SpawnEntities
    // returns pointer to tagmalloc'd allocated string.
    // tagfree after use
    char *(*WriteLevelJson)(bool transition, size_t *out_size);
    void (*ReadLevelJson)(const char *json);

    // [Paril-KEX] game can tell the server whether a save is allowed
    // currently or not.
    bool (*CanSave)(void);

    // [Paril-KEX] choose a free gclient_t slot for the given social ID; for
    // coop slot re-use. Return nullptr if none is available. You can not
    // return a slot that is currently in use by another client; that must
    // throw a fatal error.
    edict_t *(*ClientChooseSlot) (const char *userinfo, const char *social_id, bool isBot, edict_t **ignore, size_t num_ignore, bool cinematic);
    bool (*ClientConnect)(edict_t *ent, char *userinfo, const char *social_id, bool isBot);
    void (*ClientBegin)(edict_t *ent);
    void (*ClientUserinfoChanged)(edict_t *ent, const char *userinfo);
    void (*ClientDisconnect)(edict_t *ent);
    void (*ClientCommand)(edict_t *ent);
    void (*ClientThink)(edict_t *ent, usercmd_t *cmd);

    void (*RunFrame)(bool main_loop);
    // [Paril-KEX] allow the game DLL to clear per-frame stuff
    void (*PrepFrame)(void);

    // ServerCommand will be called when an "sv <command>" command is issued on the
    // server console.
    // The game can issue gi.argc() / gi.argv() commands to get the rest
    // of the parameters
    void (*ServerCommand)(void);

    //
    // global variables shared between game and server
    //

    // The edict array is allocated in the game dll so it
    // can vary in size from one game to another.
    //
    // The size will be fixed when ge->Init() is called
    struct edict_s  *edicts;
    size_t      edict_size;
    uint32_t    num_edicts;     // current number, <= max_edicts
    uint32_t    max_edicts;

    // [Paril-KEX] special flags to indicate something to the server
    int server_flags;

    // [KEX]: Pmove as export
    void (*Pmove)(pmove_t *pmove); // player movement code called by server & client

    // Fetch named extension from game DLL.
    void *(*GetExtension)(const char *name);

    void    (*Bot_SetWeapon)(edict_t * botEdict, const int weaponIndex, const bool instantSwitch);
    void    (*Bot_TriggerEdict)(edict_t * botEdict, edict_t * edict);
    void    (*Bot_UseItem)(edict_t * botEdict, const int32_t itemID);
    int32_t (*Bot_GetItemID)(const char * classname);
    void    (*Edict_ForceLookAtPoint)(edict_t * edict, const vec3_t point);
    bool    (*Bot_PickedUpItem )(edict_t * botEdict, edict_t * itemEdict);

    // [KEX]: Checks entity visibility instancing
    bool (*Entity_IsVisibleToPlayer)(edict_t* ent, edict_t* player);

    // Fetch info from the shadow light, for culling
    const shadow_light_data_t *(*GetShadowLightData)(int32_t entity_number);
} game_export_t;

typedef game_export_t *(*game_entry_t)(game_import_t *);

//===============================================================

/*
 * GetGameAPIEx() is guaranteed to be called after GetGameAPI() and before
 * ge->Init().
 *
 * Unlike GetGameAPI(), passed game_import_ex_t * is valid as long as game
 * library is loaded. Pointed to structure can be used directly without making
 * a copy of it. If copying is neccessary, no more than structsize bytes must
 * be copied.
 *
 * New fields can be safely added at the end of game_import_ex_t and
 * game_export_ex_t structures, provided GAME_API_VERSION_EX is also bumped.
 */

#define GAME_API_VERSION_EX     -1

typedef struct {
    uint32_t    apiversion;
    uint32_t    structsize;
} game_import_ex_t;

typedef struct {
    uint32_t    apiversion;
    uint32_t    structsize;

    void        (*RestartFilesystem)(void); // called when fs_restart is issued
} game_export_ex_t;

typedef const game_export_ex_t *(*game_entry_ex_t)(const game_import_ex_t *);

//===============================================================

#define CGAME_API_VERSION   2022

typedef enum
{
    LEFT,
    CENTER,
    RIGHT
} text_align_t;

typedef struct
{
    float x, y;
} cg_vec2_t;

typedef struct
{
    char layout[1024];
    int16_t inventory[MAX_ITEMS];
} cg_server_data_t;

//
// functions provided by main engine for client
//
typedef struct
{
    uint32_t    tick_rate;
    float       frame_time_s;
    uint32_t    frame_time_ms;

    // print to appropriate places (console, log file, etc)
    void (*Com_Print)(const char *msg);
    
    // config strings hold all the index strings, the lightstyles,
    // and misc data like the sky definition and cdtrack.
    // All of the current configstrings are sent to clients when
    // they connect, and changes are sent to all connected clients.
    const char *(*get_configstring)(int num);

    void (*Com_Error)(const char *message);

    // managed memory allocation
    void *(*TagMalloc)(size_t size, int tag);
    void (*TagFree)(void *block);
    void (*FreeTags)(int tag);

    // console variable interaction
    cvar_t *(*cvar)(const char *var_name, const char *value, cvar_flags_t flags);
    cvar_t *(*cvar_set)(const char *var_name, const char *value);
    cvar_t *(*cvar_forceset)(const char *var_name, const char *value);

    // add commands to the server console as if they were typed in
    // for map changing, etc
    void (*AddCommandString)(const char *text);

    // Fetch named extension from engine.
    void *(*GetExtension)(const char *name);

    // Check whether current frame is valid
    bool (*CL_FrameValid) (void);

    // Get client frame time delta
    float (*CL_FrameTime) (void);

    // [Paril-KEX] cgame-specific stuff
    uint64_t (*CL_ClientTime) (void);
    uint64_t (*CL_ClientRealTime) (void);
    int32_t (*CL_ServerFrame) (void);
    int32_t (*CL_ServerProtocol) (void);
    const char *(*CL_GetClientName) (int32_t index);
    const char *(*CL_GetClientPic) (int32_t index);
    const char *(*CL_GetClientDogtag) (int32_t index);
    const char *(*CL_GetKeyBinding) (const char *binding); // fetch key bind for key, or empty string
    bool (*Draw_RegisterPic) (const char *name);
    void (*Draw_GetPicSize) (int *w, int *h, const char *name); // will return 0 0 if not found
    void (*SCR_DrawChar)(int x, int y, int scale, int num, bool shadow);
    void (*SCR_DrawPic) (int x, int y, int w, int h, const char *name);
    void (*SCR_DrawColorPic)(int x, int y, int w, int h, const char* name, const rgba_t *color);

    // [Paril-KEX] kfont stuff
    void(*SCR_SetAltTypeface)(bool enabled);
    void (*SCR_DrawFontString)(const char *str, int x, int y, int scale, const rgba_t *color, bool shadow, text_align_t align);
    cg_vec2_t (*SCR_MeasureFontString)(const char *str, int scale);
    float (*SCR_FontLineHeight)(int scale);

    // [Paril-KEX] for legacy text input (not used in lobbies)
    bool (*CL_GetTextInput)(const char **msg, bool *is_team);

    // [Paril-KEX] FIXME this probably should be an export instead...
    int32_t (*CL_GetWarnAmmoCount)(int32_t weapon_id);

    // === [KEX] Additional APIs ===
    // returns a *temporary string* ptr to a localized input
    const char* (*Localize) (const char *base, const char **args, size_t num_args);

    // [Paril-KEX] Draw binding, for centerprint; returns y offset
    int32_t (*SCR_DrawBind) (int32_t isplit, const char *binding, const char *purpose, int x, int y, int scale);

    // [Paril-KEX]
    bool (*CL_InAutoDemoLoop) (void);
} cgame_import_t;

//
// functions exported for client by game subsystem
//
typedef struct
{
    int         apiversion;

    // the init/shutdown functions will be called between levels/connections
    // and when the client initially loads.
    void (*Init)(void);
    void (*Shutdown)(void);

    // [Paril-KEX] hud drawing
    void (*DrawHUD) (int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps);
    // [Paril-KEX] precache special pics used by hud
    void (*TouchPics) (void);

    // [Paril-KEX] layout flags; see layout_flags_t
    layout_flags_t (*LayoutFlags) (const player_state_t *ps);

    // [Paril-KEX] fetch the current wheel weapon ID in use
    int32_t (*GetActiveWeaponWheelWeapon) (const player_state_t *ps);

    // [Paril-KEX] fetch owned weapon IDs
    uint32_t (*GetOwnedWeaponWheelWeapons) (const player_state_t *ps);

    // [Paril-KEX] fetch ammo count for given ammo id
    int16_t (*GetWeaponWheelAmmoCount)(const player_state_t *ps, int32_t ammo_id);

    // [Paril-KEX] fetch powerup count for given powerup id
    int16_t (*GetPowerupWheelCount)(const player_state_t *ps, int32_t powerup_id);

    // [Paril-KEX] fetch how much damage was registered by these stats
    int16_t (*GetHitMarkerDamage)(const player_state_t *ps);

    // [KEX]: Pmove as export
    void (*Pmove)(pmove_t *pmove); // player movement code called by server & client

    // [Paril-KEX] allow cgame to react to configstring changes
    void (*ParseConfigString)(int32_t i, const char *s);

    // [Paril-KEX] parse centerprint-like messages
    void (*ParseCenterPrint)(const char *str, int isplit, bool instant);

    // [Paril-KEX] tell the cgame to clear notify stuff
    void (*ClearNotify)(int32_t isplit);

    // [Paril-KEX] tell the cgame to clear centerprint state
    void (*ClearCenterprint)(int32_t isplit);

    // [Paril-KEX] be notified by the game DLL of a message of some sort
    void (*NotifyMessage)(int32_t isplit, const char *msg, bool is_chat);

    // [Paril-KEX]
    void (*GetMonsterFlashOffset)(monster_muzzleflash_id_t id, vec3_t offset);

    // Fetch named extension from cgame DLL.
    void *(*GetExtension)(const char *name);
} cgame_export_t;

#endif // defined(GAME3_INCLUDE)
