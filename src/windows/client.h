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

//
// client.h -- Win32 client stuff
//

#include "shared/shared.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#if USE_CLIENT
#include "client/client.h"
#include "client/input.h"
#include "client/keys.h"
#include "client/ui.h"
#include "client/video.h"
#include "refresh/refresh.h"
#endif
#include "system/system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if USE_CLIENT

#define WINDOW_CLASS_NAME   "Quake 2 Pro"

#define IDI_APP 100

#define MOUSE_BUTTONS   5

// supported in Vista or greater
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL  0x020E
#endif
#ifndef RI_MOUSE_HWHEEL
#define RI_MOUSE_HWHEEL 0x0800
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED   0x02E0
#endif

#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef __LPCGUID_DEFINED__
#define __LPCGUID_DEFINED__
typedef const GUID *LPCGUID;
#endif

// MinGW-w64 doesn't define these...
#ifndef DM_GRAYSCALE
#define DM_GRAYSCALE    1
#endif
#ifndef DM_INTERLACED
#define DM_INTERLACED   2
#endif

typedef struct {
    HWND    wnd;
    HDC     dc;

    DEVMODE  dm;

    DWORD   lastMsgTime;
    HHOOK   kbdHook;

    vidFlags_t flags;

    SHORT   gamma_cust[3][256];
    SHORT   gamma_orig[3][256];

    // x and y specify position of non-client area on the screen
    // width and height specify size of client area
    vrect_t rc;

    // rectangle of client area in screen coordinates
    RECT    screen_rc;

    // center of client area in screen coordinates
    int     center_x, center_y;

    bool    alttab_disabled;

    enum {
        MODE_SIZE       = (1 << 0),
        MODE_POS        = (1 << 1),
        MODE_STYLE      = (1 << 2),
        MODE_REPOSITION = (1 << 3),
    } mode_changed;

    struct {
        bool        initialized;
        bool        grabbed;
        int         mx, my;
    } mouse;

    UINT (WINAPI *GetDpiForWindow)(HWND hwnd);
} win_state_t;

extern win_state_t      win;

void Win_Init(void);
void Win_Shutdown(void);
char *Win_GetModeList(void);
int Win_GetDpiScale(void);
void Win_SetMode(void);
void Win_UpdateGamma(const byte *table);
void Win_PumpEvents(void);
char *Win_GetClipboardData(void);
void Win_SetClipboardData(const char *data);
bool Win_InitMouse(void);
void Win_ShutdownMouse(void);
void Win_GrabMouse(bool grab);
void Win_WarpMouse(int x, int y);
bool Win_GetMouseMotion(int *dx, int *dy);

#endif // USE_CLIENT

extern HINSTANCE                    hGlobalInstance;

#if USE_DBGHELP
void Sys_InstallExceptionFilter(void);
#endif
