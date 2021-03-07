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
// sv_game.c -- interface to the game dll

#include "server.h"

#include <wasm_export.h>

#ifdef _MSC_VER
#include <malloc.h>
#endif

// from game.c
qboolean PF_inPHS(vec3_t p1, vec3_t p2);
qboolean PF_inPVS(vec3_t p1, vec3_t p2);
void PF_WriteFloat(float f);
void PF_configstring(int index, const char *val);
void PF_setmodel(edict_t *ent, const char *name);
void PF_centerprintf(edict_t *ent, const char *fmt, ...);
void PF_cprintf(edict_t *ent, int level, const char *fmt, ...);
void PF_bprintf(int level, const char *fmt, ...);
void PF_Unicast(edict_t *ent, qboolean reliable);
int PF_ImageIndex(const char *name);
int PF_SoundIndex(const char *name);
int PF_ModelIndex(const char *name);
void PF_AddCommandString(const char *string);
void PF_SetAreaPortalState(int portalnum, qboolean open);
qboolean PF_AreasConnected(int area1, int area2);
void PF_DebugGraph(float value, int color);
void PF_StartSound(edict_t *entity, int channel,
                   int soundindex, float volume,
                   float attenuation, float timeofs);
void SV_StartSound(vec3_t origin, edict_t *edict, int channel,
                   int soundindex, float volume,
                   float attenuation, float timeofs);
cvar_t *PF_cvar(const char *name, const char *value, int flags);

typedef void *wasm_function_t;

typedef struct {
    bool    loaded; // whether we've got a valid, loaded WASM game module

    // Since GetGameAPI can't return a value/pointer, otherwise the optimizer
    // tends to optimize out max_edicts, we have these new endpoints to fetch
    // the returned components of the game export.
	wasm_function_t GetEdicts; // returns a pointer to where the current edicts are being held
	wasm_function_t GetNumEdicts; // returns a pointer where the current num_edicts is being held
    
    wasm_function_t PmoveTrace;
    wasm_function_t PmovePointContents;

    // the init function will only be called when a game starts,
    // not each time a level is loaded.  Persistant data for clients
    // and the server can be allocated in init
    wasm_function_t Init;
    wasm_function_t Shutdown;

    // each new level entered will cause a call to SpawnEntities
    wasm_function_t SpawnEntities;

    // Read/Write Game is for storing persistant cross level information
    // about the world state and the clients.
    // WriteGame is called every time a level is exited.
    // ReadGame is called on a loadgame.
    wasm_function_t WriteGame;
    wasm_function_t ReadGame;

    // ReadLevel is called after the default map information has been
    // loaded with SpawnEntities
    wasm_function_t WriteLevel;
    wasm_function_t ReadLevel;

    wasm_function_t ClientConnect;
    wasm_function_t ClientBegin;
    wasm_function_t ClientUserinfoChanged;
    wasm_function_t ClientDisconnect;
    wasm_function_t ClientCommand;
    wasm_function_t ClientThink;

    wasm_function_t RunFrame;

    // ServerCommand will be called when an "sv <command>" command is issued on the
    // server console.
    // The game can issue gi.argc() / gi.argv() commands to get the rest
    // of the parameters
    wasm_function_t ServerCommand;

    //
    // global variables shared between game and server
    //

    // The edict array is allocated in the game dll so it
    // can vary in size from one game to another.
    //
    // The size will be fixed when ge->Init() is called
    wasm_address_t  edicts, edicts_end;
    wasm_address_t  num_edicts;     // current number, <= max_edicts
    uint32_t edict_size;

    wasm_function_t QueryGameCapability;
} wasm_export_t;

// this contains addresses to special bits of data that
// we will need to communicate between WASM and native.
typedef struct {
    // Zeroed texinfo; sizeof(csurface_t)
    wasm_address_t nulltexinfo;

    // Texinfo pointer, sizeof(csurface_t) * num surfaces
    wasm_address_t surfaces;

    // Four vec3_t for the pmove wrapper
    wasm_address_t vectors;

    // A wasm_trace_t that is used for the pmove wrapper
    wasm_address_t trace;

    // A usercmd_t used for ClientThink
    wasm_address_t usercmd;

    // Strings used by SpawnEntities
    wasm_address_t mapname, entstring, spawnpoint;
} wasm_mem_t;

static wasm_export_t wasm_ge;
static wasm_mem_t wasm_mem;

static wasm_module_t module;
static wasm_module_inst_t module_inst;
static wasm_exec_env_t exec_env;
static byte *wasm_binary;
static int wasm_size;
static char wasm_error[MAXERRORMSG];
static char wasi_dir[2][MAX_QPATH];
static char wasi_dir_map[2][MAX_OSPATH];
static const char *wasi_dirs[2] = {
    wasi_dir[0],
    wasi_dir[1]
};
static const char *wasi_dir_maps[2] = {
    wasi_dir_map[0],
    wasi_dir_map[1]
};
static size_t wasi_num_dirs;

wasm_address_t SV_AllocateWASMMemory(uint32_t size)
{
    return wasm_runtime_module_malloc(module_inst, size, NULL);
}

void SV_FreeWASMMemory(wasm_address_t addr)
{
    wasm_runtime_module_free(module_inst, addr);
}

/*
===============
SV_ValidateWASMAddress

Checks that a given address range is within valid WASM memory.
===============
*/
bool SV_ValidateWASMAddress(wasm_address_t address, uint32_t size)
{
    return wasm_runtime_validate_app_addr(module_inst, address, size);
}

/*
===============
SV_ValidateWASMEntity

Checks that a given address is a valid entity. This will also
allow null (zero) as a valid option.
===============
*/
bool SV_ValidateWASMEntityAddress(wasm_address_t address)
{
    return !address || ((address >= wasm_ge.edicts && address <= wasm_ge.edicts_end - wasm_ge.edict_size) &&
        ((address - wasm_ge.edicts) % wasm_ge.edict_size == 0) && SV_ValidateWASMAddress(address, wasm_ge.edict_size));
}

/*
===============
SV_ResolveWASMAddress

Return native memory location of a given WASM address.
===============
*/
void *SV_ResolveWASMAddress(wasm_address_t address)
{
    if (!address)
        return NULL;

    return wasm_runtime_addr_app_to_native(module_inst, address);
}

/*
===============
SV_UnresolveWASMAddress

Return WASM memory location of a given native address.
===============
*/
wasm_address_t SV_UnresolveWASMAddress(void *pointer)
{
    if (pointer == NULL)
        return 0;

    return wasm_runtime_addr_native_to_app(module_inst, pointer);
}

/*
===============
SV_CallWASMFunction

Call a function from the WASM module. You're responsible for making sure the
passed arguments pointer can hold the inputs *and* outputs of the function. Note
that results from WASM functions are written into this argument array!
===============
*/
void SV_CallWASMFunction(wasm_function_t function, uint32_t argc, uint32_t argv[])
{
    if (!wasm_runtime_call_wasm(exec_env, function, argc, argv)) {
        Com_Error(ERR_FATAL, "%s: error in execution of function: %s", __func__, wasm_runtime_get_exception(module_inst));
    }
}

