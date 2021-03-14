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

#if !__wasm__
#error This file should not be compiled from a native compiler.
#endif

#include "g_local.h"
#include "g_wasm.h"

static edict_pool_t entity_pool;

// Exports from elsewhere
void InitGame(void);

void WASM_Init(void) WASM_EXPORT(Init)
{
	InitGame();
}

void ShutdownGame(void);

void WASM_Shutdown(void) WASM_EXPORT(Shutdown)
{
	ShutdownGame();
}

void SpawnEntities(const char *, const char *, const char *) WASM_EXPORT(SpawnEntities);

void WASM_SpawnEntities(const char *mapname, const char *entstring, const char *spawnpoint) WASM_EXPORT(SpawnEntities)
{
	SpawnEntities(mapname, entstring, spawnpoint);
}

void WriteGame(const char *, qboolean);

void WASM_WriteGame(const char *filename, qboolean autosave) WASM_EXPORT(WriteGame)
{
	WriteGame(filename, autosave);
}

void ReadGame(const char *);

void WASM_ReadGame(const char *filename) WASM_EXPORT(ReadGame)
{
	ReadGame(filename);
}

void WriteLevel(const char *);

void WASM_WriteLevel(const char *filename) WASM_EXPORT(WriteLevel)
{
	WriteLevel(filename);
}

void ReadLevel(const char *);

void WASM_ReadLevel(const char *filename) WASM_EXPORT(ReadLevel)
{
	ReadLevel(filename);
}

void ClientThink(edict_t *, usercmd_t *);

void WASM_ClientThink(edict_t *ent, usercmd_t *cmd) WASM_EXPORT(ClientThink)
{
	ClientThink(ent, cmd);
}

qboolean ClientConnect(edict_t *, char *);

qboolean WASM_ClientConnect(edict_t *ent, char *userinfo) WASM_EXPORT(ClientConnect)
{
	return ClientConnect(ent, userinfo);
}

void ClientUserinfoChanged(edict_t *, char *);

void WASM_ClientUserinfoChanged(edict_t *ent, char *userinfo) WASM_EXPORT(ClientUserinfoChanged)
{
	ClientUserinfoChanged(ent, userinfo);
}

void ClientDisconnect(edict_t *);

void WASM_ClientDisconnect(edict_t *ent) WASM_EXPORT(ClientDisconnect)
{
	ClientDisconnect(ent);
}

void ClientBegin(edict_t *);

void WASM_ClientBegin(edict_t *ent) WASM_EXPORT(ClientBegin)
{
	ClientBegin(ent);
}

void ClientCommand(edict_t *);

void WASM_ClientCommand(edict_t *ent) WASM_EXPORT(ClientCommand)
{
	ClientCommand(ent);
}

void G_RunFrame(void);

void WASM_RunFrame(void) WASM_EXPORT(RunFrame)
{
	G_RunFrame();
}

void ServerCommand(void);

void WASM_ServerCommand(void) WASM_EXPORT(ServerCommand)
{
	ServerCommand();
}

game_capability_t QueryGameCapability(const char *);

game_capability_t WASM_QueryGameCapability(const char *cap) WASM_EXPORT(QueryGameCapability)
{
	return QueryGameCapability(cap);
}

// Imports from native
static char	string[1024];

#define PARSE_VAR_ARGS \
	va_list argptr; \
	va_start (argptr, fmt); \
	Q_vsnprintf (string, sizeof(string), fmt, argptr); \
	va_end (argptr)

void _gi_bprint(int printlevel, const char *msg) WASM_IMPORT(bprint);

void q_printf(2, 3) gi_bprintf(int printlevel, const char *fmt, ...)
{
	PARSE_VAR_ARGS;
	_gi_bprint(printlevel, string);
}

void _gi_dprint(const char *msg) WASM_IMPORT(dprint);

void q_printf(1, 2) gi_dprintf(const char *fmt, ...)
{
	PARSE_VAR_ARGS;
	_gi_dprint(string);
}

void _gi_cprint(edict_t *ent, int printlevel, const char *msg) WASM_IMPORT(cprint);

void q_printf(3, 4) gi_cprintf(edict_t *ent, int printlevel, const char *fmt, ...)
{
	PARSE_VAR_ARGS;
	_gi_cprint(ent, printlevel, string);
}

void _gi_centerprint(edict_t *ent, const char *msg) WASM_IMPORT(centerprint);

void q_printf(2, 3) gi_centerprintf(edict_t *ent, const char *fmt, ...)
{
	PARSE_VAR_ARGS;
	_gi_centerprint(ent, string);
}

void _gi_sound(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs) WASM_IMPORT(sound);

void gi_sound(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
	_gi_sound(ent, channel, soundindex, volume, attenuation, timeofs);
}

void _gi_positioned_sound(float origin_x, float origin_y, float origin_z, edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs) WASM_IMPORT(positioned_sound);

