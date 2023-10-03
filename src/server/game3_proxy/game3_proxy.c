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

#include "../server.h"
#include "game3_proxy.h"
#include "game3.h"

#include <assert.h>
#include <malloc.h>

static void sync_single_edict_server_to_game(int index);
static void sync_edicts_server_to_game(void);
static void sync_single_edict_game_to_server(int index);
static void sync_edicts_game_to_server(void);

static game_import_t game_import;
static const game_import_ex_t *game_import_ex;

static game3_export_t *game3_export;
static const game3_export_ex_t *game3_export_ex;
static game_export_t game_export;

#define GAME_EDICT_NUM(n) ((game3_edict_t *)((byte *)game3_export->edicts + game3_export->edict_size*(n)))
#define NUM_FOR_GAME_EDICT(e) ((int)(((byte *)(e) - (byte *)game3_export->edicts) / game3_export->edict_size))

static edict_t *server_edicts;

static edict_t *translate_edict_from_game(game3_edict_t *ent)
{
    assert(!ent || (ent >= GAME_EDICT_NUM(0) && ent < GAME_EDICT_NUM(game3_export->num_edicts)));
    return ent ? &server_edicts[NUM_FOR_GAME_EDICT(ent)] : NULL;
}

static game3_edict_t* translate_edict_to_game(edict_t* ent)
{
    assert(!ent || (ent >= server_edicts && ent < server_edicts + game_export.num_edicts));
    return ent ? GAME_EDICT_NUM(ent - server_edicts) : NULL;
}

static void wrap_unicast(game3_edict_t *gent, qboolean reliable)
{
    edict_t *ent = translate_edict_from_game(gent);
    game_import.unicast(ent, reliable);
}

static void wrap_bprintf(int printlevel, const char *fmt, ...)
{
    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    va_start(argptr, fmt);
    size_t len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }
    game_import.Broadcast_Print(printlevel, msg);
}

static void wrap_dprintf(const char *fmt, ...)
{
    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    va_start(argptr, fmt);
    size_t len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }

#if USE_SAVEGAMES
    // detect YQ2 game lib by unique first two messages
    if (!sv.gamedetecthack)
        sv.gamedetecthack = 1 + !strcmp(fmt, "Game is starting up.\n");
    else if (sv.gamedetecthack == 2)
        sv.gamedetecthack = 3 + !strcmp(fmt, "Game is %s built on %s.\n");
#endif

    game_import.Com_Print(msg);
}

static void wrap_cprintf(game3_edict_t *gent, int level, const char *fmt, ...)
{
    edict_t *ent = translate_edict_from_game(gent);

    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    va_start(argptr, fmt);
    size_t len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }
    game_import.Client_Print(ent, level, msg);
}

static void wrap_centerprintf(game3_edict_t *gent, const char *fmt, ...)
{
    edict_t *ent = translate_edict_from_game(gent);

    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    va_start(argptr, fmt);
    size_t len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }
    game_import.Center_Print(ent, msg);
}

static q_noreturn void wrap_error(const char *fmt, ...)
{
    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    va_start(argptr, fmt);
    size_t len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
    }
    game_import.Com_Error(msg);
}


static void wrap_setmodel(game3_edict_t *gent, const char *name)
{
    game_export.num_edicts = game3_export->num_edicts;

    int ent_idx = NUM_FOR_GAME_EDICT(gent);
    sync_single_edict_game_to_server(ent_idx);
    game_import.setmodel(translate_edict_from_game(gent), name);
    sync_single_edict_server_to_game(ent_idx);
}

static void wrap_sound(game3_edict_t *gent, int channel,
                       int soundindex, float volume,
                       float attenuation, float timeofs)
{
    edict_t *ent = translate_edict_from_game(gent);
    game_import.sound(ent, channel, soundindex, volume, attenuation, timeofs);
}

static void wrap_positioned_sound(const vec3_t origin, game3_edict_t *gent, int channel,
                                  int soundindex, float volume,
                                  float attenuation, float timeofs)
{
    edict_t *ent = translate_edict_from_game(gent);
    game_import.positioned_sound(origin, ent, channel, soundindex, volume, attenuation, timeofs);
}