/*
===============
SV_GetWASMFunction

Fetch a function from the WASM module, returning its pointer if it exists,
otherwise returning NULL.
===============
*/
wasm_function_t SV_GetWASMFunction(const char *name, const char *signature, bool fatal)
{
    wasm_function_inst_t *func = wasm_runtime_lookup_function(module_inst, name, signature);

    if (!func && fatal) {
        Com_Error(ERR_FATAL, "%s: couldn't find function: %s", __func__, name);
    }

    return func;
}

bool SV_IsWASMRunning(void)
{
    return wasm_ge.loaded;
}

typedef struct {
    uint32_t tag;
    wasm_address_t address;

    list_t list;
} wasm_tagged_mem_t;

static list_t tagged_memory;

static void SV_ShutdownGameWasm(void)
{
	if (exec_env) {
		wasm_runtime_destroy_exec_env(exec_env);
        exec_env = NULL;
    }

	if (module_inst) {
		wasm_runtime_deinstantiate(module_inst);
        module_inst = NULL;
    }

	if (module) {
		wasm_runtime_unload(module);
        module = NULL;
    }

    if (wasm_binary) {
        FS_FreeFile(wasm_binary);
        wasm_binary = NULL;
        wasm_size = 0;
    }

	wasm_runtime_destroy();

    memset(&wasm_ge, 0, sizeof(wasm_ge));
    memset(&wasm_mem, 0, sizeof(wasm_mem));

    Cvar_DestroyWASMLinkage();

    Z_FreeTags(TAG_WASM);

    List_Init(&tagged_memory);
}

static bool _SV_LoadGameWasm(const char *path)
{
    int ret = FS_LoadFileEx(path, (void **) &wasm_binary, FS_PATH_GAME | FS_PATH_BASE, TAG_FILESYSTEM);

    if (!wasm_binary) {
        if (ret != Q_ERR_NOENT) {
            Com_EPrintf("Failed to load game WASM %s: %s\n", path, Q_ErrorString(ret));
        }
        return false;
    }

    wasm_size = ret;
    return true;
}

static bool SV_LoadGameWasm(const char *game, const char *prefix)
{
    char path[MAX_OSPATH];

    if (Q_concat(path, sizeof(path), prefix, "game" WASMSUFFIX) >= sizeof(path)) {
        Com_EPrintf("Game WASM path length exceeded\n");
        return false;
    }

    return _SV_LoadGameWasm(path);
}

/*
===============
SV_SetWASMEntityPointers

Set the current addresses of num_edicts and edicts.
These may change during a game load, so we have to be sure
to fetch their new addresses.
===============
*/
void SV_SetWASMEntityPointers(void)
{
    uint32_t args[1];

    SV_CallWASMFunction(wasm_ge.GetNumEdicts, 0, args);
    wasm_ge.num_edicts = args[0];

    SV_CallWASMFunction(wasm_ge.GetEdicts, 0, args);
    wasm_ge.edicts = args[0];

    wasm_ge.edicts_end = wasm_ge.edicts + (ge->pool.max_edicts * wasm_ge.edict_size);
}

typedef struct {
	entity_state_t	s;
	uint32_t		client;
	qboolean		inuse;
	int32_t			linkcount;

	int32_t	area_next, area_prev;

	int32_t	num_clusters;
	int32_t	clusternums[MAX_ENT_CLUSTERS];
	int32_t	headnode;
	int32_t	areanum, areanum2;

	int32_t     	svflags;
	vec3_t			mins, maxs;
	vec3_t			absmin, absmax, size;
	solid_t			solid;
	int32_t         clipmask;
	wasm_address_t	owner;
} wasm_edict_t;

typedef struct {
    struct {
        wasm_field_t    number;

        wasm_field_t    origin;
        wasm_field_t    angles;
        wasm_field_t    old_origin;
        wasm_field_t    modelindex;
        wasm_field_t    modelindex2, modelindex3, modelindex4;
        wasm_field_t    frame;
        wasm_field_t    skinnum;
        wasm_field_t    effects;
        wasm_field_t    renderfx;
        wasm_field_t    solid;

        wasm_field_t    sound;
        wasm_field_t    event;
    } s;

    wasm_field_t    client;
    wasm_field_t    inuse;
    wasm_field_t    linkcount;

    wasm_field_t    area_prev;

    wasm_field_t    areanum, areanum2;

    wasm_field_t    svflags;
    wasm_field_t    mins, maxs;
    wasm_field_t    absmin, absmax, size;
    wasm_field_t    solid;
    wasm_field_t    clipmask;
    wasm_field_t    owner;
} wasm_edict_layout_t;

#define WASM_FIELD(t, member, n) \
    { t, (uint32_t) offsetof(wasm_edict_t, member), n }

static wasm_edict_layout_t wasm_standard_edict_layout = {
    {
        WASM_FIELD(WF_INT32, s.number, 1),
        
        WASM_FIELD(WF_FLOAT32, s.origin, 3),
        WASM_FIELD(WF_FLOAT32, s.angles, 3),
        WASM_FIELD(WF_FLOAT32, s.old_origin, 3),
        
        WASM_FIELD(WF_INT32, s.modelindex, 1),
        WASM_FIELD(WF_INT32, s.modelindex2, 1),
        WASM_FIELD(WF_INT32, s.modelindex3, 1),
        WASM_FIELD(WF_INT32, s.modelindex4, 1),
        WASM_FIELD(WF_INT32, s.frame, 1),
        WASM_FIELD(WF_INT32, s.skinnum, 1),
        WASM_FIELD(WF_INT32 | WF_UNSIGNED, s.effects, 1),
        WASM_FIELD(WF_INT32, s.renderfx, 1),
        WASM_FIELD(WF_INT32, s.solid, 1),

        WASM_FIELD(WF_INT32, s.sound, 1),
        WASM_FIELD(WF_INT32, s.event, 1),
    },

    WASM_FIELD(WF_POINTER, client, 1),
    WASM_FIELD(WF_INT32, inuse, 1),
    WASM_FIELD(WF_INT32, linkcount, 1),

    WASM_FIELD(WF_INT32, area_prev, 1),

    WASM_FIELD(WF_INT32, areanum, 1),
    WASM_FIELD(WF_INT32, areanum2, 1),

    WASM_FIELD(WF_INT32, svflags, 1),
    WASM_FIELD(WF_FLOAT32, mins, 3),
    WASM_FIELD(WF_FLOAT32, maxs, 3),
    WASM_FIELD(WF_FLOAT32, absmin, 3),
    WASM_FIELD(WF_FLOAT32, absmax, 3),
    WASM_FIELD(WF_FLOAT32, size, 3),
    WASM_FIELD(WF_INT32, solid, 1),
    WASM_FIELD(WF_INT32, clipmask, 1),
    WASM_FIELD(WF_POINTER, owner, 1)
};

#undef WASM_FIELD

// WASM entity management
static inline edict_t *entity_number_to_native(int32_t i)
{
    return &ge->pool.edicts[i];
}

static inline wasm_edict_t *entity_number_to_wasm(int32_t i)
{
    return SV_ResolveWASMAddress(wasm_ge.edicts + (wasm_ge.edict_size * i));
}

