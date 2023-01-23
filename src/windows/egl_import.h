/*
Copyright (C) 2022 Andrey Nazarov

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

#define QEGL_IMP \
    QEGL(PFNEGLBINDAPIPROC, eglBindAPI); \
    QEGL(PFNEGLCHOOSECONFIGPROC, eglChooseConfig); \
    QEGL(PFNEGLCREATECONTEXTPROC, eglCreateContext); \
    QEGL(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface); \
    QEGL(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext); \
    QEGL(PFNEGLDESTROYSURFACEPROC, eglDestroySurface); \
    QEGL(PFNEGLGETDISPLAYPROC, eglGetDisplay); \
    QEGL(PFNEGLGETERRORPROC, eglGetError); \
    QEGL(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress); \
    QEGL(PFNEGLINITIALIZEPROC, eglInitialize); \
    QEGL(PFNEGLMAKECURRENTPROC, eglMakeCurrent); \
    QEGL(PFNEGLQUERYSTRINGPROC, eglQueryString); \
    QEGL(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers); \
    QEGL(PFNEGLSWAPINTERVALPROC, eglSwapInterval); \
    QEGL(PFNEGLTERMINATEPROC, eglTerminate);

#define QEGL(type, func)    static type q##func
QEGL_IMP
#undef QEGL

static void egl_shutdown_import(void)
{
#define QEGL(type, func)    q##func = NULL
    QEGL_IMP
#undef QEGL

    if (egl.handle) {
        Sys_FreeLibrary(egl.handle);
        egl.handle = NULL;
    }
}

static bool egl_init_import(void)
{
    Sys_LoadLibrary("libEGL.dll", NULL, &egl.handle);
    if (!egl.handle)
        return false;

#define QEGL(type, func)    if ((q##func = Sys_GetProcAddress(egl.handle, #func)) == NULL) goto fail;
    QEGL_IMP
#undef QEGL

    return true;

fail:
    egl_shutdown_import();
    return false;
}
