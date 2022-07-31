/*
Copyright (C) 2022 Andrey Nazarov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "shared/shared.h"
#include "common/cvar.h"
#include "common/common.h"
#include "common/zone.h"
#include "client/client.h"
#include "client/keys.h"
#include "client/video.h"
#include "client/ui.h"
#include "refresh/refresh.h"
#include "system/system.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>

#include <GL/glx.h>

#include <poll.h>

static struct {
    Display     *dpy;
    int         screen;
    Window      win;
    Window      root;
    Cursor      empty_cursor;
    vidFlags_t  flags;
    vrect_t     rc;
    bool        mapped;

    struct {
        Atom    delete;
        Atom    wm_state;
        Atom    fs;
    } atom;

    struct {
        bool    initialized;
        bool    grabbed;
        bool    grab_pending;
        int     xi_opcode;
        double  x, y;
    } mouse;

    GLXContext                  ctx;
    unsigned                    extensions;
    PFNGLXSWAPINTERVALEXTPROC   SwapIntervalEXT;
} x11;

enum {
    QGLX_ARB_create_context     = (1 << 0),
    QGLX_ARB_multisample        = (1 << 1),
    QGLX_EXT_swap_control       = (1 << 2),
    QGLX_EXT_swap_control_tear  = (1 << 3),
};

static unsigned glx_parse_extension_string(const char *s)
{
    static const char *const extnames[] = {
        "GLX_ARB_create_context",
        "GLX_ARB_multisample",
        "GLX_EXT_swap_control",
        "GLX_EXT_swap_control_tear",
        NULL
    };

    return Com_ParseExtensionString(s, extnames);
}

static void *get_proc_addr(const char *sym)
{
    return glXGetProcAddressARB((const GLubyte *)sym);
}

static void swap_buffers(void)
{
    glXSwapBuffers(x11.dpy, x11.win);
}

static void swap_interval(int val)
{
    if (val < 0 && !(x11.extensions & GLX_EXT_swap_control_tear)) {
        Com_Printf("Negative swap interval is not supported on this system.\n");
        val = -val;
    }

    if (x11.SwapIntervalEXT)
        x11.SwapIntervalEXT(x11.dpy, x11.win, val);
}

static void shutdown(void)
{
    if (x11.dpy) {
        glXMakeCurrent(x11.dpy, None, NULL);

        if (x11.ctx)
            glXDestroyContext(x11.dpy, x11.ctx);

        if (x11.mouse.grabbed)
            XUngrabPointer(x11.dpy, CurrentTime);

        if (x11.win)
            XDestroyWindow(x11.dpy, x11.win);

        if (x11.empty_cursor)
            XFreeCursor(x11.dpy, x11.empty_cursor);

        XCloseDisplay(x11.dpy);
    }

    memset(&x11, 0, sizeof(x11));
}

static Atom *get_prop_atom_list(Window win, Atom atom, int *nitems_p)
{
    Atom type;
    int format, result;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char *data;

    *nitems_p = 0;
    result = XGetWindowProperty(x11.dpy, win, atom, 0, 1024, False, XA_ATOM,
                                &type, &format, &nitems, &bytes_left, &data);
    if (result != Success)
        return NULL;

    if (type != XA_ATOM || format != 32 || bytes_left) {
        XFree(data);
        return NULL;
    }

    *nitems_p = nitems;
    return (Atom *)data;
}

static int io_error_handler(Display *dpy)
{
    Com_Error(ERR_FATAL, "X input/output error");
}

static int error_handler(Display *dpy, XErrorEvent *event)
{
    char buffer[MAX_STRING_CHARS];

    XGetErrorText(dpy, event->error_code, buffer, sizeof(buffer));
    Com_EPrintf("X request (major %u, minor %u) failed: %s\n",
                event->request_code, event->minor_code, buffer);
    return 0;
}

static bool choose_fb_config(r_opengl_config_t *cfg, GLXFBConfig *fbc)
{
    int glx_attr[] = {
        GLX_X_RENDERABLE, True,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 5,
        GLX_GREEN_SIZE, 5,
        GLX_BLUE_SIZE, 5,
        GLX_DOUBLEBUFFER, True,
        GLX_DEPTH_SIZE, cfg->depthbits,
        GLX_STENCIL_SIZE, cfg->stencilbits,
        GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, cfg->multisamples,
        None
    };

    if (!cfg->multisamples)
        glx_attr[16] = None;

    int num_configs;
    GLXFBConfig *configs = glXChooseFBConfig(x11.dpy, x11.screen, glx_attr, &num_configs);
    if (!configs) {
        Com_EPrintf("Failed to choose FB config\n");
        return false;
    }

    if (num_configs == 0) {
        Com_EPrintf("glXChooseFBConfig returned 0 configs\n");
        XFree(configs);
        return false;
    }

    *fbc = configs[0];
    XFree(configs);
    return true;
}

static bool init(void)
{
    if (!(x11.dpy = XOpenDisplay(NULL))) {
        Com_EPrintf("Failed to open X display\n");
        return false;
    }

    XSetIOErrorHandler(io_error_handler);
    XSetErrorHandler(error_handler);

    x11.screen = DefaultScreen(x11.dpy);
    x11.root = RootWindow(x11.dpy, x11.screen);

    int glx_major, glx_minor;
    if (!glXQueryVersion(x11.dpy, &glx_major, &glx_minor)) {
        Com_EPrintf("GLX not available\n");
        goto fail;
    }
    if (glx_major == 1 && glx_minor < 3) {
        Com_EPrintf("At least GLX 1.3 required\n");
        goto fail;
    }

    x11.extensions = glx_parse_extension_string(glXQueryExtensionsString(x11.dpy, x11.screen));

    r_opengl_config_t *cfg = R_GetGLConfig();

    if (cfg->multisamples) {
        if (x11.extensions & QGLX_ARB_multisample) {
            Com_Printf("Enabling GLX_ARB_multisample\n");
        } else {
            Com_WPrintf("GLX_ARB_multisample not found\n");
            cfg->multisamples = 0;
        }
    }

    GLXFBConfig fbc;
    if (!choose_fb_config(cfg, &fbc)) {
        Com_Printf("Falling back to failsafe config\n");
        r_opengl_config_t failsafe = { .depthbits = 24 };
        if (!choose_fb_config(&failsafe, &fbc))
            goto fail;
    }

    XVisualInfo *visinfo = glXGetVisualFromFBConfig(x11.dpy, fbc);
    if (!visinfo) {
        Com_EPrintf("Failed to get visual\n");
        goto fail;
    }

    unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
    XSetWindowAttributes attr = {
        .background_pixel = BlackPixel(x11.dpy, x11.screen),
        .border_pixel = 0,
        .colormap = XCreateColormap(x11.dpy, x11.root, visinfo->visual, AllocNone),
        .event_mask = StructureNotifyMask | FocusChangeMask | KeyPressMask | KeyReleaseMask |
            ButtonPressMask | ButtonReleaseMask | PointerMotionMask | PropertyChangeMask,
    };

    VID_GetGeometry(&x11.rc);

    x11.win = XCreateWindow(x11.dpy, x11.root, x11.rc.x, x11.rc.y, x11.rc.width, x11.rc.height,
                            0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);
    XFree(visinfo);
    if (!x11.win) {
        Com_EPrintf("Failed to create window\n");
        goto fail;
    }

    XSizeHints hints = {
        .flags = PMinSize,
        .min_width = 320,
        .min_height = 240,
    };

    XSetWMNormalHints(x11.dpy, x11.win, &hints);

    XWMHints wm_hints = {
        .flags = InputHint | StateHint,
        .input = True,
        .initial_state = NormalState,
    };

    XSetWMHints(x11.dpy, x11.win, &wm_hints);

    XStoreName(x11.dpy, x11.win, PRODUCT);
    XSetIconName(x11.dpy, x11.win, PRODUCT);

#define XA(x)   XInternAtom(x11.dpy, #x, False)

    x11.atom.delete = XA(WM_DELETE_WINDOW);
    XSetWMProtocols(x11.dpy, x11.win, &x11.atom.delete, 1);

    x11.atom.wm_state = XA(_NET_WM_STATE);

    int nitems;
    Atom *list = get_prop_atom_list(x11.root, XA(_NET_SUPPORTED), &nitems);
    if (list) {
        Atom fs = XA(_NET_WM_STATE_FULLSCREEN);
        for (int i = 0; i < nitems; i++) {
            if (list[i] == fs) {
                x11.atom.fs = fs;
                break;
            }
        }
        XFree(list);
    }

    if (cfg->debug) {
        PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = NULL;

        if (x11.extensions & QGLX_ARB_create_context)
            glXCreateContextAttribsARB = get_proc_addr("glXCreateContextAttribsARB");

        if (glXCreateContextAttribsARB) {
            Com_Printf("Enabling GLX_ARB_create_context\n");

            int ctx_attr[] = {
                GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
                None
            };

            if (!(x11.ctx = glXCreateContextAttribsARB(x11.dpy, fbc, NULL, True, ctx_attr)))
                Com_EPrintf("Failed to create debug GL context\n");
        } else {
            Com_WPrintf("GLX_ARB_create_context not found\n");
        }
    }
    if (!x11.ctx && !(x11.ctx = glXCreateNewContext(x11.dpy, fbc, GLX_RGBA_TYPE, NULL, True))) {
        Com_EPrintf("Failed to create GL context\n");
        goto fail;
    }

    if (!glXMakeCurrent(x11.dpy, x11.win, x11.ctx)) {
        Com_EPrintf("Failed to make GL context current\n");
        goto fail;
    }

    if (x11.extensions & QGLX_EXT_swap_control)
        x11.SwapIntervalEXT = get_proc_addr("glXSwapIntervalEXT");

    if (x11.SwapIntervalEXT) {
        if (x11.extensions & QGLX_EXT_swap_control_tear)
            Com_Printf("Enabling GLX_EXT_swap_control_tear\n");
        else
            Com_Printf("Enabling GLX_EXT_swap_control\n");
    } else {
        Com_WPrintf("GLX_EXT_swap_control not found\n");
    }

    char data = 0;
    Pixmap pixmap = XCreateBitmapFromData(x11.dpy, x11.win, &data, 1, 1);
    if (pixmap) {
        XColor color = { 0 };
        x11.empty_cursor = XCreatePixmapCursor(x11.dpy, pixmap, pixmap, &color, &color, 0, 0);
        XFreePixmap(x11.dpy, pixmap);
    }

    Bool set;
    XkbSetDetectableAutoRepeat(x11.dpy, True, &set);
    return true;

fail:
    shutdown();
    return false;
}

static void set_fullscreen(bool fs)
{
    if (!x11.atom.fs)
        return;

    XEvent event = {
        .xclient = {
            .type = ClientMessage,
            .send_event = True,
            .message_type = x11.atom.wm_state,
            .window = x11.win,
            .format = 32,
            .data = { .l = { fs, x11.atom.fs, 0, 1 } }
        }
    };
    XSendEvent(x11.dpy, x11.root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

static void set_mode(void)
{
    if (!x11.mapped) {
        if (vid_fullscreen->integer && x11.atom.fs) {
            XChangeProperty(x11.dpy, x11.win, x11.atom.wm_state, XA_ATOM, 32,
                            PropModeAppend, (unsigned char *)&x11.atom.fs, 1);
        }
        XMapWindow(x11.dpy, x11.win);
        x11.mapped = true;
    } else if (vid_fullscreen->integer) {
        set_fullscreen(true);
    } else if (x11.flags & QVF_FULLSCREEN) {
        set_fullscreen(false);
    } else {
        VID_GetGeometry(&x11.rc);
        XMoveResizeWindow(x11.dpy, x11.win, x11.rc.x, x11.rc.y, x11.rc.width, x11.rc.height);
    }
}

static char *get_mode_list(void)
{
    return Z_CopyString("desktop");
}

static void mode_changed(void)
{
    R_ModeChanged(x11.rc.width, x11.rc.height, x11.flags);
    SCR_ModeChanged();
}

static void configure_event(XConfigureEvent *event)
{
    x11.rc.x = event->x;
    x11.rc.y = event->y;
    x11.rc.width = event->width;
    x11.rc.height = event->height;
    if (!(x11.flags & QVF_FULLSCREEN))
        VID_SetGeometry(&x11.rc);
    mode_changed();
}

static const struct {
    KeySym sym;
    int key;
} keytable[] = {
    { XK_BackSpace, K_BACKSPACE },
    { XK_Tab,       K_TAB       },
    { XK_Return,    K_ENTER     },
    { XK_Pause,     K_PAUSE     },
    { XK_Escape,    K_ESCAPE    },

    { XK_KP_0,        K_KP_INS        },
    { XK_KP_1,        K_KP_END        },
    { XK_KP_2,        K_KP_DOWNARROW  },
    { XK_KP_3,        K_KP_PGDN       },
    { XK_KP_4,        K_KP_LEFTARROW  },
    { XK_KP_5,        K_KP_5          },
    { XK_KP_6,        K_KP_RIGHTARROW },
    { XK_KP_7,        K_KP_HOME       },
    { XK_KP_8,        K_KP_UPARROW    },
    { XK_KP_9,        K_KP_PGUP       },
    { XK_KP_Delete,   K_KP_DEL        },
    { XK_KP_Divide,   K_KP_SLASH      },
    { XK_KP_Multiply, K_KP_MULTIPLY   },
    { XK_KP_Subtract, K_KP_MINUS      },
    { XK_KP_Add,      K_KP_PLUS       },
    { XK_KP_Enter,    K_KP_ENTER      },

    { XK_Up,        K_UPARROW    },
    { XK_Down,      K_DOWNARROW  },
    { XK_Right,     K_RIGHTARROW },
    { XK_Left,      K_LEFTARROW  },
    { XK_Insert,    K_INS        },
    { XK_Home,      K_HOME       },
    { XK_End,       K_END        },
    { XK_Page_Up,   K_PGUP       },
    { XK_Page_Down, K_PGDN       },

    { XK_F1,  K_F1  },
    { XK_F2,  K_F2  },
    { XK_F3,  K_F3  },
    { XK_F4,  K_F4  },
    { XK_F5,  K_F5  },
    { XK_F6,  K_F6  },
    { XK_F7,  K_F7  },
    { XK_F8,  K_F8  },
    { XK_F9,  K_F9  },
    { XK_F10, K_F10 },
    { XK_F11, K_F11 },
    { XK_F12, K_F12 },

    { XK_Num_Lock,    K_NUMLOCK   },
    { XK_Caps_Lock,   K_CAPSLOCK  },
    { XK_Scroll_Lock, K_SCROLLOCK },
    { XK_Super_L,     K_LWINKEY   },
    { XK_Super_R,     K_RWINKEY   },
    { XK_Menu,        K_MENU      },

    { XK_Shift_L,   K_LSHIFT },
    { XK_Shift_R,   K_RSHIFT },
    { XK_Control_L, K_LCTRL  },
    { XK_Control_R, K_RCTRL  },
    { XK_Alt_L,     K_LALT   },
    { XK_Alt_R,     K_RALT   },
};

static int lookup_key(KeySym sym)
{
    for (int i = 0; i < q_countof(keytable); i++)
        if (keytable[i].sym == sym)
            return keytable[i].key;
    return 0;
}

static void key_event(XKeyEvent *event)
{
    KeySym sym = XLookupKeysym(event, 0);
    if (sym == NoSymbol) {
        Com_DPrintf("Unknown keycode %d\n", event->keycode);
        return;
    }

    int key;
    if (Q_isprint(sym)) {
        key = sym;
    } else if (!(key = lookup_key(sym))) {
        Com_DPrintf("Unknown keysym %#lx\n", sym);
        return;
    }

    Key_Event2(key, event->type == KeyPress, com_eventTime);
}

static void button_event(XButtonEvent *event)
{
    static const uint8_t buttontab[] = {
        K_MOUSE1, K_MOUSE3, K_MOUSE2, K_MWHEELUP, K_MWHEELDOWN,
        K_MWHEELLEFT, K_MWHEELRIGHT, K_MOUSE4, K_MOUSE5,
        K_MOUSE6, K_MOUSE7, K_MOUSE8
    };

    if (event->button < 1 || event->button > q_countof(buttontab)) {
        Com_DPrintf("Unknown button %d\n", event->button);
        return;
    }

    int key = buttontab[event->button - 1];
    Key_Event(key, event->type == ButtonPress, com_eventTime);
}

static void message_event(XClientMessageEvent *event)
{
    if (event->format == 32 && event->data.l[0] == x11.atom.delete)
        Com_Quit(NULL, ERR_DISCONNECT);
}

static void property_event(XPropertyEvent *event)
{
    Atom *state;
    int nitems;

    Com_DDPrintf("%s\n", XGetAtomName(x11.dpy, event->atom));
    if (event->atom != x11.atom.wm_state)
        return;
    if (!(state = get_prop_atom_list(x11.win, event->atom, &nitems)))
        return;

    bool fs = false;
    for (int i = 0; i < nitems; i++) {
        if (state[i] == x11.atom.fs)
            fs = true;
        Com_DDPrintf("  %s\n", XGetAtomName(x11.dpy, state[i]));
    }
    XFree(state);

    bool was_fs = x11.flags & QVF_FULLSCREEN;
    if (fs)
        x11.flags |= QVF_FULLSCREEN;
    else
        x11.flags &= ~QVF_FULLSCREEN;

    if (was_fs != fs)
        mode_changed();

    if ((bool)vid_fullscreen->integer != fs)
        Cvar_SetInteger(vid_fullscreen, fs, FROM_CODE);
}

static void raw_motion_event(XIRawEvent *event)
{
    double *v = event->raw_values;
    if (XIMaskIsSet(event->valuators.mask, 0))
        x11.mouse.x += *v++;
    if (XIMaskIsSet(event->valuators.mask, 1))
        x11.mouse.y += *v++;
}

static void generic_event(XGenericEventCookie *event)
{
    if (event->extension != x11.mouse.xi_opcode)
        return;
    if (!XGetEventData(x11.dpy, event))
        return;
    switch (event->evtype) {
    case XI_RawMotion:
        raw_motion_event(event->data);
        break;
    }
    XFreeEventData(x11.dpy, event);
}

static int grab_pointer(void)
{
    return XGrabPointer(x11.dpy, x11.win, True, 0, GrabModeAsync, GrabModeAsync,
                        x11.win, x11.empty_cursor, CurrentTime);
}

static void focus_event(XFocusChangeEvent *event)
{
    if (event->type == FocusOut) {
        CL_Activate(ACT_RESTORED);
        return;
    }

    CL_Activate(ACT_ACTIVATED);

    if (event->mode != NotifyUngrab)
        return;
    if (!x11.mouse.grab_pending)
        return;

    int ret = grab_pointer();
    if (ret != GrabSuccess) {
        Com_EPrintf("Mouse grab failed with error %d\n", ret);
        return;
    }

    x11.mouse.x = 0;
    x11.mouse.y = 0;
    x11.mouse.grabbed = true;
    x11.mouse.grab_pending = false;
}

#if USE_DEBUG
static const char *const eventtab[LASTEvent] = {
    "<error>", "<reply>", "KeyPress", "KeyRelease", "ButtonPress",
    "ButtonRelease", "MotionNotify", "EnterNotify", "LeaveNotify", "FocusIn",
    "FocusOut", "KeymapNotify", "Expose", "GraphicsExpose", "NoExpose",
    "VisibilityNotify", "CreateNotify", "DestroyNotify", "UnmapNotify",
    "MapNotify", "MapRequest", "ReparentNotify", "ConfigureNotify",
    "ConfigureRequest", "GravityNotify", "ResizeRequest", "CirculateNotify",
    "CirculateRequest", "PropertyNotify", "SelectionClear", "SelectionRequest",
    "SelectionNotify", "ColormapNotify", "ClientMessage", "MappingNotify",
    "GenericEvent"
};
#endif

static void pump_events(void)
{
    XEvent event;

    while (XPending(x11.dpy)) {
        XNextEvent(x11.dpy, &event);
        Com_DDDPrintf("%s\n", eventtab[event.type]);
        switch (event.type) {
        case GenericEvent:
            generic_event(&event.xcookie);
            break;
        case ConfigureNotify:
            configure_event(&event.xconfigure);
            break;
        case KeyPress:
        case KeyRelease:
            key_event(&event.xkey);
            break;
        case ButtonPress:
        case ButtonRelease:
            button_event(&event.xbutton);
            break;
        case MotionNotify:
            UI_MouseEvent(event.xmotion.x, event.xmotion.y);
            break;
        case UnmapNotify:
            CL_Activate(ACT_MINIMIZED);
            break;
        case MapNotify:
            CL_Activate(ACT_RESTORED);
            break;
        case FocusIn:
        case FocusOut:
            focus_event(&event.xfocus);
            break;
        case PropertyNotify:
            property_event(&event.xproperty);
            break;
        case ClientMessage:
            message_event(&event.xclient);
            break;
        case DestroyNotify:
            Com_Quit(NULL, ERR_DISCONNECT);
            break;
        }
    }
}

static bool init_mouse(void)
{
    int event, error;
    if (!XQueryExtension(x11.dpy, "XInputExtension", &x11.mouse.xi_opcode, &event, &error)) {
        Com_EPrintf("XInputExtension not available\n");
        return false;
    }

    int major = 2;
    int minor = 0;
    if (XIQueryVersion(x11.dpy, &major, &minor) != Success) {
        Com_EPrintf("XInput2 not available\n");
        return false;
    }

    uint8_t mask[4] = { 0 };
    XIEventMask eventmask = {
        .deviceid = XIAllMasterDevices,
        .mask_len = sizeof(mask),
        .mask = mask
    };
    XISetMask(mask, XI_RawMotion);
    XISelectEvents(x11.dpy, x11.root, &eventmask, 1);

    Com_Printf("XInput2 mouse initialized\n");
    x11.mouse.initialized = true;
    return true;
}

static void shutdown_mouse(void)
{
    if (x11.mouse.grabbed)
        XUngrabPointer(x11.dpy, CurrentTime);

    memset(&x11.mouse, 0, sizeof(x11.mouse));
}

static void grab_mouse(bool grab)
{
    if (!x11.mouse.initialized)
        return;

    x11.mouse.x = 0;
    x11.mouse.y = 0;
    x11.mouse.grab_pending = false;
    if (x11.mouse.grabbed == grab)
        return;

    if (grab) {
        if (grab_pointer() != GrabSuccess) {
            x11.mouse.grab_pending = true;  // wait for other client grab to be released
            return;
        }
    } else {
        XUngrabPointer(x11.dpy, CurrentTime);
    }

    x11.mouse.grabbed = grab;
}

static void warp_mouse(int x, int y)
{
    XWarpPointer(x11.dpy, None, x11.win, 0, 0, 0, 0, x, y);
}

static bool get_mouse_motion(int *dx, int *dy)
{
    if (!x11.mouse.grabbed)
        return false;
    *dx = x11.mouse.x;
    *dy = x11.mouse.y;
    x11.mouse.x = 0;
    x11.mouse.y = 0;
    return true;
}

static char *get_selection_data(void)
{
    Atom type;
    int format, result;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char *data;

    Window sowner = XGetSelectionOwner(x11.dpy, XA_PRIMARY);
    if (sowner == None)
        return NULL;

    Atom property = XA(GETCLIPBOARDDATA_PROP);

    XConvertSelection(x11.dpy, XA_PRIMARY, XA_STRING, property, x11.win, CurrentTime);

    unsigned now = Sys_Milliseconds();
    unsigned deadline = now + 50;
    while (1) {
        XEvent event;
        if (XCheckTypedWindowEvent(x11.dpy, x11.win, SelectionNotify, &event))
            break;

        if (now > deadline)
            return NULL;

        struct pollfd fd = {
            .fd = ConnectionNumber(x11.dpy),
            .events = POLLIN,
        };
        if (poll(&fd, 1, deadline - now) <= 0)
            return NULL;

        now = Sys_Milliseconds();
    }

    result = XGetWindowProperty(x11.dpy, x11.win, property, 0, 1024, True,
                                AnyPropertyType, &type, &format, &nitems, &bytes_left, &data);
    if (result != Success)
        return NULL;

    char *copy = Z_CopyString((char *)data);
    XFree(data);
    return copy;
}

static bool probe(void)
{
    Display *dpy = XOpenDisplay(NULL);

    if (dpy) {
        XCloseDisplay(dpy);
        return true;
    }

    return false;
}

const vid_driver_t vid_x11 = {
    .name = "x11",

    .probe = probe,
    .init = init,
    .shutdown = shutdown,
    .fatal_shutdown = shutdown,
    .pump_events = pump_events,

    .set_mode = set_mode,
    .get_mode_list = get_mode_list,

    .get_proc_addr = get_proc_addr,
    .swap_buffers = swap_buffers,
    .swap_interval = swap_interval,

    .get_selection_data = get_selection_data,

    .init_mouse = init_mouse,
    .shutdown_mouse = shutdown_mouse,
    .grab_mouse = grab_mouse,
    .warp_mouse = warp_mouse,
    .get_mouse_motion = get_mouse_motion,
};