static inline edict_t *entity_wasm_to_native(uint32_t address)
{
    if (!address)
        return NULL;

    return &ge->pool.edicts[(address - wasm_ge.edicts) / wasm_ge.edict_size];
}

static inline uint32_t entity_native_to_wasm(edict_t *e)
{
    if (e == NULL)
        return 0;

    return wasm_ge.edicts + ((e - ge->pool.edicts) * wasm_ge.edict_size);
}

static inline void copy_link_wasm_to_native(edict_t *native_edict, const wasm_edict_t *wasm_edict)
{
	native_edict->s = wasm_edict->s;
	native_edict->inuse = wasm_edict->inuse;
	native_edict->svflags = wasm_edict->svflags;
	VectorCopy(wasm_edict->mins, native_edict->mins);
	VectorCopy(wasm_edict->maxs, native_edict->maxs);
	native_edict->clipmask = wasm_edict->clipmask;
	native_edict->solid = wasm_edict->solid;
	native_edict->linkcount = wasm_edict->linkcount;
}

static inline void copy_link_native_to_wasm(wasm_edict_t *wasm_edict, const edict_t *native_edict)
{
    if (wasm_edict->linkcount == native_edict->linkcount)
        return;

	wasm_edict->area_next = native_edict->area.next ? 1 : 0;
	wasm_edict->area_prev = native_edict->area.prev ? 1 : 0;
	wasm_edict->linkcount = native_edict->linkcount;
	wasm_edict->areanum = native_edict->areanum;
	wasm_edict->areanum2 = native_edict->areanum2;
	VectorCopy(native_edict->absmin, wasm_edict->absmin);
	VectorCopy(native_edict->absmax, wasm_edict->absmax);
	VectorCopy(native_edict->size, wasm_edict->size);
	wasm_edict->s.solid = native_edict->s.solid;
	wasm_edict->linkcount = native_edict->linkcount;
}

static inline void copy_frame_native_to_wasm(wasm_edict_t *wasm_edict, const edict_t *native_edict)
{
	wasm_edict->s.number = native_edict->s.number;
	wasm_edict->s.event = native_edict->s.event;
}

static inline bool should_sync_entity(const wasm_edict_t *wasm_edict, const edict_t *native)
{
	return (!wasm_edict->client != !native->client ||
		wasm_edict->inuse != native->inuse ||
		wasm_edict->inuse);
}

static inline void sync_entity(wasm_edict_t *wasm_edict, edict_t *native, bool force)
{
	// Don't bother syncing non-inuse entities.
	if (!force && !should_sync_entity(wasm_edict, native))
		return;

	// sync main data
	copy_link_wasm_to_native(native, wasm_edict);

	// fill owner pointer
	native->owner = entity_wasm_to_native(wasm_edict->owner);

	// sync client structure, if it exists
	if (wasm_edict->client)
	{
		if (!native->client)
			native->client = Z_TagMalloc(sizeof(gclient_t), TAG_WASM);

		size_t client_struct_size = sizeof(gclient_t);

		if (g_features->integer & GMF_CLIENTNUM)
			client_struct_size -= sizeof(int32_t);

		memcpy(native->client, SV_ResolveWASMAddress(wasm_edict->client), client_struct_size);
	}
	else
	{
		if (native->client)
		{
			Z_Free(native->client);
			native->client = NULL;
		}
	}
}

static void pre_sync_entities(void)
{
	for (int32_t i = 0; i < ge->pool.num_edicts; i++)
		copy_frame_native_to_wasm(entity_number_to_wasm(i), entity_number_to_native(i));
}

static void post_sync_entities(void)
{
	const int32_t wasm_num = *(int32_t *)SV_ResolveWASMAddress(wasm_ge.num_edicts);

	const int32_t num_sync = max(ge->pool.num_edicts, wasm_num);

	ge->pool.num_edicts = wasm_num;

	for (int32_t i = 0; i < num_sync; i++)
		sync_entity(entity_number_to_wasm(i), entity_number_to_native(i), false);
}

