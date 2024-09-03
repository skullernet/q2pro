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
 *
 * Game is not required to implement all entry points in game_export_ex_t.
 * Non-implemented entry points must be set to NULL. Server, however, must
 * implement all entry points in game_import_ex_t for advertised API version.
 *
 * Only upstream Q2PRO engine may define new extended API versions. Mods should
 * never do this on their own, but may implement private extensions obtainable
 * via GetExtension() callback.
 *
 * API version history:
 * 1 - Initial release.
 * 2 - Added CustomizeEntity().
 * 3 - Added EntityVisibleToClient(), renamed CustomizeEntity() to
 * CustomizeEntityToClient() and changed the meaning of return value.
 */

#define GAME_API_VERSION_EX_MINIMUM             1
#define GAME_API_VERSION_EX_CUSTOMIZE_ENTITY    2
#define GAME_API_VERSION_EX_ENTITY_VISIBLE      3
#define GAME_API_VERSION_EX                     3

typedef enum {
    VIS_PVS     = 0,
    VIS_PHS     = 1,
    VIS_NOAREAS = 2     // can be OR'ed with one of above
} vis_t;

typedef struct {
    entity_state_t s;
#if USE_PROTOCOL_EXTENSIONS
    entity_state_extension_t x;
#endif
} customize_entity_t;

typedef struct {
    uint32_t    apiversion;
    uint32_t    structsize;

    void        (*local_sound)(edict_t *target, const vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
    const char  *(*get_configstring)(int index);
    trace_t     (*q_gameabi clip)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *clip, int contentmask);
    qboolean    (*inVIS)(const vec3_t p1, const vec3_t p2, vis_t vis);

    void        *(*GetExtension)(const char *name);
    void        *(*TagRealloc)(void *ptr, size_t size);
} game_import_ex_t;

typedef struct {
    uint32_t    apiversion;
    uint32_t    structsize;

    void        *(*GetExtension)(const char *name);
    qboolean    (*CanSave)(void);
    void        (*PrepFrame)(void);
    void        (*RestartFilesystem)(void); // called when fs_restart is issued
    qboolean    (*CustomizeEntityToClient)(edict_t *client, edict_t *ent, customize_entity_t *temp); // if true is returned, `temp' must be initialized
    qboolean    (*EntityVisibleToClient)(edict_t *client, edict_t *ent);
} game_export_ex_t;

typedef const game_export_ex_t *(*game_entry_ex_t)(const game_import_ex_t *);

/*
==============================================================================

SERVER API EXTENSIONS

==============================================================================
*/

#define FILESYSTEM_API_V1 "FILESYSTEM_API_V1"

typedef struct {
    int64_t     (*OpenFile)(const char *path, qhandle_t *f, unsigned mode); // returns file length
    int         (*CloseFile)(qhandle_t f);
    int         (*LoadFile)(const char *path, void **buffer, unsigned flags, unsigned tag);

    int         (*ReadFile)(void *buffer, size_t len, qhandle_t f);
    int         (*WriteFile)(const void *buffer, size_t len, qhandle_t f);
    int         (*FlushFile)(qhandle_t f);
    int64_t     (*TellFile)(qhandle_t f);
    int         (*SeekFile)(qhandle_t f, int64_t offset, int whence);
    int         (*ReadLine)(qhandle_t f, char *buffer, size_t size);

    void        **(*ListFiles)(const char *path, const char *filter, unsigned flags, int *count_p);
    void        (*FreeFileList)(void **list);

    const char  *(*ErrorString)(int error);
} filesystem_api_v1_t;

#define DEBUG_DRAW_API_V1 "DEBUG_DRAW_API_V1"

typedef struct {
    void (*ClearDebugLines)(void);
    void (*AddDebugLine)(const vec3_t start, const vec3_t end, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugPoint)(const vec3_t point, float size, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugAxis)(const vec3_t origin, const vec3_t angles, float size, uint32_t time, qboolean depth_test);
    void (*AddDebugBounds)(const vec3_t mins, const vec3_t maxs, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugSphere)(const vec3_t origin, float radius, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugCircle)(const vec3_t origin, float radius, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugCylinder)(const vec3_t origin, float half_height, float radius, uint32_t color, uint32_t time,
                             qboolean depth_test);
    void (*AddDebugArrow)(const vec3_t start, const vec3_t end, float size, uint32_t line_color,
                          uint32_t arrow_color, uint32_t time, qboolean depth_test);
    void (*AddDebugCurveArrow)(const vec3_t start, const vec3_t ctrl, const vec3_t end, float size,
                               uint32_t line_color, uint32_t arrow_color, uint32_t time, qboolean depth_test);
    void (*AddDebugText)(const vec3_t origin, const vec3_t angles, const char *text,
                         float size, uint32_t color, uint32_t time, qboolean depth_test);
} debug_draw_api_v1_t;