static void wrap_unlinkentity(game3_edict_t *ent)
{
    int ent_idx = NUM_FOR_GAME_EDICT(ent);
    sync_single_edict_game_to_server(ent_idx);
    game_import.unlinkentity(translate_edict_from_game(ent));
    sync_single_edict_server_to_game(ent_idx);
}

static void wrap_linkentity(game3_edict_t *ent)
{
    game_export.num_edicts = game3_export->num_edicts;

    int ent_idx = NUM_FOR_GAME_EDICT(ent);
    sync_single_edict_game_to_server(ent_idx);
    game_import.linkentity(translate_edict_from_game(ent));
    sync_single_edict_server_to_game(ent_idx);
}

static int wrap_BoxEdicts(const vec3_t mins, const vec3_t maxs, game3_edict_t **glist, int maxcount, int areatype)
{
    edict_t **list = alloca(maxcount * sizeof(edict_t *));
    int num_edicts = game_import.BoxEdicts(mins, maxs, list, maxcount, areatype);
    for (int i = 0; i < num_edicts; i++) {
        glist[i] = translate_edict_to_game(list[i]);
    }
    return num_edicts;
}

static game3_trace_t wrap_trace(const vec3_t start, const vec3_t mins,
                                const vec3_t maxs, const vec3_t end,
                                game3_edict_t *passedict, int contentmask)
{
    trace_t str = game_import.trace(start, mins, maxs, end, translate_edict_from_game(passedict), contentmask);
    game3_trace_t tr;
    tr.allsolid = str.allsolid;
    tr.startsolid = str.startsolid;
    tr.fraction = str.fraction;
    VectorCopy(str.endpos, tr.endpos);
    tr.plane = str.plane;
    tr.surface = str.surface;
    tr.contents = str.contents;
    tr.ent = translate_edict_to_game(str.ent);
    return tr;
}

static void wrap_local_sound(game3_edict_t *target, const vec3_t origin, game3_edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
    game_import.local_sound(translate_edict_from_game(target), origin, translate_edict_from_game(ent), channel, soundindex, volume, attenuation, timeofs, 0);
}

static const char *wrap_get_configstring(int index)
{
    return game_import.get_configstring(index);
}

static trace_t wrap_clip(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, game3_edict_t *clip, int contentmask)
{
    return game_import_ex->clip(start, mins, maxs, end, translate_edict_from_game(clip), contentmask);
}

static qboolean wrap_inVIS(const vec3_t p1, const vec3_t p2, vis_t vis)
{
    return game_import_ex->inVIS(p1, p2, vis);
}

static void *wrap_GetExtension(const char *name)
{
    return game_import_ex->GetExtension(name);
}

static void *wrap_TagRealloc(void *ptr, size_t size)
{
    return game_import_ex->TagRealloc(ptr, size);
}

// Macro to check whether a member that wasn't synced has been unexpectedly changed
#define VERIFY_UNCHANGED(pred, a, b, member) \
    if (pred)                                \
    {                                        \
        assert((a).member == (b).member);    \
    }

