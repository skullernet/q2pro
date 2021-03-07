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

#ifndef GAME_H
#define GAME_H

#include "shared/list.h"

//
// game.h -- game dll information visible to server
//

#define GAME_API_VERSION    3

// edict->svflags

#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects
#define SVF_DEADMONSTER         0x00000002  // treat as CONTENTS_DEADMONSTER for collision
#define SVF_MONSTER             0x00000004  // treat as CONTENTS_MONSTER for collision

// edict->solid values

typedef enum {
    SOLID_NOT,          // no interaction with other objects
    SOLID_TRIGGER,      // only touch when inside, after moving
    SOLID_BBOX,         // touch on edge
    SOLID_BSP           // bsp clip, touch on edge
} solid_t;

// extended features

#define GMF_CLIENTNUM               0x00000001
#define GMF_PROPERINUSE             0x00000002
#define GMF_MVDSPEC                 0x00000004
#define GMF_WANT_ALL_DISCONNECTS    0x00000008

#define GMF_ENHANCED_SAVEGAMES      0x00000400
#define GMF_VARIABLE_FPS            0x00000800
#define GMF_EXTRA_USERINFO          0x00001000
#define GMF_IPV6_ADDRESS_AWARE      0x00002000

//===============================================================

#define MAX_ENT_CLUSTERS    16

typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;

typedef struct {
    edict_t *edicts;
    int     edict_size;
    int     num_edicts;     // current number, <= max_edicts
    int     max_edicts;
} edict_pool_t;

#ifndef GAME_INCLUDE

struct gclient_s {
    player_state_t  ps;     // communicated by server to clients
    int             ping;

    // the game dll can add anything it wants after
    // this point in the structure
    int             clientNum;
};

struct edict_s {
    entity_state_t  s;
    struct gclient_s    *client;
    qboolean    inuse;
    int         linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t      area;               // linked to a division node or leaf

    int         num_clusters;       // if -1, use headnode instead
    int         clusternums[MAX_ENT_CLUSTERS];
    int         headnode;           // unused if num_clusters != -1
    int         areanum, areanum2;

    //================================

    int         svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    int         clipmask;
    edict_t     *owner;

    // the game dll can add anything it wants after
    // this point in the structure
};

#else

// Imported function declarations. These are imported on WASM,
// but wrap the function pointers on native, except for the
// "formatted" string functions which have slightly different
// implementations on WASM.
#if __wasm__
#define NATIVE_IMPORT
#else
#define NATIVE_IMPORT(x) (* x)
#endif

#ifndef NATIVE_LINKAGE
#define NATIVE_LINKAGE extern
#endif

// special messages
NATIVE_LINKAGE void NATIVE_IMPORT(q_printf(2, 3) gi_bprintf)(int printlevel, const char *fmt, ...);
NATIVE_LINKAGE void NATIVE_IMPORT(q_printf(1, 2) gi_dprintf)(const char *fmt, ...);
NATIVE_LINKAGE void NATIVE_IMPORT(q_printf(3, 4) gi_cprintf)(edict_t *ent, int printlevel, const char *fmt, ...);
NATIVE_LINKAGE void NATIVE_IMPORT(q_printf(2, 3) gi_centerprintf)(edict_t *ent, const char *fmt, ...);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_sound)(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_positioned_sound)(vec3_t origin, edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

// config strings hold all the index strings, the lightstyles,
// and misc data like the sky definition and cdtrack.
// All of the current configstrings are sent to clients when
// they connect, and changes are sent to all connected clients.
NATIVE_LINKAGE void NATIVE_IMPORT(gi_configstring)(int num, const char *string);

NATIVE_LINKAGE void NATIVE_IMPORT(q_noreturn q_printf(1, 2) gi_error)(const char *fmt, ...);

// the *index functions create configstrings and some internal server state
NATIVE_LINKAGE int NATIVE_IMPORT(gi_modelindex)(const char *name);
NATIVE_LINKAGE int NATIVE_IMPORT(gi_soundindex)(const char *name);
NATIVE_LINKAGE int NATIVE_IMPORT(gi_imageindex)(const char *name);

NATIVE_LINKAGE void NATIVE_IMPORT(gi_setmodel)(edict_t *ent, const char *name);

// collision detection
NATIVE_LINKAGE trace_t NATIVE_IMPORT(q_gameabi gi_trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
NATIVE_LINKAGE int NATIVE_IMPORT(gi_pointcontents)(vec3_t point);
NATIVE_LINKAGE qboolean NATIVE_IMPORT(gi_inPVS)(vec3_t p1, vec3_t p2);
NATIVE_LINKAGE qboolean NATIVE_IMPORT(gi_inPHS)(vec3_t p1, vec3_t p2);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_SetAreaPortalState)(int portalnum, qboolean open);
NATIVE_LINKAGE qboolean NATIVE_IMPORT(gi_AreasConnected)(int area1, int area2);

// an entity will never be sent to a client or used for collision
// if it is not passed to linkentity.  If the size, position, or
// solidity changes, it must be relinked.
NATIVE_LINKAGE void NATIVE_IMPORT(gi_linkentity)(edict_t *ent);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_unlinkentity)(edict_t *ent);     // call before removing an interactive edict
NATIVE_LINKAGE int NATIVE_IMPORT(gi_BoxEdicts)(vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_Pmove)(pmove_t *pmove);          // player movement code common with client prediction

// network messaging
NATIVE_LINKAGE void NATIVE_IMPORT(gi_multicast)(vec3_t origin, multicast_t to);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_unicast)(edict_t *ent, qboolean reliable);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteChar)(int c);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteByte)(int c);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteShort)(int c);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteLong)(int c);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteFloat)(float f);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteString)(const char *s);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WritePosition)(const vec3_t pos);    // some fractional bits
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteDir)(const vec3_t pos);         // single byte encoded, very coarse
NATIVE_LINKAGE void NATIVE_IMPORT(gi_WriteAngle)(float f);

// managed memory allocation
NATIVE_LINKAGE void *NATIVE_IMPORT(gi_TagMalloc)(unsigned size, unsigned tag);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_TagFree)(void *block);
NATIVE_LINKAGE void NATIVE_IMPORT(gi_FreeTags)(unsigned tag);

// console variable interaction
NATIVE_LINKAGE cvar_t *NATIVE_IMPORT(gi_cvar)(const char *var_name, const char *value, int flags);
NATIVE_LINKAGE cvar_t *NATIVE_IMPORT(gi_cvar_set)(const char *var_name, const char *value);
NATIVE_LINKAGE cvar_t *NATIVE_IMPORT(gi_cvar_forceset)(const char *var_name, const char *value);

// ClientCommand and ServerCommand parameter access
NATIVE_LINKAGE int NATIVE_IMPORT(gi_argc)(void);
NATIVE_LINKAGE char *NATIVE_IMPORT(gi_argv)(int n);
NATIVE_LINKAGE char *NATIVE_IMPORT(gi_args)(void);     // concatenation of all argv >= 1

// add commands to the server console as if they were typed in
// for map changing, etc
NATIVE_LINKAGE void NATIVE_IMPORT(gi_AddCommandString)(const char *text);

NATIVE_LINKAGE void NATIVE_IMPORT(gi_DebugGraph)(float value, int color);

#endif      // GAME_INCLUDE

#include "game_ext.h"

#endif // GAME_H
