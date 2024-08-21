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

#include "client.h"
#include <EGL/egl.h>

static struct {
    void        *handle;
    EGLDisplay  dpy;
    EGLSurface  surf;
    EGLContext  ctx;
} egl;

#include "egl_import.h"

static void egl_shutdown(void)
{
    if (egl.dpy) {
        qeglMakeCurrent(egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (egl.ctx)
            qeglDestroyContext(egl.dpy, egl.ctx);

        if (egl.surf)
            qeglDestroySurface(egl.dpy, egl.surf);

        qeglTerminate(egl.dpy);
    }

    egl_shutdown_import();
    Win_Shutdown();

    memset(&egl, 0, sizeof(egl));
}

static void print_error(const char *what)
{
    Com_EPrintf("%s failed with error %#x\n", what, qeglGetError());
}

static bool choose_config(r_opengl_config_t cfg, EGLConfig *config)
{
    EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_DEPTH_SIZE, cfg.depthbits,
        EGL_STENCIL_SIZE, cfg.stencilbits,
        EGL_SAMPLE_BUFFERS, (bool)cfg.multisamples,
        EGL_SAMPLES, cfg.multisamples,
        EGL_NONE
    };

    EGLint num_configs;
    if (!qeglChooseConfig(egl.dpy, cfg_attr, config, 1, &num_configs)) {
        print_error("eglChooseConfig");
        return false;
    }

    if (num_configs == 0) {
        Com_EPrintf("eglChooseConfig returned 0 configs\n");
        return false;
    }

    return true;
}

static bool egl_init(void)
{
    if (!egl_init_import()) {
        Com_EPrintf("egl_init_import failed: %s\n", Com_GetLastError());
        return false;
    }

    // create the main window
    Win_Init();

    egl.dpy = qeglGetDisplay(win.dc);
    if (!egl.dpy) {
        Com_EPrintf("eglGetDisplay failed\n");
        goto fail;
    }

    EGLint egl_major, egl_minor;
    if (!qeglInitialize(egl.dpy, &egl_major, &egl_minor)) {
        print_error("eglInitialize");
        goto fail;
    }
    if (egl_major == 1 && egl_minor < 5) {
        Com_EPrintf("At least EGL 1.5 required\n");
        goto fail;
    }

    Com_DPrintf("EGL_VENDOR: %s\n", qeglQueryString(egl.dpy, EGL_VENDOR));
    Com_DPrintf("EGL_VERSION: %s\n", qeglQueryString(egl.dpy, EGL_VERSION));
    Com_DPrintf("EGL_CLIENT_APIS: %s\n", qeglQueryString(egl.dpy, EGL_CLIENT_APIS));

    if (!qeglBindAPI(EGL_OPENGL_ES_API)) {
        print_error("eglBindAPI");
        goto fail;
    }

    r_opengl_config_t cfg = R_GetGLConfig();

    EGLConfig config;
    if (!choose_config(cfg, &config)) {
        Com_Printf("Falling back to failsafe config\n");
        r_opengl_config_t failsafe = { .depthbits = 24 };
        if (!choose_config(failsafe, &config))
            goto fail;
    }

    egl.surf = qeglCreateWindowSurface(egl.dpy, config, win.wnd, NULL);
    if (!egl.surf) {
        print_error("eglCreateWindowSurface");
        goto fail;
    }

    EGLint ctx_attr[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_DEBUG, cfg.debug,
        EGL_NONE
    };
    egl.ctx = qeglCreateContext(egl.dpy, config, EGL_NO_CONTEXT, ctx_attr);
    if (!egl.ctx) {
        print_error("eglCreateContext");
        goto fail;
    }

    if (!qeglMakeCurrent(egl.dpy, egl.surf, egl.surf, egl.ctx)) {
        print_error("eglMakeCurrent");
        goto fail;
    }

    return true;

fail:
    egl_shutdown();
    return false;
}

static void *egl_get_proc_addr(const char *sym)
{
    return qeglGetProcAddress(sym);
}

static void egl_swap_buffers(void)
{
    qeglSwapBuffers(egl.dpy, egl.surf);
}

static void egl_swap_interval(int val)
{
    if (!qeglSwapInterval(egl.dpy, val))
        print_error("eglSwapInterval");
}

static bool egl_probe(void)
{
    return os_access("libEGL.dll", X_OK) == 0;
}

const vid_driver_t vid_win32egl = {
    .name = "win32egl",

    .probe = egl_probe,
    .init = egl_init,
    .shutdown = egl_shutdown,
    .pump_events = Win_PumpEvents,

    .get_mode_list = Win_GetModeList,
    .get_dpi_scale = Win_GetDpiScale,
    .set_mode = Win_SetMode,
    .update_gamma = Win_UpdateGamma,

    .get_proc_addr = egl_get_proc_addr,
    .swap_buffers = egl_swap_buffers,
    .swap_interval = egl_swap_interval,

    .get_clipboard_data = Win_GetClipboardData,
    .set_clipboard_data = Win_SetClipboardData,

    .init_mouse = Win_InitMouse,
    .shutdown_mouse = Win_ShutdownMouse,
    .grab_mouse = Win_GrabMouse,
    .warp_mouse = Win_WarpMouse,
    .get_mouse_motion = Win_GetMouseMotion,
};
