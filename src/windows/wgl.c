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

#include "client.h"

#include <GL/gl.h>
#include <GL/wglext.h>

static struct {
    HGLRC       context;        // handle to GL rendering context
    HMODULE     handle;         // handle to GL library
    unsigned    extensions;
    PFNWGLCREATECONTEXTATTRIBSARBPROC   CreateContextAttribsARB;
    PFNWGLCHOOSEPIXELFORMATARBPROC      ChoosePixelFormatARB;
    PFNWGLSWAPINTERVALEXTPROC           SwapIntervalEXT;
} wgl;

static cvar_t   *gl_allow_software;

enum {
    QWGL_ARB_create_context             = BIT(0),
    QWGL_ARB_multisample                = BIT(1),
    QWGL_ARB_pixel_format               = BIT(2),
    QWGL_EXT_create_context_es_profile  = BIT(3),
    QWGL_EXT_swap_control               = BIT(4),
    QWGL_EXT_swap_control_tear          = BIT(5),
};

static unsigned wgl_parse_extension_string(const char *s)
{
    static const char *const extnames[] = {
        "WGL_ARB_create_context",
        "WGL_ARB_multisample",
        "WGL_ARB_pixel_format",
        "WGL_EXT_create_context_es_profile",
        "WGL_EXT_swap_control",
        "WGL_EXT_swap_control_tear",
        NULL
    };

    return Com_ParseExtensionString(s, extnames);
}

static void wgl_shutdown(void)
{
    wglMakeCurrent(NULL, NULL);

    if (wgl.context) {
        wglDeleteContext(wgl.context);
        wgl.context = NULL;
    }

    Win_Shutdown();

    memset(&wgl, 0, sizeof(wgl));
}

static void print_error(const char *what)
{
    Com_EPrintf("%s failed: %s\n", what, Sys_ErrorString(GetLastError()));
}

#define FAIL_OK     0
#define FAIL_SOFT   -1
#define FAIL_HARD   -2