void gi_positioned_sound(vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
	if (!origin)
		gi_sound(ent, channel, soundindex, volume, attenuation, timeofs);
	else
		_gi_positioned_sound(origin[0], origin[1], origin[2], ent, channel, soundindex, volume, attenuation, timeofs);
}

void _gi_configstring(int num, const char *string) WASM_IMPORT(configstring);

void gi_configstring(int num, const char *string)
{
	_gi_configstring(num, string);
}

void q_noreturn _gi_error(const char *msg) WASM_IMPORT(error);

void q_noreturn q_printf(1, 2) gi_error(const char *fmt, ...)
{
	PARSE_VAR_ARGS;
	_gi_error(string);
}

int _gi_modelindex(const char *name) WASM_IMPORT(modelindex);

int gi_modelindex(const char *name)
{
	return _gi_modelindex(name);
}

int _gi_soundindex(const char *name) WASM_IMPORT(soundindex);

int gi_soundindex(const char *name)
{
	return _gi_soundindex(name);
}

int _gi_imageindex(const char *name) WASM_IMPORT(imageindex);

int gi_imageindex(const char *name)
{
	return _gi_imageindex(name);
}

void _gi_setmodel(edict_t *ent, const char *name) WASM_IMPORT(setmodel);

void gi_setmodel(edict_t *ent, const char *name)
{
	_gi_setmodel(ent, name);
}

void _gi_trace(float start_x, float start_y, float start_z, float mins_x, float mins_y, float mins_z,
	float maxs_x, float maxs_y, float maxs_z, float end_x, float end_y, float end_z, edict_t *passent,
	int contentmask, trace_t *out) WASM_IMPORT(trace);

trace_t q_gameabi gi_trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask)
{
	static trace_t tr;
	
	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	_gi_trace(start[0], start[1], start[2], mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], end[0], end[1], end[2], passent, contentmask, &tr);

	return tr;
}

int _gi_pointcontents(float point_x, float point_y, float point_z) WASM_IMPORT(pointcontents);

int gi_pointcontents(vec3_t point)
{
	return _gi_pointcontents(point[0], point[1], point[2]);
}

qboolean _gi_inPVS(float p1_x, float p1_y, float p1_z, float p2_x, float p2_y, float p2_z) WASM_IMPORT(inPVS);