static void sync_single_edict_server_to_game(int index)
{
    edict_t *server_edict = &server_edicts[index];
    server_entity_t *sent = &sv.entities[index];
    game3_edict_t *game_edict = GAME_EDICT_NUM(index);

    // Skip unused entities
    bool server_edict_inuse = server_edict->inuse || HAS_EFFECTS(server_edict);
    if(!server_edict_inuse && !game_edict->inuse)
        return;

    bool is_new = !(game_edict->inuse || HAS_EFFECTS(game_edict));
    game_edict->inuse = server_edict->inuse;
    // Don't change fields if edict became unused
    if(!game_edict->inuse)
        return;

    // Check that fields not changed by server are, indeed, unchanged
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, origin[0]);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, origin[1]);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, origin[2]);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, angles[0]);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, angles[1]);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, angles[2]);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, modelindex2);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, modelindex3);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, modelindex4);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, frame);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, skinnum);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, effects);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, renderfx);
    VERIFY_UNCHANGED(!is_new, server_edict->s, game_edict->s, sound);

    game_edict->s.number = server_edict->s.number;
    VectorCopy(server_edict->s.old_origin, game_edict->s.old_origin);
    game_edict->s.modelindex = server_edict->s.modelindex;
    game_edict->s.solid = server_edict->s.solid;
    game_edict->s.event = server_edict->s.event;

    VERIFY_UNCHANGED(!is_new, *game_edict, *server_edict, client);
    game_edict->linkcount = server_edict->linkcount;
    game_edict->area.prev = server_edict->linked ? &game_edict->area : NULL; // emulate entity being linked
    game_edict->num_clusters = sent->num_clusters;
    memcpy(&game_edict->clusternums, &sent->clusternums, sizeof(game_edict->clusternums));
    game_edict->headnode = sent->headnode;
    game_edict->areanum = server_edict->areanum;
    game_edict->areanum2 = server_edict->areanum2;
    game_edict->svflags = server_edict->svflags;

    /* Although it's said "If the size, position, or solidity changes, [an edict] must be relinked.",
     * those fields may not only be changed by linkentity. */
    // changed by setmodel
    VectorCopy(server_edict->mins, game_edict->mins);
    VectorCopy(server_edict->maxs, game_edict->maxs);
    // changed by linkentity
    VectorCopy(server_edict->absmin, game_edict->absmin);
    VectorCopy(server_edict->absmax, game_edict->absmax);
    VectorCopy(server_edict->size, game_edict->size);
    VERIFY_UNCHANGED(!is_new, *game_edict, *server_edict, solid);
    VERIFY_UNCHANGED(!is_new, *game_edict, *server_edict, clipmask);

    // slightly changed 'translate_edict_to_game', as owners may be beyond num_edicts when loading a game
    assert(!server_edict->owner || (server_edict->owner >= server_edicts && server_edict->owner < server_edicts + game_export.max_edicts));
    game_edict->owner = server_edict->owner ? GAME_EDICT_NUM(server_edict->owner - server_edicts) : NULL;
}

// Sync edicts from server to game
static void sync_edicts_server_to_game(void)
{
    for (int i = 0; i < game_export.num_edicts; i++) {
        sync_single_edict_server_to_game(i);
    }

    game3_export->num_edicts = game_export.num_edicts;
}

static void game_entity_state_to_server(entity_state_t* server_state, const game3_entity_state_t* game_state)
{
    server_state->number = game_state->number;
    VectorCopy(game_state->origin, server_state->origin);
    VectorCopy(game_state->angles, server_state->angles);
    VectorCopy(game_state->old_origin, server_state->old_origin);
    server_state->modelindex = game_state->modelindex;
    server_state->modelindex2 = game_state->modelindex2;
    server_state->modelindex3 = game_state->modelindex3;
    server_state->modelindex4 = game_state->modelindex4;
    server_state->frame = game_state->frame;
    server_state->skinnum = game_state->skinnum;
    server_state->effects = game_state->effects;
    server_state->renderfx = game_state->renderfx;
    server_state->solid = game_state->solid;
    server_state->sound = game_state->sound;
    server_state->event = game_state->event;
}

static void sync_single_edict_game_to_server(int index)
{
    edict_t *server_edict = &server_edicts[index];
    server_entity_t *sent = &sv.entities[index];
    game3_edict_t *game_edict = GAME_EDICT_NUM(index);

    server_edict->client = game_edict->client; // client may be set, even though inuse == false

    // Skip unused entities
    bool game_edict_inuse = game_edict->inuse || HAS_EFFECTS(game_edict);
    if(!game_edict_inuse && !server_edict->inuse)
        return;

    server_edict->inuse = game_edict->inuse;
    // HAS_EFFECTS() needs correct entity state
    game_entity_state_to_server(&server_edict->s, &game_edict->s);
    // Don't change more fields if edict became unused
    if(!server_edict->inuse)
        return;

    // Check whether game cleared links
    if (!game_edict->area.next && !game_edict->area.prev && (sent->area.next || sent->area.prev))
        PF_UnlinkEdict(server_edict);
    server_edict->svflags = game_edict->svflags;

    VectorCopy(game_edict->mins, server_edict->mins);
    VectorCopy(game_edict->maxs, server_edict->maxs);
    VectorCopy(game_edict->absmin, server_edict->absmin);
    VectorCopy(game_edict->absmax, server_edict->absmax);
    VectorCopy(game_edict->size, server_edict->size);
    server_edict->solid = game_edict->solid;
    server_edict->clipmask = game_edict->clipmask;

    server_edict->owner = game_edict->owner ? &server_edicts[NUM_FOR_GAME_EDICT(game_edict->owner)] : NULL;
}

