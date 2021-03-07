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

#if __wasm__
#error This file should not be compiled from a WASM compiler.
#endif

#define NATIVE_LINKAGE
#include "g_local.h"
#include "shared/native.h"

wasm_game_export_t   globals;

// Bring in the exports from elsewhere in the source.
void InitGame(void);
void ShutdownGame(void);
void SpawnEntities(const char *, const char *, const char *);
void WriteGame(const char *, qboolean);
void ReadGame(const char *);
void WriteLevel(const char *);
void ReadLevel(const char *);
void ClientThink(edict_t *, usercmd_t *);
qboolean ClientConnect(edict_t *, char *);
void ClientUserinfoChanged(edict_t *, char *);
void ClientDisconnect(edict_t *);
void ClientBegin(edict_t *);
void ClientCommand(edict_t *);
void G_RunFrame(void);
game_capability_t QueryGameCapability(const char *);

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
static void GetGameAPI_V3(game_import_t *import)
{
    gi_bprintf = import->bprintf;
    gi_dprintf = import->dprintf;
    gi_cprintf = import->cprintf;
    gi_centerprintf = import->centerprintf;
    gi_sound = import->sound;
    gi_positioned_sound = import->positioned_sound;
    
    gi_configstring = import->configstring;

    gi_error = import->error;
    
    gi_modelindex = import->modelindex;
    gi_soundindex = import->soundindex;
    gi_imageindex = import->imageindex;
    
    gi_trace = import->trace;
    gi_pointcontents = import->pointcontents;
    gi_inPVS = import->inPVS;
    gi_inPHS = import->inPHS;
    gi_SetAreaPortalState = import->SetAreaPortalState;
    gi_AreasConnected = import->AreasConnected;
    
    gi_linkentity = import->linkentity;
    gi_unlinkentity = import->unlinkentity;
    gi_BoxEdicts = import->BoxEdicts;
    gi_Pmove = import->Pmove;
    
    gi_multicast = import->multicast;
    gi_unicast = import->unicast;
    gi_WriteChar = import->WriteChar;
    gi_WriteByte = import->WriteByte;
    gi_WriteShort = import->WriteShort;
    gi_WriteLong = import->WriteLong;
    gi_WriteFloat = import->WriteFloat;
    gi_WriteString = import->WriteString;
    gi_WritePosition = import->WritePosition;
    gi_WriteDir = import->WriteDir;
    gi_WriteAngle = import->WriteAngle;
    
    gi_TagMalloc = import->TagMalloc;
    gi_TagFree = import->TagFree;
    gi_FreeTags = import->FreeTags;
    
    gi_cvar = import->cvar;
    gi_cvar_set = import->cvar_set;
    gi_cvar_forceset = import->cvar_forceset;
    
    gi_argc = import->argc;
    gi_argv = import->argv;
    gi_args = import->args;

    gi_AddCommandString = import->AddCommandString;

    gi_DebugGraph = import->DebugGraph;

    globals.ge.apiversion = GAME_API_VERSION;
    globals.ge.Init = InitGame;
    globals.ge.Shutdown = ShutdownGame;
    globals.ge.SpawnEntities = SpawnEntities;

    globals.ge.WriteGame = WriteGame;
    globals.ge.ReadGame = ReadGame;
    globals.ge.WriteLevel = WriteLevel;
    globals.ge.ReadLevel = ReadLevel;

    globals.ge.ClientThink = ClientThink;
    globals.ge.ClientConnect = ClientConnect;
    globals.ge.ClientUserinfoChanged = ClientUserinfoChanged;
    globals.ge.ClientDisconnect = ClientDisconnect;
    globals.ge.ClientBegin = ClientBegin;
    globals.ge.ClientCommand = ClientCommand;

    globals.ge.RunFrame = G_RunFrame;

    globals.ge.ServerCommand = ServerCommand;

    globals.ge.pool.edict_size = sizeof(edict_t);

    pool = &globals.ge.pool;
}

static void GetGameAPI_V87(wasm_game_import_t *import)
{
    // make sure it's a valid 87 implementation.
    // returning will fall back to just implementing 3.
    if (import->gi.DebugGraph != NULL)
        return;

    // we're only interested in supporting version 87, which
    // provides only one new ability: a simple query feature
    // between engine and game. This should allow for anything
    // that any new API should need in the future.
    if (import->apiversion != WASM_API_VERSION)
        return;

    gi_QueryEngineCapability = import->QueryEngineCapability;

    globals.ge.apiversion = WASM_API_VERSION;

    globals.QueryGameCapability = QueryGameCapability;
}

q_exported game_export_t *GetGameAPI(game_import_t *import)
{
    if (!globals.ge.apiversion)
        GetGameAPI_V3(import);
    else
        GetGameAPI_V87((wasm_game_import_t *) import);

    return (game_export_t *) &globals;
}