static int wgl_setup_gl(r_opengl_config_t cfg)
{
    PIXELFORMATDESCRIPTOR pfd;
    int pixelformat;

    // create the main window
    Win_Init();

    // choose pixel format
    if (wgl.ChoosePixelFormatARB && cfg.multisamples) {
        int attr[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB, TRUE,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB, cfg.colorbits,
            WGL_DEPTH_BITS_ARB, cfg.depthbits,
            WGL_STENCIL_BITS_ARB, cfg.stencilbits,
            WGL_SAMPLE_BUFFERS_ARB, 1,
            WGL_SAMPLES_ARB, cfg.multisamples,
            0
        };
        UINT num_formats;

        if (!wgl.ChoosePixelFormatARB(win.dc, attr, NULL, 1, &pixelformat, &num_formats)) {
            print_error("wglChoosePixelFormatARB");
            goto soft;
        }
        if (num_formats == 0) {
            Com_EPrintf("No suitable OpenGL pixelformat found for %d multisamples\n", cfg.multisamples);
            goto soft;
        }
    } else {
        pfd = (PIXELFORMATDESCRIPTOR) {
            .nSize = sizeof(pfd),
            .nVersion = 1,
            .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            .iPixelType = PFD_TYPE_RGBA,
            .cColorBits = cfg.colorbits,
            .cDepthBits = cfg.depthbits,
            .cStencilBits = cfg.stencilbits,
            .iLayerType = PFD_MAIN_PLANE,
        };

        if (!(pixelformat = ChoosePixelFormat(win.dc, &pfd))) {
            print_error("ChoosePixelFormat");
            goto soft;
        }
    }

    // set pixel format
    if (!DescribePixelFormat(win.dc, pixelformat, sizeof(pfd), &pfd)) {
        print_error("DescribePixelFormat");
        goto soft;
    }

    if (!SetPixelFormat(win.dc, pixelformat, &pfd)) {
        print_error("SetPixelFormat");
        goto soft;
    }

    // check for software emulation
    if (pfd.dwFlags & PFD_GENERIC_FORMAT) {
        if (!gl_allow_software->integer) {
            Com_EPrintf("No hardware OpenGL acceleration detected\n");
            goto soft;
        }
        Com_WPrintf("Using software emulation\n");
    } else if (pfd.dwFlags & PFD_GENERIC_ACCELERATED) {
        Com_DPrintf("MCD acceleration found\n");
    } else {
        Com_DPrintf("ICD acceleration found\n");
    }

    // startup the OpenGL subsystem by creating a context and making it current
    if (wgl.CreateContextAttribsARB && (cfg.debug || cfg.profile)) {
        int attr[9];
        int i = 0;

        if (cfg.profile) {
            attr[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
            attr[i++] = cfg.major_ver;
            attr[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
            attr[i++] = cfg.minor_ver;
        }
        if (cfg.profile == QGL_PROFILE_ES) {
            attr[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
            attr[i++] = WGL_CONTEXT_ES_PROFILE_BIT_EXT;
        }
        if (cfg.debug) {
            attr[i++] = WGL_CONTEXT_FLAGS_ARB;
            attr[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
        }
        attr[i] = 0;

        if (!(wgl.context = wgl.CreateContextAttribsARB(win.dc, NULL, attr))) {
            print_error("wglCreateContextAttribsARB");
            goto soft;
        }
    } else {
        if (!(wgl.context = wglCreateContext(win.dc))) {
            print_error("wglCreateContext");
            goto hard;
        }
    }

    if (!wglMakeCurrent(win.dc, wgl.context)) {
        print_error("wglMakeCurrent");
        wglDeleteContext(wgl.context);
        wgl.context = NULL;
        goto hard;
    }

    return FAIL_OK;

soft:
    // it failed, clean up
    Win_Shutdown();
    return FAIL_SOFT;

hard:
    Win_Shutdown();
    return FAIL_HARD;
}

#define GPA(x)  (void *)wglGetProcAddress(x)

static unsigned get_fake_window_extensions(void)
{
    static const char class[] = "Q2PRO FAKE WINDOW CLASS";
    static const char name[] = "Q2PRO FAKE WINDOW NAME";
    unsigned extensions = 0;

    WNDCLASSEXA wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = DefWindowProc,
        .hInstance = hGlobalInstance,
        .lpszClassName = class,
    };

    if (!RegisterClassExA(&wc))
        goto fail0;

    HWND wnd = CreateWindowA(class, name, 0, 0, 0, 0, 0,
                             NULL, NULL, hGlobalInstance, NULL);
    if (!wnd)
        goto fail1;

    HDC dc;
    if (!(dc = GetDC(wnd)))
        goto fail2;

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(pfd),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 24,
        .cDepthBits = 24,
        .iLayerType = PFD_MAIN_PLANE,
    };

    int pixelformat;
    if (!(pixelformat = ChoosePixelFormat(dc, &pfd)))
        goto fail3;

    if (!SetPixelFormat(dc, pixelformat, &pfd))
        goto fail3;

    HGLRC rc;
    if (!(rc = wglCreateContext(dc)))
        goto fail3;

    if (!wglMakeCurrent(dc, rc))
        goto fail4;

    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = GPA("wglGetExtensionsStringARB");
    if (!wglGetExtensionsStringARB)
        goto fail5;

    extensions = wgl_parse_extension_string(wglGetExtensionsStringARB(dc));

    if (extensions & QWGL_ARB_create_context)
        wgl.CreateContextAttribsARB = GPA("wglCreateContextAttribsARB");

    if (extensions & QWGL_ARB_pixel_format)
        wgl.ChoosePixelFormatARB = GPA("wglChoosePixelFormatARB");

fail5:
    wglMakeCurrent(NULL, NULL);
fail4:
    wglDeleteContext(rc);
fail3:
    ReleaseDC(wnd, dc);
fail2:
    DestroyWindow(wnd);
fail1:
    UnregisterClassA(class, hGlobalInstance);
fail0:
    return extensions;
}

static bool wgl_init(void)
{
    const char *extensions = NULL;
    unsigned fake_extensions = 0;
    r_opengl_config_t cfg;
    int ret;

    gl_allow_software = Cvar_Get("gl_allow_software", "0", 0);

    wgl.handle = GetModuleHandle("opengl32");
    if (!wgl.handle) {
        print_error("GetModuleHandle");
        return false;
    }

    cfg = R_GetGLConfig();

    // check for extensions by creating a fake window
    if (cfg.multisamples || cfg.debug || cfg.profile)
        fake_extensions = get_fake_window_extensions();

    if (cfg.multisamples) {
        if (fake_extensions & QWGL_ARB_multisample) {
            if (!wgl.ChoosePixelFormatARB) {
                Com_WPrintf("Ignoring WGL_ARB_multisample, WGL_ARB_pixel_format not found\n");
                cfg.multisamples = 0;
            }
        } else {
            Com_WPrintf("WGL_ARB_multisample not found for %d multisamples\n", cfg.multisamples);
            cfg.multisamples = 0;
        }
    }

    if ((cfg.debug || cfg.profile) && !wgl.CreateContextAttribsARB) {
        Com_WPrintf("WGL_ARB_create_context not found\n");
        cfg.debug = false;
        cfg.profile = QGL_PROFILE_NONE;
    }

    if (cfg.profile == QGL_PROFILE_ES && !(fake_extensions & QWGL_EXT_create_context_es_profile)) {
        Com_WPrintf("WGL_EXT_create_context_es_profile not found\n");
        cfg.profile = QGL_PROFILE_NONE;
    }

    // create window, choose PFD, setup OpenGL context
    ret = wgl_setup_gl(cfg);

    // attempt to recover
    if (ret == FAIL_SOFT) {
        Com_Printf("Falling back to failsafe config\n");
        r_opengl_config_t failsafe = {
            .colorbits = 24,
            .depthbits = 24,
        };
        ret = wgl_setup_gl(failsafe);
    }

    if (ret) {
        // it failed, clean up
        memset(&wgl, 0, sizeof(wgl));
        return false;
    }

    // initialize WGL extensions
    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = GPA("wglGetExtensionsStringARB");
    if (wglGetExtensionsStringARB)
        extensions = wglGetExtensionsStringARB(win.dc);

    // fall back to GL_EXTENSIONS for legacy drivers
    if (!extensions || !*extensions)
        extensions = (const char *)glGetString(GL_EXTENSIONS);

    wgl.extensions = wgl_parse_extension_string(extensions);

    if (wgl.extensions & QWGL_EXT_swap_control)
        wgl.SwapIntervalEXT = GPA("wglSwapIntervalEXT");

    if (!wgl.SwapIntervalEXT)
        Com_WPrintf("WGL_EXT_swap_control not found\n");

    return true;
}

static void *wgl_get_proc_addr(const char *sym)
{
    void *entry = wglGetProcAddress(sym);

    if (entry)
        return entry;

    return GetProcAddress(wgl.handle, sym);
}

static void wgl_swap_buffers(void)
{
    SwapBuffers(win.dc);
}

static void wgl_swap_interval(int val)
{
    if (val < 0 && !(wgl.extensions & QWGL_EXT_swap_control_tear)) {
        Com_Printf("Negative swap interval is not supported on this system.\n");
        val = -val;
    }

    if (wgl.SwapIntervalEXT && !wgl.SwapIntervalEXT(val))
        print_error("wglSwapIntervalEXT");
}

static bool wgl_probe(void)
{
    return true;
}

const vid_driver_t vid_win32wgl = {
    .name = "win32wgl",

    .probe = wgl_probe,
    .init = wgl_init,
    .shutdown = wgl_shutdown,
    .pump_events = Win_PumpEvents,

    .get_mode_list = Win_GetModeList,
    .get_dpi_scale = Win_GetDpiScale,
    .set_mode = Win_SetMode,
    .update_gamma = Win_UpdateGamma,

    .get_proc_addr = wgl_get_proc_addr,
    .swap_buffers = wgl_swap_buffers,
    .swap_interval = wgl_swap_interval,

    .get_clipboard_data = Win_GetClipboardData,
    .set_clipboard_data = Win_SetClipboardData,

    .init_mouse = Win_InitMouse,
    .shutdown_mouse = Win_ShutdownMouse,
    .grab_mouse = Win_GrabMouse,
    .warp_mouse = Win_WarpMouse,
    .get_mouse_motion = Win_GetMouseMotion,
};
