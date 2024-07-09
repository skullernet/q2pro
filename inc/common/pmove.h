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
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/

typedef struct {
    bool        qwmode;
    bool        airaccelerate;
    bool        strafehack;
    bool        flyhack;
    bool        waterhack;
    byte        time_shift;
    byte        coord_bits;
    float       speedmult;
    float       watermult;
    float       maxspeed;
    float       friction;
    float       waterfriction;
    float       flyfriction;
} pmoveParams_t;

void PmoveOld(pmove_old_t *pmove, const pmoveParams_t *params);
void PmoveNew(pmove_new_t *pmove, const pmoveParams_t *params);

void PmoveInit(pmoveParams_t *pmp);
void PmoveEnableQW(pmoveParams_t *pmp);
void PmoveEnableExt(pmoveParams_t *pmp);