qboolean gi_inPVS(vec3_t p1, vec3_t p2)
{
	return _gi_inPVS(p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
}

qboolean _gi_inPHS(float p1_x, float p1_y, float p1_z, float p2_x, float p2_y, float p2_z) WASM_IMPORT(inPHS);

qboolean gi_inPHS(vec3_t p1, vec3_t p2)
{
	return _gi_inPHS(p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
}

void _gi_SetAreaPortalState(int portalnum, qboolean open) WASM_IMPORT(SetAreaPortalState);

void gi_SetAreaPortalState(int portalnum, qboolean open)
{
	_gi_SetAreaPortalState(portalnum, open);
}

qboolean _gi_AreasConnected(int area1, int area2) WASM_IMPORT(AreasConnected);

qboolean gi_AreasConnected(int area1, int area2)
{
	return _gi_AreasConnected(area1, area2);
}

void _gi_linkentity(edict_t *ent) WASM_IMPORT(linkentity);

void gi_linkentity(edict_t *ent)
{
	_gi_linkentity(ent);
}

void _gi_unlinkentity(edict_t *ent) WASM_IMPORT(unlinkentity);

void gi_unlinkentity(edict_t *ent)
{
	_gi_unlinkentity(ent);
}

int _gi_BoxEdicts(float mins_x, float mins_y, float mins_z, float maxs_x, float maxs_y, float maxs_z, edict_t **list, int maxcount, int areatype) WASM_IMPORT(BoxEdicts);

int gi_BoxEdicts(vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype)
{
	return _gi_BoxEdicts(mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], list, maxcount, areatype);
}

void _gi_Pmove(pmove_t *pmove) WASM_IMPORT(Pmove);

void gi_Pmove(pmove_t *pmove)
{
	_gi_Pmove(pmove);
}

void _gi_multicast(float origin_x, float origin_y, float origin_z, multicast_t to) WASM_IMPORT(multicast);

void gi_multicast(vec3_t origin, multicast_t to)
{
	_gi_multicast(origin[0], origin[1], origin[2], to);
}

void _gi_unicast(edict_t *ent, qboolean reliable) WASM_IMPORT(unicast);

void gi_unicast(edict_t *ent, qboolean reliable)
{
	_gi_unicast(ent, reliable);
}

void _gi_WriteChar(int c) WASM_IMPORT(WriteChar);

void gi_WriteChar(int c)
{
	_gi_WriteChar(c);
}

void _gi_WriteByte(int c) WASM_IMPORT(WriteByte);

void gi_WriteByte(int c)
{
	_gi_WriteByte(c);
}

void _gi_WriteShort(int c) WASM_IMPORT(WriteShort);

void gi_WriteShort(int c)
{
	_gi_WriteShort(c);
}

void _gi_WriteLong(int c) WASM_IMPORT(WriteLong);

void gi_WriteLong(int c)
{
	_gi_WriteLong(c);
}

void _gi_WriteFloat(float f) WASM_IMPORT(WriteFloat);

void gi_WriteFloat(float f)
{
	_gi_WriteFloat(f);
}

void _gi_WriteString(const char *s) WASM_IMPORT(WriteString);

void gi_WriteString(const char *s)
{
	_gi_WriteString(s);
}

void _gi_WritePosition(float pos_x, float pos_y, float pos_z) WASM_IMPORT(WritePosition);

void gi_WritePosition(const vec3_t pos)
{
	_gi_WritePosition(pos[0], pos[1], pos[2]);
}

void _gi_WriteDir(float pos_x, float pos_y, float pos_z) WASM_IMPORT(WriteDir);

void gi_WriteDir(const vec3_t pos)
{
	_gi_WriteDir(pos[0], pos[1], pos[2]);
}

void _gi_WriteAngle(float f) WASM_IMPORT(WriteAngle);

void gi_WriteAngle(float f)
{
	_gi_WriteAngle(f);
}

void *_gi_TagMalloc(unsigned size, unsigned tag) WASM_IMPORT(TagMalloc);

void *gi_TagMalloc(unsigned size, unsigned tag)
{
	return _gi_TagMalloc(size, tag);
}

void _gi_TagFree(void *block) WASM_IMPORT(TagFree);

void gi_TagFree(void *block)
{
	_gi_TagFree(block);
}

void _gi_FreeTags(unsigned tag) WASM_IMPORT(FreeTags);

void gi_FreeTags(unsigned tag)
{
	_gi_FreeTags(tag);
}

cvar_t *_gi_cvar(const char *var_name, const char *value, int flags) WASM_IMPORT(cvar);

cvar_t *gi_cvar(const char *var_name, const char *value, int flags)
{
	return _gi_cvar(var_name, value, flags);
}

cvar_t *_gi_cvar_set(const char *var_name, const char *value) WASM_IMPORT(cvar_set);

cvar_t *gi_cvar_set(const char *var_name, const char *value)
{
	return _gi_cvar_set(var_name, value);
}

cvar_t *_gi_cvar_forceset(const char *var_name, const char *value) WASM_IMPORT(cvar_forceset);

cvar_t *gi_cvar_forceset(const char *var_name, const char *value)
{
	return _gi_cvar_forceset(var_name, value);
}

int _gi_argc(void) WASM_IMPORT(argc);

int gi_argc(void)
{
	return _gi_argc();
}

char *_gi_argv(int n) WASM_IMPORT(argv);

char *gi_argv(int n)
{
	return _gi_argv(n);
}

char *_gi_args(void) WASM_IMPORT(args);

char *gi_args(void)
{
	return _gi_args();
}

void _gi_AddCommandString(const char *text) WASM_IMPORT(AddCommandString);

void gi_AddCommandString(const char *text)
{
	return _gi_AddCommandString(text);
}

void _gi_DebugGraph(float value, int color) WASM_IMPORT(DebugGraph);

void gi_DebugGraph(float value, int color)
{
	return _gi_DebugGraph(value, color);
}

game_capability_t _gi_QueryEngineCapability(const char *cap) WASM_IMPORT(QueryEngineCapability);

game_capability_t gi_QueryEngineCapability(const char *cap)
{
	return _gi_QueryEngineCapability(cap);
}

int32_t GetGameAPI(int32_t apiversion) WASM_EXPORT(GetGameAPI)
{
	// bad version
	if (apiversion != GAME_API_EXTENDED_VERSION)
		return 0;

	pool = &entity_pool;
	pool->edict_size = sizeof(edict_t);

	// we're good!
	return GAME_API_EXTENDED_VERSION;
}

edict_t *GetEdicts(void) WASM_EXPORT(GetEdicts)
{
	return entity_pool.edicts;
}

int32_t *GetNumEdicts(void) WASM_EXPORT(GetNumEdicts)
{
	return &entity_pool.num_edicts;
}

int32_t GetMaxEdicts(void) WASM_EXPORT(GetMaxEdicts)
{
	return entity_pool.max_edicts;
}

int32_t GetEdictSize(void) WASM_EXPORT(GetEdictSize)
{
	return entity_pool.edict_size;
}

void PmoveTrace(pmove_t *pm, float start_x, float start_y, float start_z, float mins_x, float mins_y, float mins_z,
	float maxs_x, float maxs_y, float maxs_z, float end_x, float end_y, float end_z, trace_t *out) WASM_EXPORT(PmoveTrace)
{
	*out = pm->trace((float []) { start_x, start_y, start_z }, (float []) { mins_x, mins_y, mins_z },
		(float []) { maxs_x, maxs_y, maxs_z }, (float []) { end_x, end_y, end_z });
}

int32_t PmovePointContents(pmove_t *pm, float p_x, float p_y, float p_z) WASM_EXPORT(PmovePointContents)
{
	return pm->pointcontents((float []) { p_x, p_y, p_z });
}
