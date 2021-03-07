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

#ifndef WASM_GAME_H
#define WASM_GAME_H

#include <stdint.h>
#include "game.h"
#include "native.h"

// Chosen because:
// - 'W' in ASCII
// - Doesn't conflict with any known API versions
#define WASM_API_VERSION    87

// This may seem weird, but, since WASM doesn't deal with
// import/export as function pointers this is completely legal.
// On WASM, a capability check will either return true or false.
// On native, however, it will return a function pointer, or NULL.
// Note that the Query*Capability functions are only guaranteed to
// "work" before InitGame has completed execution. Any usage of the
// function after that should always return NULL.
#ifdef __wasm__
typedef qboolean game_capability_t;
#define CAPABILITY_FALSE false
#else
typedef void (*game_capability_t)();
#define CAPABILITY_FALSE NULL
#endif

typedef struct {
    game_import_t   gi;

    int32_t apiversion;
    
    game_capability_t (*QueryEngineCapability)(const char *cap);
} wasm_game_import_t;

typedef struct {
    game_export_t   ge;
    
    game_capability_t (*QueryGameCapability)(const char *cap);
} wasm_game_export_t;

#ifdef GAME_INCLUDE
// see game.h for what NATIVE_IMPORT does.
NATIVE_LINKAGE game_capability_t NATIVE_IMPORT(gi_QueryEngineCapability)(const char *cap);
#endif

// =====
// non_standard_layout::xxx 
// =====
// Shared with WASM modules
typedef enum {
    WF_INVALID,

    WF_INT8,
    WF_INT16,
    WF_INT32,
    WF_INT64,
    WF_FLOAT32,
    WF_FLOAT64,
    WF_POINTER,

    WF_UNSIGNED = 32
} wasm_field_type_t;

#define WF_FIELD_TYPE(t) \
    ((t) & ~WF_UNSIGNED)

#define WF_FIELD_FLAGS(t) \
    ((t) & WF_UNSIGNED)

typedef struct {
    wasm_field_type_t   type;
    uint32_t            offset;
    uint32_t            count;
} wasm_field_t;

#endif // WASM_GAME_H