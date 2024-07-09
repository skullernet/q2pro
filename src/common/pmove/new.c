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

#include "shared/shared.h"
#include "common/pmove.h"

#define PMOVE_NEW 1
#define PMOVE_TYPE pmove_new_t
#define PMOVE_FUNC PmoveNew
#define PMOVE_TIME_SHIFT pmp->time_shift
#define PMOVE_C2S(x) SignExtend(COORD2SHORT(x), pmp->coord_bits)
#include "template.c"
