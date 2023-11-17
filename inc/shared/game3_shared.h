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

// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum {
    // can accelerate and turn
    G3PM_NORMAL,
    G3PM_SPECTATOR,
    // no acceleration or turning
    G3PM_DEAD,
    G3PM_GIB,     // different bounding box
    G3PM_FREEZE
} game3_pmtype_t;

static inline pmtype_t pmtype_from_game3(game3_pmtype_t pmtype)
{
    switch(pmtype)
    {
    case G3PM_NORMAL:
        return PM_NORMAL;
    case G3PM_SPECTATOR:
        return PM_SPECTATOR;
    case G3PM_DEAD:
        return PM_DEAD;
    case G3PM_GIB:
        return PM_GIB;
    case G3PM_FREEZE:
        return PM_FREEZE;
    }
    return (pmtype_t)pmtype;
}

static inline game3_pmtype_t pmtype_to_game3(pmtype_t pmtype)
{
    switch(pmtype)
    {
    case PM_NORMAL:
        return G3PM_NORMAL;
    case PM_GRAPPLE:
    case PM_NOCLIP:
    case PM_SPECTATOR:
        return G3PM_SPECTATOR;
    case PM_DEAD:
        return G3PM_DEAD;
    case PM_GIB:
        return G3PM_GIB;
    case PM_FREEZE:
        return G3PM_FREEZE;
    }
    return (game3_pmtype_t)pmtype;
}

// pmove->pm_flags
#define G3PMF_DUCKED          BIT(0)
#define G3PMF_JUMP_HELD       BIT(1)
#define G3PMF_ON_GROUND       BIT(2)
#define G3PMF_TIME_WATERJUMP  BIT(3)      // pm_time is waterjump
#define G3PMF_TIME_LAND       BIT(4)      // pm_time is time before rejump
#define G3PMF_TIME_TELEPORT   BIT(5)      // pm_time is non-moving time
#define G3PMF_NO_PREDICTION   BIT(6)      // temporarily disables prediction (used for grappling hook)
#define G3PMF_TELEPORT_BIT    BIT(7)      // used by Q2PRO (non-extended servers)

static inline pmflags_t pmflags_from_game3(byte pmflags)
{
    pmflags_t new_pmflags = 0;
    if(pmflags & G3PMF_DUCKED)
        new_pmflags |= PMF_DUCKED;
    if(pmflags & G3PMF_JUMP_HELD)
        new_pmflags |= PMF_JUMP_HELD;
    if(pmflags & G3PMF_ON_GROUND)
        new_pmflags |= PMF_ON_GROUND;
    if(pmflags & G3PMF_TIME_WATERJUMP)
        new_pmflags |= PMF_TIME_WATERJUMP;
    if(pmflags & G3PMF_TIME_LAND)
        new_pmflags |= PMF_TIME_LAND;
    if(pmflags & G3PMF_TIME_TELEPORT)
        new_pmflags |= PMF_TIME_TELEPORT;
    if(pmflags & G3PMF_NO_PREDICTION)
        new_pmflags |= PMF_NO_PREDICTION;
    if(pmflags & G3PMF_TELEPORT_BIT)
        new_pmflags |= PMF_TELEPORT_BIT;
    return new_pmflags;
}

static inline byte pmflags_to_game3(pmflags_t pmflags)
{
    byte new_pmflags = 0;
    if(pmflags & PMF_DUCKED)
        new_pmflags |= G3PMF_DUCKED;
    if(pmflags & PMF_JUMP_HELD)
        new_pmflags |= G3PMF_JUMP_HELD;
    if(pmflags & PMF_ON_GROUND)
        new_pmflags |= G3PMF_ON_GROUND;
    if(pmflags & PMF_TIME_WATERJUMP)
        new_pmflags |= G3PMF_TIME_WATERJUMP;
    if(pmflags & PMF_TIME_LAND)
        new_pmflags |= G3PMF_TIME_LAND;
    if(pmflags & PMF_TIME_TELEPORT)
        new_pmflags |= G3PMF_TIME_TELEPORT;
    if(pmflags & PMF_NO_PREDICTION)
        new_pmflags |= G3PMF_NO_PREDICTION;
    if(pmflags & PMF_TELEPORT_BIT)
        new_pmflags |= G3PMF_TELEPORT_BIT;
    return new_pmflags;
}

typedef struct {
    game3_pmtype_t pm_type;

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
