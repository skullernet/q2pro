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

typedef struct game3_edict_s game3_edict_t;

/* TODO: These are pmtype, pmflags as defined by Q2PRO;
 * need translation to "new" values */
#if 0
// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum {
    // can accelerate and turn
    PM_NORMAL,
    PM_SPECTATOR,
    // no acceleration or turning
    PM_DEAD,
    PM_GIB,     // different bounding box
    PM_FREEZE
} pmtype_t;

// pmove->pm_flags
#define PMF_DUCKED          BIT(0)
#define PMF_JUMP_HELD       BIT(1)
#define PMF_ON_GROUND       BIT(2)
#define PMF_TIME_WATERJUMP  BIT(3)      // pm_time is waterjump
#define PMF_TIME_LAND       BIT(4)      // pm_time is time before rejump
#define PMF_TIME_TELEPORT   BIT(5)      // pm_time is non-moving time
#define PMF_NO_PREDICTION   BIT(6)      // temporarily disables prediction (used for grappling hook)
#define PMF_TELEPORT_BIT    BIT(7)      // used by Q2PRO (non-extended servers)
#endif

typedef struct {
    pmtype_t    pm_type;

    short       origin[3];      // 12.3
    short       velocity[3];    // 12.3
    byte        pm_flags;       // ducked, jump_held, etc
    byte        pm_time;        // each unit = 8 ms
    short       gravity;
    short       delta_angles[3];    // add to command angles to get view direction
                                    // changed by spawns, rotating objects, and teleporters
} game3_pmove_state_t;

typedef struct {
    qboolean    allsolid;   // if true, plane is not valid
    qboolean    startsolid; // if true, the initial point was in a solid area
    float       fraction;   // time completed, 1.0 = didn't hit anything
    vec3_t      endpos;     // final position
    cplane_t    plane;      // surface normal at impact
    csurface_t  *surface;   // surface hit
    int         contents;   // contents on other side of surface hit
    game3_edict_t  *ent;    // not set by CM_*() functions
} game3_trace_t;

typedef struct game3_usercmd_s {
    byte    msec;
    byte    buttons;
    short   angles[3];
    short   forwardmove, sidemove, upmove;
    byte    impulse;        // remove?
    byte    lightlevel;     // light level the player is standing on
} game3_usercmd_t;

typedef struct {
    // state (in / out)
    game3_pmove_state_t s;

    // command (in)
    game3_usercmd_t cmd;
    qboolean        snapinitial;    // if s has been changed outside pmove

    // results (out)
    int         numtouch;
    game3_edict_t  *touchents[MAXTOUCH];

    vec3_t      viewangles;         // clamped
    float       viewheight;

    vec3_t      mins, maxs;         // bounding box size

    game3_edict_t  *groundentity;
    int         watertype;
    int         waterlevel;

    // callbacks to test the world
    game3_trace_t (* q_gameabi trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end);
    int         (*pointcontents)(const vec3_t point);
} game3_pmove_t;
