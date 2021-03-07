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

#ifndef SERVER_H
#define SERVER_H

#include "common/net/net.h"

typedef enum {
    ss_dead,            // no map loaded
    ss_loading,         // spawning level edicts
    ss_game,            // actively running
    ss_pic,             // showing static picture
    ss_broadcast        // running MVD client
} server_state_t;

#if USE_ICMP
void SV_ErrorEvent(netadr_t *from, int ee_errno, int ee_info);
#endif
void SV_Init(void);
void SV_Shutdown(const char *finalmsg, error_type_t type);
unsigned SV_Frame(unsigned msec);
#if USE_SYSCON
void SV_SetConsoleTitle(void);
#endif
//void SV_ConsoleOutput(const char *msg);

#if USE_MVD_CLIENT && USE_CLIENT
int MVD_GetDemoPercent(bool *paused, int *framenum);
#endif

#if USE_CLIENT
char *SV_GetSaveInfo(const char *dir);
#endif

#if USE_SERVER && USE_WASM
bool SV_IsWASMRunning(void);
bool SV_ValidateWASMAddress(wasm_address_t address, uint32_t size);
// Be wary of the three functions below: when memory grows on the WASM side,
// native pointers may not be pointing to the same place they were before.
// Its lifetime should only be considered to be valid *up until* the next WASM
// function call.
wasm_address_t SV_AllocateWASMMemory(uint32_t size);
void SV_FreeWASMMemory(wasm_address_t addr);
void *SV_ResolveWASMAddress(wasm_address_t address);
wasm_address_t SV_UnresolveWASMAddress(void *pointer);
#endif

#endif // SERVER_H
