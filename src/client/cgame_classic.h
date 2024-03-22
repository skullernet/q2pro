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

#include "shared/game.h"
#include "common/pmove.h"

// Extension structure to give classic cgame some extended features
typedef struct {
    int api_version;

    // Whether server is using "extended" protocol
    bool (*IsExtendedServer)(void);

    // Drawing color support
    void (*ClearColor)(void);
    void (*SetAlpha)(float alpha);
    void (*SetColor)(uint32_t color);

    // Return pmove parameters for server
    const pmoveParams_t *(*GetPmoveParams)(void);
} cgame_q2pro_extended_support_ext_t;

// Extension name
extern const char cgame_q2pro_extended_support_ext[];

extern cgame_export_t *GetClassicCGameAPI(cgame_import_t *import);