#define WASM_MEMORY_VALIDATE_NO_NULL(addr, size) \
	if (!addr || !SV_ValidateWASMAddress(addr, size)) { \
        Com_Error(ERR_DROP, "%s: invalid pointer passed for argument %s", __func__, #addr); \
    }

#define WASM_ENTITY_VALIDATE_NO_NULL(addr) \
	if (!addr || !SV_ValidateWASMEntityAddress(addr)) { \
        Com_Error(ERR_DROP, "%s: invalid entity pointer passed for argument %s", __func__, #addr); \
    }

#define WASM_ENTITY_VALIDATE(addr) \
	if (!SV_ValidateWASMEntityAddress(addr)) { \
        Com_Error(ERR_DROP, "%s: invalid entity pointer passed for argument %s", __func__, #addr); \
    }

static void PF_wasm_dprint(wasm_exec_env_t env, const char *str)
{
    Com_Printf("%s", str);
}

static void PF_wasm_bprint(wasm_exec_env_t env, int print_level, const char *str)
{
    PF_bprintf(print_level, "%s", str);
}

static uint32_t PF_wasm_cvar(wasm_exec_env_t env, const char *name, const char *value, int32_t flags)
{
    return PF_cvar(name, value, flags)->wasm;
}

static uint32_t PF_wasm_cvar_set(wasm_exec_env_t env, const char *name, const char *value)
{
    return Cvar_UserSet(name, value)->wasm;
}

static uint32_t PF_wasm_cvar_forceset(wasm_exec_env_t env, const char *name, const char *value)
{
    return Cvar_Set(name, value)->wasm;
}

static uint32_t PF_wasm_TagMalloc(wasm_exec_env_t env, uint32_t size, uint32_t tag)
{
	wasm_address_t loc = SV_AllocateWASMMemory(size);

	if (!loc) {
        Com_Error(ERR_DROP, "%s: out of memory", __func__);
    }

    wasm_tagged_mem_t *tagged = Z_TagMalloc(sizeof(wasm_tagged_mem_t), TAG_WASM);
    tagged->tag = tag;
    tagged->address = loc;
    List_Append(&tagged_memory, &tagged->list);

    return loc;
}

static void PF_wasm_TagFree(wasm_exec_env_t env, uint32_t ptr)
{
    wasm_tagged_mem_t *block;
    bool found = false;

    LIST_FOR_EACH(wasm_tagged_mem_t, block, &tagged_memory, list) {
        if (block->address == ptr) {
            found = true;
            break;
        }
    }

    if (!found) {
        Com_Error(ERR_DROP, "%s: bad memory block", __func__);
    }

    List_Remove(&block->list);

	SV_FreeWASMMemory(block->address);

    Z_Free(block);
}

static void PF_wasm_FreeTags(wasm_exec_env_t env, uint32_t tag)
{
    wasm_tagged_mem_t *block, *next;

    LIST_FOR_EACH_SAFE(wasm_tagged_mem_t, block, next, &tagged_memory, list) {
        if (block->tag == tag) {
            List_Remove(&block->list);
	        SV_FreeWASMMemory(block->address);
            Z_Free(block);
        }
    }
}

static void PF_wasm_configstring(wasm_exec_env_t env, int32_t id, const char *value)
{
    PF_configstring(id, value);
}

static int32_t PF_wasm_modelindex(wasm_exec_env_t env, const char *name)
{
    return PF_ModelIndex(name);
}

static int32_t PF_wasm_imageindex(wasm_exec_env_t env, const char *name)
{
    return PF_ImageIndex(name);
}

static int32_t PF_wasm_soundindex(wasm_exec_env_t env, const char *name)
{
    return PF_SoundIndex(name);
}

static void PF_wasm_cprint(wasm_exec_env_t env, wasm_address_t wasm_edict, int print_level, const char *str)
{
    WASM_ENTITY_VALIDATE_NO_NULL(wasm_edict);
    
	edict_t *native = entity_wasm_to_native(wasm_edict);

    PF_cprintf(native, print_level, "%s", str);
}

static void PF_wasm_centerprint(wasm_exec_env_t env, wasm_address_t wasm_edict, const char *str)
{
    WASM_ENTITY_VALIDATE_NO_NULL(wasm_edict);
    
	edict_t *native = entity_wasm_to_native(wasm_edict);

    PF_centerprintf(native, "%s", str);
}

static void PF_wasm_error(wasm_exec_env_t env, const char *str)
{
    Com_Error(ERR_DROP, "Game Error: %s", str);
}

static void PF_wasm_linkentity(wasm_exec_env_t env, wasm_address_t wasm_addr)
{
    WASM_ENTITY_VALIDATE_NO_NULL(wasm_addr);
    
	edict_t *native_edict = entity_wasm_to_native(wasm_addr);
    wasm_edict_t *wasm_edict = SV_ResolveWASMAddress(wasm_addr);

	copy_link_wasm_to_native(native_edict, wasm_edict);
	const bool copy_old_origin = wasm_edict->linkcount == 0;
    PF_LinkEdict(native_edict);
	if (copy_old_origin)
		VectorCopy(native_edict->s.old_origin, wasm_edict->s.old_origin);
	copy_link_native_to_wasm(wasm_edict, native_edict);
}

static void PF_wasm_unlinkentity(wasm_exec_env_t env, wasm_address_t wasm_addr)
{
    WASM_ENTITY_VALIDATE_NO_NULL(wasm_addr);
    
	edict_t *native_edict = entity_wasm_to_native(wasm_addr);
    wasm_edict_t *wasm_edict = SV_ResolveWASMAddress(wasm_addr);

	copy_link_wasm_to_native(native_edict, wasm_edict);
    PF_UnlinkEdict(native_edict);
	copy_link_native_to_wasm(wasm_edict, native_edict);
}

static void PF_wasm_setmodel(wasm_exec_env_t env, wasm_address_t wasm_addr, const char *model)
{
    WASM_ENTITY_VALIDATE_NO_NULL(wasm_addr);
    
	edict_t *native_edict = entity_wasm_to_native(wasm_addr);
    wasm_edict_t *wasm_edict = SV_ResolveWASMAddress(wasm_addr);

	copy_link_wasm_to_native(native_edict, wasm_edict);
    PF_setmodel(native_edict, model);
	copy_link_native_to_wasm(wasm_edict, native_edict);

	// setmodel also sets up mins, maxs, and modelindex
	wasm_edict->s.modelindex = native_edict->s.modelindex;
	VectorCopy(native_edict->mins, wasm_edict->mins);
	VectorCopy(native_edict->maxs, wasm_edict->maxs);
}

typedef struct
{
	qboolean		allsolid;
	qboolean		startsolid;
	vec_t			fraction;
	vec3_t			endpos;
	cplane_t		plane;
	wasm_address_t	surface;
	int             contents;
	wasm_address_t  ent;
} wasm_trace_t;

static wasm_address_t wasm_pmove_addr;
extern mtexinfo_t nulltexinfo;

static trace_t wasm_pmove_trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	uint32_t args[] = {
		wasm_pmove_addr,
		*(uint32_t *) (&start[0]),
		*(uint32_t *) (&start[1]),
		*(uint32_t *) (&start[2]),
		*(uint32_t *) (&mins[0]),
		*(uint32_t *) (&mins[1]),
		*(uint32_t *) (&mins[2]),
		*(uint32_t *) (&maxs[0]),
		*(uint32_t *) (&maxs[1]),
		*(uint32_t *) (&maxs[2]),
		*(uint32_t *) (&end[0]),
		*(uint32_t *) (&end[1]),
		*(uint32_t *) (&end[2]),
		wasm_mem.trace
	};

    SV_CallWASMFunction(wasm_ge.PmoveTrace, 14, args);

	wasm_trace_t *wasm_tr = SV_ResolveWASMAddress(wasm_mem.trace);
	static trace_t tr;

	tr.allsolid = wasm_tr->allsolid;
	tr.contents = wasm_tr->contents;
	VectorCopy(wasm_tr->endpos, tr.endpos);
	tr.ent = entity_wasm_to_native(wasm_tr->ent);
	tr.fraction = wasm_tr->fraction;
	tr.plane = wasm_tr->plane;
	tr.startsolid = wasm_tr->startsolid;

	if (wasm_tr->surface == wasm_mem.nulltexinfo) {
		tr.surface = &(nulltexinfo.c);
    } else {
		tr.surface = &(sv.cm.cache->texinfo[(wasm_tr->surface - wasm_mem.surfaces) / sizeof(csurface_t)].c);
    }

	return tr;
}

static int wasm_pmove_pointcontents(vec3_t start)
{
	uint32_t args[] = {
		wasm_pmove_addr,
		*(uint32_t *) (&start[0]),
		*(uint32_t *) (&start[1]),
		*(uint32_t *) (&start[2]),
	};

    SV_CallWASMFunction(wasm_ge.PmovePointContents, 4, args);

	return args[0];
}

typedef struct
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t	cmd;
	qboolean	snapinitial;    // if s has been changed outside pmove

	// results (out)
	int32_t	numtouch;
	wasm_address_t touchents[MAXTOUCH];

	vec3_t	viewangles;         // clamped
	vec_t	viewheight;

	vec3_t	mins, maxs;         // bounding box size

	wasm_address_t groundentity;
	int32_t	watertype;
	int32_t	waterlevel;

	// callbacks to test the world
	wasm_address_t trace;
	wasm_address_t pointcontents;
} wasm_pmove_t;

static void PF_wasm_Pmove(wasm_exec_env_t env, wasm_address_t wasm_pmove)
{
    WASM_MEMORY_VALIDATE_NO_NULL(wasm_pmove, sizeof(wasm_pmove_t));

	static pmove_t pm;
    wasm_pmove_t *wasm_pm = SV_ResolveWASMAddress(wasm_pmove);

	pm.s = wasm_pm->s;
	pm.cmd = wasm_pm->cmd;
	pm.snapinitial = wasm_pm->snapinitial;

	pm.trace = wasm_pmove_trace;
	pm.pointcontents = wasm_pmove_pointcontents;

	wasm_pmove_addr = wasm_pmove;

    PF_Pmove(&pm);

    wasm_pm = SV_ResolveWASMAddress(wasm_pmove);

	wasm_pm->s = pm.s;
	wasm_pm->numtouch = pm.numtouch;
	for (int32_t i = 0; i < pm.numtouch; i++)
		wasm_pm->touchents[i] = entity_native_to_wasm(pm.touchents[i]);
	VectorCopy(pm.viewangles, wasm_pm->viewangles);
	wasm_pm->viewheight = pm.viewheight;
	VectorCopy(pm.mins, wasm_pm->mins);
	VectorCopy(pm.maxs, wasm_pm->maxs);
	wasm_pm->groundentity = entity_native_to_wasm(pm.groundentity);
	wasm_pm->watertype = pm.watertype;
	wasm_pm->waterlevel = pm.waterlevel;
}

static void PF_wasm_trace(wasm_exec_env_t env, float start_x, float start_y, float start_z, float mins_x, float mins_y, float mins_z, float maxs_x, float maxs_y, float maxs_z,
    float end_x, float end_y, float end_z, wasm_address_t passent, int contentmask, wasm_address_t out)
{
    WASM_ENTITY_VALIDATE(passent);
    WASM_MEMORY_VALIDATE_NO_NULL(out, sizeof(wasm_trace_t));

    post_sync_entities();

    trace_t tr = SV_Trace((float[]) { start_x, start_y, start_z }, (float[]) { mins_x, mins_y, mins_z },
        (float[]) { maxs_x, maxs_y, maxs_z }, (float[]) { end_x, end_y, end_z }, entity_wasm_to_native(passent), contentmask);

    wasm_trace_t *out_tr = SV_ResolveWASMAddress(out);

	out_tr->allsolid = tr.allsolid;
	out_tr->contents = tr.contents;
	VectorCopy(tr.endpos, out_tr->endpos);
	out_tr->ent = entity_native_to_wasm(tr.ent);
	out_tr->fraction = tr.fraction;
	out_tr->plane = tr.plane;
	out_tr->startsolid = tr.startsolid;

	if (!tr.surface || !tr.surface->name[0]) {
		out_tr->surface = wasm_mem.nulltexinfo;
    } else {
        out_tr->surface = wasm_mem.surfaces + ((((mtexinfo_t *)tr.surface) - sv.cm.cache->texinfo) * sizeof(csurface_t));
    }
}

static int PF_wasm_pointcontents(wasm_exec_env_t env, float p_x, float p_y, float p_z)
{
    post_sync_entities();

    return SV_PointContents((float []) { p_x, p_y, p_z });
}

static void PF_wasm_WriteAngle(wasm_exec_env_t env, float c)
{
	MSG_WriteAngle(c);
}

static void PF_wasm_WriteByte(wasm_exec_env_t env, int c)
{
	MSG_WriteByte(c);
}

static void PF_wasm_WriteChar(wasm_exec_env_t env, int c)
{
	MSG_WriteChar(c);
}

static void PF_wasm_WriteDir(wasm_exec_env_t env, float p_x, float p_y, float p_z)
{
    MSG_WriteDir((const float[]) { p_x, p_y, p_z });
}

static void PF_wasm_WriteFloat(wasm_exec_env_t env, float p)
{
    PF_WriteFloat(p);
}

static void PF_wasm_WriteLong(wasm_exec_env_t env, int p)
{
	MSG_WriteLong(p);
}

static void PF_wasm_WritePosition(wasm_exec_env_t env, float p_x, float p_y, float p_z)
{
    MSG_WritePos((const float[]) { p_x, p_y, p_z });
}

static void PF_wasm_WriteShort(wasm_exec_env_t env, int p)
{
    MSG_WriteShort(p);
}

static void PF_wasm_WriteString(wasm_exec_env_t env, const char *p)
{
    MSG_WriteString(p);
}

static void PF_wasm_unicast(wasm_exec_env_t env, wasm_address_t ent, qboolean reliable)
{
    WASM_ENTITY_VALIDATE_NO_NULL(ent);
    
	edict_t *native = entity_wasm_to_native(ent);

    PF_Unicast(native, reliable);
}

static void PF_wasm_multicast(wasm_exec_env_t env, float origin_x, float origin_y, float origin_z, multicast_t to)
{
    SV_Multicast((float[]) { origin_x, origin_y, origin_z }, to);
}

static int PF_wasm_BoxEdicts(wasm_exec_env_t env, float mins_x, float mins_y, float mins_z, float maxs_x, float maxs_y, float maxs_z, wasm_address_t list, int maxcount, int areatype)
{
    WASM_MEMORY_VALIDATE_NO_NULL(list, sizeof(uint32_t) * maxcount);

    post_sync_entities();

#ifdef _MSC_VER
    edict_t **list_native = _malloca(sizeof(edict_t *) * maxcount);

    if (!list_native) {
        Com_Error(ERR_DROP, "%s: stack overflow", __func__);
    }
#else
    edict_t *list_native[maxcount];
#endif

    int count = SV_AreaEdicts((float[]) { mins_x, mins_y, mins_z }, (float[]) { maxs_x, maxs_y, maxs_z }, list_native, maxcount, areatype);

    uint32_t *list_wasm = SV_ResolveWASMAddress(list);

	for (int32_t i = 0; i < count; i++)
		list_wasm[i] = entity_native_to_wasm(list_native[i]);

	return count;
}

static void PF_wasm_sound(wasm_exec_env_t env, wasm_address_t ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
    WASM_ENTITY_VALIDATE_NO_NULL(ent);

	edict_t *native = entity_wasm_to_native(ent);

	sync_entity(SV_ResolveWASMAddress(ent), native, false);

    PF_StartSound(native, channel, soundindex, volume, attenuation, timeofs);
}

static void PF_wasm_positioned_sound(wasm_exec_env_t env, float origin_x, float origin_y, float origin_z, wasm_address_t ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
    WASM_ENTITY_VALIDATE_NO_NULL(ent);
    
	edict_t *native = entity_wasm_to_native(ent);

	sync_entity(SV_ResolveWASMAddress(ent), native, false);

    SV_StartSound(isfinite(origin_x) ? (float[]) { origin_x, origin_y, origin_z } : NULL, native, channel, soundindex, volume, attenuation, timeofs);
}

static int32_t PF_wasm_argc(wasm_exec_env_t env)
{
    return Cmd_Argc();
}

static uint32_t PF_wasm_argv(wasm_exec_env_t env, int32_t i)
{
    return 0;
	/*if (i < 0 || i >= sizeof(wasm_buffers_t::cmds) / sizeof(*wasm_buffers_t::cmds))
		return 0;

	return WASM_BUFFERS_OFFSET(cmds[i]);*/
}

static uint32_t PF_wasm_args(wasm_exec_env_t env)
{
    return 0;
	//return WASM_BUFFERS_OFFSET(scmd);
}

static void PF_wasm_AddCommandString(wasm_exec_env_t env, const char *string)
{
    PF_AddCommandString(string);
}

static qboolean PF_wasm_AreasConnected(wasm_exec_env_t env, int area1, int area2)
{
    return PF_AreasConnected(area1, area2);
}

static qboolean PF_wasm_inPHS(wasm_exec_env_t env, float p1_x, float p1_y, float p1_z, float p2_x, float p2_y, float p2_z)
{
    return PF_inPHS((float[]) { p1_x, p1_y, p1_z }, (float[]) { p2_x, p2_y, p2_z });
}

static qboolean PF_wasm_inPVS(wasm_exec_env_t env, float p1_x, float p1_y, float p1_z, float p2_x, float p2_y, float p2_z)
{
    return PF_inPVS((float[]) { p1_x, p1_y, p1_z }, (float[]) { p2_x, p2_y, p2_z });
}

static void PF_wasm_SetAreaPortalState(wasm_exec_env_t env, int portal, qboolean state)
{
    PF_SetAreaPortalState(portal, state);
}

static void PF_wasm_DebugGraph(wasm_exec_env_t env, float value, int color)
{
    PF_DebugGraph(value, color);
}

static game_capability_t PF_wasm_QueryEngineCapability(wasm_exec_env_t env, const char *cap)
{
    return false;
}

static bool SV_RegisterGameImports(void)
{
#define WASM_SYMBOL(name, sig) \
	{ #name, (void *) PF_wasm_ ## name, sig, NULL }

    static NativeSymbol native_symbols[] = {
	    WASM_SYMBOL(dprint, "($)"),
	    WASM_SYMBOL(bprint, "(i$)"),
	    WASM_SYMBOL(cprint, "(ii$)"),
	    WASM_SYMBOL(error, "(*)"),
	    WASM_SYMBOL(centerprint, "(i$)"),
	    WASM_SYMBOL(cvar, "($$i)i"),
	    WASM_SYMBOL(cvar_set, "($$)i"),
	    WASM_SYMBOL(cvar_forceset, "($$)i"),
	    WASM_SYMBOL(TagMalloc, "(ii)i"),
	    WASM_SYMBOL(TagFree, "(i)"),
	    WASM_SYMBOL(FreeTags, "(i)"),
	    WASM_SYMBOL(configstring, "(i$)"),
	    WASM_SYMBOL(modelindex, "($)i"),
	    WASM_SYMBOL(imageindex, "($)i"),
	    WASM_SYMBOL(soundindex, "($)i"),
	    WASM_SYMBOL(linkentity, "(i)"),
	    WASM_SYMBOL(unlinkentity, "(i)"),
	    WASM_SYMBOL(setmodel, "(i$)"),
	    WASM_SYMBOL(Pmove, "(i)"),
	    WASM_SYMBOL(trace, "(ffffffffffffiii)"),
	    WASM_SYMBOL(pointcontents, "(fff)i"),
	    WASM_SYMBOL(WriteAngle, "(f)"),
	    WASM_SYMBOL(WriteByte, "(i)"),
	    WASM_SYMBOL(WriteChar, "(i)"),
	    WASM_SYMBOL(WriteDir, "(fff)"),
	    WASM_SYMBOL(WriteFloat, "(f)"),
	    WASM_SYMBOL(WriteLong, "(i)"),
	    WASM_SYMBOL(WritePosition, "(fff)"),
	    WASM_SYMBOL(WriteShort, "(i)"),
	    WASM_SYMBOL(WriteString, "($)"),
	    WASM_SYMBOL(unicast, "(ii)"),
	    WASM_SYMBOL(multicast, "(fffi)"),
	    WASM_SYMBOL(BoxEdicts, "(ffffffiii)i"),
	    WASM_SYMBOL(sound, "(iiifff)"),
	    WASM_SYMBOL(positioned_sound, "(fffiiifff)"),
	    WASM_SYMBOL(argc, "()i"),
	    WASM_SYMBOL(argv, "(i)i"),
	    WASM_SYMBOL(args, "()i"),
	    WASM_SYMBOL(AddCommandString, "($)"),
	    WASM_SYMBOL(AreasConnected, "(ii)i"),
	    WASM_SYMBOL(inPHS, "(ii)i"),
	    WASM_SYMBOL(inPVS, "(ii)i"),
	    WASM_SYMBOL(SetAreaPortalState, "(ii)"),
	    WASM_SYMBOL(DebugGraph, "(fi)"),
	    WASM_SYMBOL(QueryEngineCapability, "($)i")
    };

#undef WASM_SYMBOL

    return wasm_runtime_register_natives("q2", native_symbols, sizeof(native_symbols) / sizeof(*native_symbols));
}

/*
===============
SV_InitGameWasm

Attempt to load a WASM game subsystem for
a new map. Returns false if we can't.
===============
*/
static bool _SV_InitGameWasm(void)
{
    bool success = false;

    // for debugging or `proxy' mods
    if (sys_forcegamelib->string[0])
        success = _SV_LoadGameWasm(sys_forcegamelib->string);

    // try game first
    if (!success && fs_game->string[0]) {
        success = SV_LoadGameWasm(fs_game->string, "q2pro_");
        if (!success)
            success = SV_LoadGameWasm(fs_game->string, "");
    }

    // then try baseq2
    if (!success) {
        success = SV_LoadGameWasm(BASEGAME, "q2pro_");
        if (!success)
            success = SV_LoadGameWasm(BASEGAME, "");
    }

    // all paths failed
    if (!success) {
        Com_Printf("Failed to load game WASM.\n");
        return false;
    }

    // got a game WASM, try initializing it.
	if (!wasm_runtime_init()) {
        Com_Printf("Failed to initialize WASM runtime.\n");
        return false;
    }

    // register API callbacks
    SV_RegisterGameImports();

	// parse the WASM file from buffer and create a WASM module
	module = wasm_runtime_load(wasm_binary, wasm_size, wasm_error, sizeof(wasm_error));

    if (!module) {
        Com_Printf("Failed to load WASM module: %s\n", wasm_error);
        return false;
    }

    // set up basename map first
    wasi_num_dirs = 1;

    if (*fs_game->string) {
        Q_strlcpy(wasi_dir[0], fs_game->string, sizeof(wasi_dir[0]));
    } else {
        Q_strlcpy(wasi_dir[0], BASEGAME, sizeof(wasi_dir[0]));
    }

    wasi_dir_map[0][0] = 0;
    Q_concat(wasi_dir_map[0], sizeof(wasi_dir_map[0]), fs_gamedir);

	wasm_runtime_set_wasi_args(module, wasi_dirs, wasi_num_dirs, wasi_dir_maps, wasi_num_dirs, NULL, 0, NULL, 0);

	// create an instance of the WASM module (WASM linear memory is ready)
	module_inst = wasm_runtime_instantiate(module, 8192, sys_wasmheapsize->integer, wasm_error, sizeof(wasm_error));

	if (!module_inst) {
        Com_Printf("Failed to instantiate WASM module: %s\n", wasm_error);
        return false;
    }

    // create execution environment
	exec_env = wasm_runtime_create_exec_env(module_inst, sys_wasmstacksize->integer);

	if (!exec_env) {
        Com_Printf("Failed to create WASM execution environment: %s\n", wasm_error);
        return false;
    }

    // initialize WASI
	wasm_function_t initialize_func = SV_GetWASMFunction("_initialize", NULL, true);

    SV_CallWASMFunction(initialize_func, 0, NULL);

    // it's a valid WASM module! It might still not be a valid mod binary, though.
    // now, we find all of the linked functions.
    uint32_t return_values[1] = { WASM_API_VERSION };

	wasm_function_t GetGameAPI = SV_GetWASMFunction("GetGameAPI", "(i)i", true);

    SV_CallWASMFunction(GetGameAPI, 1, return_values);

    if (return_values[0] != WASM_API_VERSION) {
        Com_Printf("Wasm library is version %d, expected %d\n",
                  return_values[0], WASM_API_VERSION);
        return false;
    }

	wasm_ge.GetEdicts = SV_GetWASMFunction("GetEdicts", "()i", true);
	wasm_ge.GetNumEdicts = SV_GetWASMFunction("GetNumEdicts", "()*", true);

	wasm_ge.PmoveTrace = SV_GetWASMFunction("PmoveTrace", "(******)", true);
	wasm_ge.PmovePointContents = SV_GetWASMFunction("PmovePointContents", "(**)", true);
	wasm_ge.Init = SV_GetWASMFunction("Init", NULL, true);
	wasm_ge.SpawnEntities = SV_GetWASMFunction("SpawnEntities", "($$$)", true);
	wasm_ge.ClientConnect = SV_GetWASMFunction("ClientConnect", "(*$)i", true);
	wasm_ge.ClientBegin = SV_GetWASMFunction("ClientBegin", "(*)", true);
	wasm_ge.ClientUserinfoChanged = SV_GetWASMFunction("ClientUserinfoChanged", "(*$)", true);
	wasm_ge.ClientCommand = SV_GetWASMFunction("ClientCommand", "(*)", true);
	wasm_ge.ClientDisconnect = SV_GetWASMFunction("ClientDisconnect", "(*)", true);
	wasm_ge.ClientThink = SV_GetWASMFunction("ClientThink", "(**)", true);
	wasm_ge.RunFrame = SV_GetWASMFunction("RunFrame", NULL, true);
	wasm_ge.ServerCommand = SV_GetWASMFunction("ServerCommand", NULL, true);
	wasm_ge.WriteGame = SV_GetWASMFunction("WriteGame", "($i)", true);
	wasm_ge.ReadGame = SV_GetWASMFunction("ReadGame", "($)", true);
	wasm_ge.WriteLevel = SV_GetWASMFunction("WriteLevel", "($)", true);
	wasm_ge.ReadLevel = SV_GetWASMFunction("ReadLevel", "($)", true);

	wasm_ge.QueryGameCapability = SV_GetWASMFunction("QueryGameCapability", "($)*", true);
    
    // from beyond this point, we mark as loaded so other subsystems can be made aware of
    // memory allocations (cvar)
    wasm_ge.loaded = true;

    return true;
}

static void SV_InitGameWasm(void)
{
    // Static memory that persists for the entire game module life
    wasm_mem.trace = SV_AllocateWASMMemory(sizeof(wasm_trace_t));
    wasm_mem.vectors = SV_AllocateWASMMemory(sizeof(vec3_t) * 4);
    wasm_mem.usercmd = SV_AllocateWASMMemory(sizeof(usercmd_t));

    if (!wasm_mem.trace || !wasm_mem.vectors || !wasm_mem.usercmd) {
        Com_Error(ERR_DROP, "Couldn't allocate WASM memory\n");
    }

    SV_CallWASMFunction(wasm_ge.Init, 0, NULL);

    uint32_t return_values[1];

	wasm_function_t GetMaxEdicts = SV_GetWASMFunction("GetMaxEdicts", "()i", true);

    SV_CallWASMFunction(GetMaxEdicts, 0, return_values);
    ge->pool.max_edicts = return_values[0];

    // sanitize max_edicts
    if (ge->pool.max_edicts <= sv_maxclients->integer || ge->pool.max_edicts > MAX_EDICTS) {
        Com_Error(ERR_DROP, "Game WASM returned bad max_edicts\n");
    }

    ge->pool.edicts = Z_TagMalloc(sizeof(edict_t) * ge->pool.max_edicts, TAG_WASM);
    memset(ge->pool.edicts, 0, sizeof(edict_t) * ge->pool.max_edicts);

	wasm_function_t GetEdictSize = SV_GetWASMFunction("GetEdictSize", "()i", true);

    SV_CallWASMFunction(GetEdictSize, 0, return_values);

    wasm_ge.edict_size = return_values[0];

    // sanitize edict_size
    if (!wasm_ge.edict_size || wasm_ge.edict_size > (unsigned)INT_MAX / MAX_EDICTS) {
        Com_Error(ERR_DROP, "Game WASM returned bad edict_size\n");
    }

    SV_SetWASMEntityPointers();
}

static void SV_SpawnEntitiesWasm(const char *mapname, const char *entstring, const char *spawnpoint)
{
    // Free all of these first so we can reclaim the space before allocations
    if (wasm_mem.nulltexinfo) {
        SV_FreeWASMMemory(wasm_mem.nulltexinfo);
    }
    if (wasm_mem.mapname) {
        SV_FreeWASMMemory(wasm_mem.mapname);
    }
    if (wasm_mem.entstring) {
        SV_FreeWASMMemory(wasm_mem.entstring);
    }
    if (wasm_mem.spawnpoint) {
        SV_FreeWASMMemory(wasm_mem.spawnpoint);
    }

    // Allocate static memory that persists for an entire level.
    wasm_mem.nulltexinfo = SV_AllocateWASMMemory(sizeof(csurface_t) * (sv.cm.cache->numtexinfo + 1));

    if (!wasm_mem.nulltexinfo) {
        Com_Error(ERR_DROP, "Couldn't allocate WASM memory\n");
    }

    wasm_mem.surfaces = wasm_mem.nulltexinfo + sizeof(csurface_t);

    // copy in the BSP's surface info
    for (int i = 0; i < sv.cm.cache->numtexinfo; i++) {
        wasm_address_t surf_addr = wasm_mem.surfaces + (sizeof(csurface_t) * i);
        mtexinfo_t *texinfo = sv.cm.cache->texinfo + i;

        memcpy(SV_ResolveWASMAddress(surf_addr), &texinfo->c, sizeof(csurface_t));
    }

    wasm_mem.mapname = SV_AllocateWASMMemory(strlen(mapname) + 1);
    
    if (!wasm_mem.mapname) {
        Com_Error(ERR_DROP, "Couldn't allocate WASM memory\n");
    }

    Q_strlcpy(SV_ResolveWASMAddress(wasm_mem.mapname), mapname, strlen(mapname) + 1);

    wasm_mem.entstring = SV_AllocateWASMMemory(strlen(entstring) + 1);
    
    if (!wasm_mem.entstring) {
        Com_Error(ERR_DROP, "Couldn't allocate WASM memory\n");
    }

    Q_strlcpy(SV_ResolveWASMAddress(wasm_mem.entstring), entstring, strlen(entstring) + 1);

    wasm_mem.spawnpoint = SV_AllocateWASMMemory(strlen(spawnpoint) + 1);
    
    if (!wasm_mem.spawnpoint) {
        Com_Error(ERR_DROP, "Couldn't allocate WASM memory\n");
    }

    Q_strlcpy(SV_ResolveWASMAddress(wasm_mem.spawnpoint), spawnpoint, strlen(spawnpoint) + 1);

    uint32_t args[] = {
        wasm_mem.mapname,
        wasm_mem.entstring,
        wasm_mem.spawnpoint
    };

    SV_CallWASMFunction(wasm_ge.SpawnEntities, 3, args);

    post_sync_entities();
}

static void SV_WriteGameWasm(const char *filename, qboolean autosave)
{
    wasm_address_t filename_addr = SV_AllocateWASMMemory(strlen(filename) + 1);
    Q_strlcpy(SV_ResolveWASMAddress(filename_addr), filename, strlen(filename) + 1);

    uint32_t args[] = {
        filename_addr,
        autosave
    };

    SV_CallWASMFunction(wasm_ge.WriteGame, 2, args);
    SV_FreeWASMMemory(filename_addr);
}

static void SV_ReadGameWasm(const char *filename)
{
    wasm_address_t filename_addr = SV_AllocateWASMMemory(strlen(filename) + 1);
    Q_strlcpy(SV_ResolveWASMAddress(filename_addr), filename, strlen(filename) + 1);

    uint32_t args[] = {
        filename_addr
    };

    SV_CallWASMFunction(wasm_ge.ReadGame, 1, args);
    SV_FreeWASMMemory(filename_addr);

    SV_SetWASMEntityPointers();

	post_sync_entities();
}

static void SV_WriteLevelWasm(const char *filename)
{
    wasm_address_t filename_addr = SV_AllocateWASMMemory(strlen(filename) + 1);
    Q_strlcpy(SV_ResolveWASMAddress(filename_addr), filename, strlen(filename) + 1);

    uint32_t args[] = {
        filename_addr
    };

    SV_CallWASMFunction(wasm_ge.WriteLevel, 1, args);
    SV_FreeWASMMemory(filename_addr);
}

static void SV_ReadLevelWasm(const char *filename)
{
    wasm_address_t filename_addr = SV_AllocateWASMMemory(strlen(filename) + 1);
    Q_strlcpy(SV_ResolveWASMAddress(filename_addr), filename, strlen(filename) + 1);

    uint32_t args[] = {
        filename_addr
    };

    SV_CallWASMFunction(wasm_ge.ReadLevel, 1, args);
    SV_FreeWASMMemory(filename_addr);

	post_sync_entities();
}

static qboolean SV_ClientConnectWasm(edict_t *ent, char *userinfo)
{
	pre_sync_entities();

    wasm_address_t userinfo_addr = SV_AllocateWASMMemory(MAX_INFO_STRING);
    Q_strlcpy(SV_ResolveWASMAddress(userinfo_addr), userinfo, MAX_INFO_STRING);

    uint32_t args[] = {
        entity_native_to_wasm(ent),
        userinfo_addr
    };

    SV_CallWASMFunction(wasm_ge.ClientConnect, 2, args);

    Q_strlcpy(userinfo, SV_ResolveWASMAddress(userinfo_addr), MAX_INFO_STRING);

    SV_FreeWASMMemory(userinfo_addr);

    post_sync_entities();

    return args[0];
}

static void SV_ClientBeginWasm(edict_t *ent)
{
	pre_sync_entities();

    uint32_t args[] = {
        entity_native_to_wasm(ent)
    };
    
    SV_CallWASMFunction(wasm_ge.ClientBegin, 1, args);

    post_sync_entities();
}

static qboolean SV_ClientUserinfoChangedWasm(edict_t *ent, char *userinfo)
{
	pre_sync_entities();

    wasm_address_t userinfo_addr = SV_AllocateWASMMemory(MAX_INFO_STRING);
    Q_strlcpy(SV_ResolveWASMAddress(userinfo_addr), userinfo, MAX_INFO_STRING);

    uint32_t args[] = {
        entity_native_to_wasm(ent),
        userinfo_addr
    };
    
    SV_CallWASMFunction(wasm_ge.ClientConnect, 2, args);

    SV_FreeWASMMemory(userinfo_addr);

    post_sync_entities();

    return args[0];
}

static void SV_ClientDisconnectWasm(edict_t *ent)
{
	pre_sync_entities();

    uint32_t args[] = {
        entity_native_to_wasm(ent)
    };

    SV_CallWASMFunction(wasm_ge.ClientDisconnect, 1, args);

    post_sync_entities();
}

static void SV_ClientCommandWasm(edict_t *ent)
{
	pre_sync_entities();

    uint32_t args[] = {
        entity_native_to_wasm(ent)
    };

    SV_CallWASMFunction(wasm_ge.ClientCommand, 1, args);

    post_sync_entities();
}

static void SV_ClientThinkWasm(edict_t *ent, usercmd_t *cmd)
{
	pre_sync_entities();

    memcpy(SV_ResolveWASMAddress(wasm_mem.usercmd), cmd, sizeof(*cmd));

    uint32_t args[] = {
        entity_native_to_wasm(ent),
        wasm_mem.usercmd
    };

    SV_CallWASMFunction(wasm_ge.ClientThink, 2, args);

    post_sync_entities();
}

static void SV_RunFrameWasm(void)
{
	pre_sync_entities();

    SV_CallWASMFunction(wasm_ge.RunFrame, 0, NULL);

    post_sync_entities();
}

static void SV_ServerCommandWasm(void)
{
	pre_sync_entities();

    SV_CallWASMFunction(wasm_ge.ServerCommand, 0, NULL);

    post_sync_entities();
}

static game_capability_t SV_QueryGameCapabilityWasm(const char *cap)
{
    wasm_address_t cap_addr = SV_AllocateWASMMemory(strlen(cap) + 1);

    Q_strlcpy(SV_ResolveWASMAddress(cap_addr), cap, strlen(cap) + 1);

    uint32_t args[] = {
        cap_addr
    };
    
    SV_CallWASMFunction(wasm_ge.QueryGameCapability, 1, args);

    SV_FreeWASMMemory(cap_addr);

    return (game_capability_t) args[0];
}

static wasm_game_export_t wasm_game_exports = {
    WASM_API_VERSION,

    // set up our game exports wrapper
    SV_InitGameWasm,
    SV_ShutdownGameWasm,

    SV_SpawnEntitiesWasm,
    
    SV_WriteGameWasm,
    SV_ReadGameWasm,
    SV_WriteLevelWasm,
    SV_ReadLevelWasm,
    
    SV_ClientConnectWasm,
    SV_ClientBeginWasm,
    SV_ClientUserinfoChangedWasm,
    SV_ClientDisconnectWasm,
    SV_ClientCommandWasm,
    SV_ClientThinkWasm,
    
    SV_RunFrameWasm,

    SV_ServerCommandWasm,

    NULL,
    0,
    0,
    0,

    SV_QueryGameCapabilityWasm
};

/*
===============
SV_InitGameWasm

Attempt to load a WASM game subsystem for
a new map. Returns false if we can't.
===============
*/
bool SV_InitGameWasmProgs(void)
{
    if (sys_nowasm->value)
    {
        Com_Printf("Skipping WASM due to sys_nowasm\n");
        return false;
    }

    List_Init(&tagged_memory);

    if (!_SV_InitGameWasm())
    {
        SV_ShutdownGameWasm();
        return false;
    }

    ge = &wasm_game_exports.ge;

    ge->Init();

    ge->pool.edict_size = sizeof(edict_t);

    post_sync_entities();

    return true;
}