// Sync edicts from game to server
static void sync_edicts_game_to_server(void)
{
    for (int i = 0; i < game3_export->num_edicts; i++) {
        sync_single_edict_game_to_server(i);
    }

    game_export.num_edicts = game3_export->num_edicts;
}

static void wrap_Init(void)
{
    game3_export->Init();

    server_edicts = Z_Mallocz(sizeof(edict_t) * game3_export->max_edicts);
    game_export.edicts = server_edicts;
    game_export.edict_size = sizeof(edict_t);
    game_export.max_edicts = game3_export->max_edicts;
    game_export.num_edicts = game3_export->num_edicts;

    sync_edicts_game_to_server();
}

static void wrap_Shutdown(void)
{
    game3_export->Shutdown();

    Z_Free(server_edicts);
    server_edicts = NULL;
}

static void wrap_SpawnEntities(const char *mapname, const char *entstring, const char *spawnpoint)
{
    sync_edicts_server_to_game();
    game3_export->SpawnEntities(mapname, entstring, spawnpoint);
    sync_edicts_game_to_server();
}

static void wrap_WriteGame(const char *filename, qboolean autosave)
{
    sync_edicts_server_to_game();
    game3_export->WriteGame(filename, autosave);
}

static void wrap_ReadGame(const char *filename)
{
    game3_export->ReadGame(filename);
    sync_edicts_game_to_server();
}

static void wrap_WriteLevel(const char *filename)
{
    sync_edicts_server_to_game();
    game3_export->WriteLevel(filename);
}

static void wrap_ReadLevel(const char *filename)
{
    game3_export->ReadLevel(filename);
    sync_edicts_game_to_server();
}

static qboolean wrap_ClientConnect(edict_t *ent, char *userinfo)
{
    int ent_idx = NUM_FOR_EDICT(ent);
    sync_single_edict_server_to_game(ent_idx);
    qboolean result = game3_export->ClientConnect(translate_edict_to_game(ent), userinfo);
    sync_single_edict_game_to_server(ent_idx);
    game_export.num_edicts = game3_export->num_edicts;
    return result;
}

static void wrap_ClientBegin(edict_t *ent)
{
    int ent_idx = NUM_FOR_EDICT(ent);
    sync_single_edict_server_to_game(ent_idx);
    game3_export->ClientBegin(translate_edict_to_game(ent));
    sync_single_edict_game_to_server(ent_idx);
    game_export.num_edicts = game3_export->num_edicts;
}

static void wrap_ClientUserinfoChanged(edict_t *ent, char *userinfo)
{
    int ent_idx = NUM_FOR_EDICT(ent);
    sync_single_edict_server_to_game(ent_idx);
    game3_export->ClientUserinfoChanged(translate_edict_to_game(ent), userinfo);
    sync_single_edict_game_to_server(ent_idx);
    game_export.num_edicts = game3_export->num_edicts;
}

static void wrap_ClientDisconnect(edict_t *ent)
{
    int ent_idx = NUM_FOR_EDICT(ent);
    sync_single_edict_server_to_game(ent_idx);
    game3_export->ClientDisconnect(translate_edict_to_game(ent));
    sync_single_edict_game_to_server(ent_idx);
    game_export.num_edicts = game3_export->num_edicts;
}

static void wrap_ClientCommand(edict_t *ent)
{
    // ClientCommand() may spawn new entitities, so sync them all
    sync_edicts_server_to_game();
    game3_export->ClientCommand(translate_edict_to_game(ent));
    sync_edicts_game_to_server();
}

static void wrap_ClientThink(edict_t *ent, usercmd_t *cmd)
{
    // ClientThink() may spawn new entitities, so sync them all
    sync_edicts_server_to_game();
    game3_export->ClientThink(translate_edict_to_game(ent), cmd);
    sync_edicts_game_to_server();
}

static void wrap_RunFrame(void)
{
    sync_edicts_server_to_game();
    game3_export->RunFrame();
    sync_edicts_game_to_server();
}

static void wrap_ServerCommand(void)
{
    sync_edicts_server_to_game();
    game3_export->ServerCommand();
    sync_edicts_game_to_server();
}

static const game3_import_ex_t game3_import_ex = {
    .apiversion = GAME3_API_VERSION_EX,
    .structsize = sizeof(game3_import_ex_t),

    .local_sound = wrap_local_sound,
    .get_configstring = wrap_get_configstring,
    .clip = wrap_clip,
    .inVIS = wrap_inVIS,

    .GetExtension = wrap_GetExtension,
    .TagRealloc = wrap_TagRealloc,
};

game_export_t *GetGame3Proxy(game_import_t *import, const game_import_ex_t *import_ex, void *game3_entry, void *game3_ex_entry)
{
    game_import = *import;
    game_import_ex = import_ex;

    game3_import_t import3;
    game3_export_t *(*entry)(game3_import_t *) = game3_entry;
    const game3_export_ex_t *(*entry_ex)(const game3_import_ex_t *) = game3_ex_entry;

    import3.multicast = import->multicast;
    import3.unicast = wrap_unicast;
    import3.bprintf = wrap_bprintf;
    import3.dprintf = wrap_dprintf;
    import3.cprintf = wrap_cprintf;
    import3.centerprintf = wrap_centerprintf;
    import3.error = wrap_error;

    import3.linkentity = wrap_linkentity;
    import3.unlinkentity = wrap_unlinkentity;
    import3.BoxEdicts = wrap_BoxEdicts;
    import3.trace = wrap_trace;
    import3.pointcontents = import->pointcontents;
    import3.setmodel = wrap_setmodel;
    import3.inPVS = import->inPVS;
    import3.inPHS = import->inPHS;
    import3.Pmove = import->Pmove;

    import3.modelindex = import->modelindex;
    import3.soundindex = import->soundindex;
    import3.imageindex = import->imageindex;

    import3.configstring = import->configstring;
    import3.sound = wrap_sound;
    import3.positioned_sound = wrap_positioned_sound;

    import3.WriteChar = import->WriteChar;
    import3.WriteByte = import->WriteByte;
    import3.WriteShort = import->WriteShort;
    import3.WriteLong = import->WriteLong;
    import3.WriteFloat = import->WriteFloat;
    import3.WriteString = import->WriteString;
    import3.WritePosition = import->WritePosition;
    import3.WriteDir = import->WriteDir;
    import3.WriteAngle = import->WriteAngle;

    import3.TagMalloc = import->TagMalloc;
    import3.TagFree = import->TagFree;
    import3.FreeTags = import->FreeTags;

    import3.cvar = import->cvar;
    import3.cvar_set = import->cvar_set;
    import3.cvar_forceset = import->cvar_forceset;

    import3.argc = import->argc;
    import3.argv = import->argv;
    import3.args = import->args;
    import3.AddCommandString = import->AddCommandString;

    import3.DebugGraph = import->DebugGraph;
    import3.SetAreaPortalState = import->SetAreaPortalState;
    import3.AreasConnected = import->AreasConnected;

    game3_export = entry(&import3);
    if (game3_ex_entry)
        game3_export_ex = entry_ex(&game3_import_ex);

    game_export.apiversion = GAME_API_VERSION;
    game_export.Init = wrap_Init;
    game_export.Shutdown = wrap_Shutdown;
    game_export.SpawnEntities = wrap_SpawnEntities;
    game_export.WriteGame = wrap_WriteGame;
    game_export.ReadGame = wrap_ReadGame;
    game_export.WriteLevel = wrap_WriteLevel;
    game_export.ReadLevel = wrap_ReadLevel;

    game_export.ClientConnect = wrap_ClientConnect;
    game_export.ClientBegin = wrap_ClientBegin;
    game_export.ClientUserinfoChanged = wrap_ClientUserinfoChanged;
    game_export.ClientDisconnect = wrap_ClientDisconnect;
    game_export.ClientCommand = wrap_ClientCommand;
    game_export.ClientThink = wrap_ClientThink;
    game_export.RunFrame = wrap_RunFrame;
    game_export.ServerCommand = wrap_ServerCommand;

    return &game_export;
}
