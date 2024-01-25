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
#include "common/cmd.h"
#include "common/common.h"
#include "common/zone.h"
#include "client/client.h"
#include "client/keys.h"
#include "client/video.h"
#include "client/ui.h"
#include "refresh/refresh.h"
#include "system/system.h"
#include "keytables/keytables.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include <libdecor.h>

#include "relative-pointer-unstable-v1.h"
#include "pointer-constraints-unstable-v1.h"
#include "primary-selection-unstable-v1.h"

#include <linux/input-event-codes.h>

#include <EGL/egl.h>

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

struct output {
    struct wl_list link;
    struct wl_list surf;
    uint32_t id;
    int scale;
    struct wl_output *wl_output;
};

static struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct libdecor *libdecor;
    struct libdecor_frame *frame;
    struct wl_egl_window *egl_window;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct wl_data_offer *data_offer;
    struct wl_data_source *data_source;
    struct zwp_relative_pointer_manager_v1 *rel_pointer_manager;
    struct zwp_relative_pointer_v1 *rel_pointer;
    struct zwp_pointer_constraints_v1 *pointer_constraints;
    struct zwp_locked_pointer_v1 *locked_pointer;
    struct zwp_primary_selection_device_manager_v1 *selection_device_manager;
    struct zwp_primary_selection_device_v1 *selection_device;
    struct zwp_primary_selection_offer_v1 *selection_offer;

    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;

    int scale_factor;
    int width;
    int height;
    vidFlags_t flags;

    bool mouse_grabbed;
    wl_fixed_t rel_mouse_x;
    wl_fixed_t rel_mouse_y;
    wl_fixed_t abs_mouse_x;
    wl_fixed_t abs_mouse_y;

    bool pointer_focus;
    uint32_t pointer_enter_serial;
    int cursor_hotspot_x;
    int cursor_hotspot_y;

    uint32_t keyboard_enter_serial;
    int32_t keyrepeat_delta;
    int32_t keyrepeat_delay;
    int lastkeydown;
    unsigned keydown_time;
    unsigned keyrepeat_time;

    char *clipboard_data;
    const char *data_offer_type;
    const char *selection_offer_type;

    struct wl_list outputs;
    struct wl_list surface_outputs;
} wl;

static const char *proxy_tag = APPLICATION;


/*
===============================================================================

SEAT

===============================================================================
*/

static void set_cursor(void)
{
    if (wl.mouse_grabbed) {
        wl_pointer_set_cursor(wl.pointer, wl.pointer_enter_serial, NULL, 0, 0);
    } else if (wl.cursor_surface) {
        wl_pointer_set_cursor(wl.pointer, wl.pointer_enter_serial, wl.cursor_surface,
                              wl.cursor_hotspot_x, wl.cursor_hotspot_y);
    }
}

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy)
{
    if (surface == wl.surface) {
        wl.pointer_enter_serial = serial;
        wl.pointer_focus = true;
        set_cursor();
        UI_MouseEvent(wl_fixed_to_int(sx * wl.scale_factor),
                      wl_fixed_to_int(sy * wl.scale_factor));
    }
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface)
{
    if (surface == wl.surface)
        wl.pointer_focus = false;
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    if (wl.pointer_focus) {
        UI_MouseEvent(wl_fixed_to_int(sx * wl.scale_factor),
                      wl_fixed_to_int(sy * wl.scale_factor));
    }
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state)
{
    bool down = state == WL_POINTER_BUTTON_STATE_PRESSED;
    int key;

    if (!wl.pointer_focus && down)
        return;

    switch (button) {
    case BTN_LEFT:    key = K_MOUSE1; break;
    case BTN_RIGHT:   key = K_MOUSE2; break;
    case BTN_MIDDLE:  key = K_MOUSE3; break;
    case BTN_SIDE:    key = K_MOUSE4; break;
    case BTN_EXTRA:   key = K_MOUSE5; break;
    case BTN_BACK:    key = K_MOUSE6; break;
    case BTN_FORWARD: key = K_MOUSE7; break;
    default:
        Com_DPrintf("Unknown button %d\n", button);
        return;
    }

    Key_Event(key, down, time);
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value)
{
    if (!wl.pointer_focus)
        return;

    switch (axis) {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        if (value < 0) {
            Key_Event(K_MWHEELUP, true, time);
            Key_Event(K_MWHEELUP, false, time);
        } else if (value > 0) {
            Key_Event(K_MWHEELDOWN, true, time);
            Key_Event(K_MWHEELDOWN, false, time);
        }
        break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        if (value > 0) {
            Key_Event(K_MWHEELRIGHT, true, time);
            Key_Event(K_MWHEELRIGHT, false, time);
        } else if (value < 0) {
            Key_Event(K_MWHEELLEFT, true, time);
            Key_Event(K_MWHEELLEFT, false, time);
        }
        break;
    }
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t format, int32_t fd, uint32_t size)
{
    close(fd);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys)
{
    if (surface == wl.surface) {
        wl.keyboard_enter_serial = serial;
        CL_Activate(ACT_ACTIVATED);
    }
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, struct wl_surface *surface)
{
    if (surface != wl.surface)
        return;

    CL_Activate(ACT_RESTORED);

    if (wl.data_offer) {
        wl_data_offer_destroy(wl.data_offer);
        wl.data_offer = NULL;
    }

    if (wl.selection_offer) {
        zwp_primary_selection_offer_v1_destroy(wl.selection_offer);
        wl.selection_offer = NULL;
    }

    wl.lastkeydown = 0;
}

static void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
                                uint32_t serial, uint32_t time, uint32_t ev_key,
                                uint32_t state)
{
    bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
    int key = 0;

    if (ev_key < keytable_evdev.count)
        key = keytable_evdev.keys[ev_key];

    if (!key) {
        Com_DPrintf("Unknown key %d\n", ev_key);
        return;
    }

    Key_Event2(key, down, time);

    if (down) {
        wl.lastkeydown = key;
        wl.keydown_time = com_eventTime;
    } else if (key == wl.lastkeydown) {
        wl.lastkeydown = 0;
    }
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_confined,
                                      uint32_t group)
{
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                        int32_t rate, int32_t delay)
{
    wl.keyrepeat_delta = rate ? 1000 / rate : 0;
    wl.keyrepeat_delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
    keyboard_handle_repeat_info,
};

static void seat_handle_caps(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl.pointer) {
        wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(wl.pointer, &pointer_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl.pointer) {
        wl_pointer_destroy(wl.pointer);
        wl.pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl.keyboard) {
        wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(wl.keyboard, &keyboard_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl.keyboard) {
        wl_keyboard_destroy(wl.keyboard);
        wl.keyboard = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_caps,
};


/*
===============================================================================

SURFACE

===============================================================================
*/

static void reload_cursor(void)
{
    if (!wl.shm)
        return;

    const char *size_str = getenv("XCURSOR_SIZE");
    int size = 24;
    if (size_str) {
        char *end;
        long s = strtol(size_str, &end, 10);
        if (s > 0 && s <= INT_MAX / wl.scale_factor && !*end)
            size = s;
    }

    if (wl.cursor_theme)
        wl_cursor_theme_destroy(wl.cursor_theme);

    const char *theme = getenv("XCURSOR_THEME");
    if (!(wl.cursor_theme = wl_cursor_theme_load(theme, size * wl.scale_factor, wl.shm)))
        return;

    struct wl_cursor *cursor = wl_cursor_theme_get_cursor(wl.cursor_theme, "left_ptr");
    if (!cursor)
        return;

    struct wl_cursor_image *image = cursor->images[0];
    wl.cursor_hotspot_x = image->hotspot_x / wl.scale_factor;
    wl.cursor_hotspot_y = image->hotspot_y / wl.scale_factor;

    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    if (!buffer)
        return;

    wl_surface_set_buffer_scale(wl.cursor_surface, wl.scale_factor);
    wl_surface_attach(wl.cursor_surface, buffer, 0, 0);
    wl_surface_damage(wl.cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(wl.cursor_surface);
}

static void mode_changed(void)
{
    int width = wl.width * wl.scale_factor;
    int height = wl.height * wl.scale_factor;

    if (wl.egl_window)
        wl_egl_window_resize(wl.egl_window, width, height, 0, 0);

    R_ModeChanged(width, height, wl.flags);
    SCR_ModeChanged();
}

static void update_scale(void)
{
    struct output *output;
    int scale = 1;

    wl_list_for_each(output, &wl.surface_outputs, surf)
        scale = max(scale, output->scale);

    wl_surface_set_buffer_scale(wl.surface, scale);
    wl_surface_commit(wl.surface);
    if (wl.scale_factor != scale) {
        wl.scale_factor = scale;
        reload_cursor();
        mode_changed();
    }
}

static void surface_handle_enter(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *wl_output)
{
    if (wl_proxy_get_tag((struct wl_proxy *)wl_output) != &proxy_tag)
        return;

    struct output *output = wl_output_get_user_data(wl_output);
    if (!output)
        return;
    if (!wl_list_empty(&output->surf))
        return;

    wl_list_insert(&wl.surface_outputs, &output->surf);
    update_scale();
}

static void surface_handle_leave(void *data, struct wl_surface *wl_surface,
                                 struct wl_output *wl_output)
{
    if (wl_proxy_get_tag((struct wl_proxy *)wl_output) != &proxy_tag)
        return;

    struct output *output = wl_output_get_user_data(wl_output);
    if (!output)
        return;
    if (wl_list_empty(&output->surf))
        return;

    wl_list_remove(&output->surf);
    wl_list_init(&output->surf);
    update_scale();
}

static const struct wl_surface_listener surface_listener = {
    surface_handle_enter,
    surface_handle_leave,
};


/*
===============================================================================

FRAME

===============================================================================
*/

static void get_default_geometry(int *width, int *height)
{
    vrect_t rc;
    VID_GetGeometry(&rc);
    *width = rc.width;
    *height = rc.height;
}

static void frame_configure(struct libdecor_frame *frame,
                            struct libdecor_configuration *configuration,
                            void *user_data)
{
    int width, height;

    if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height))
        get_default_geometry(&width, &height);

    wl.width = width;
    wl.height = height;

    enum libdecor_window_state window_state;
    if (libdecor_configuration_get_window_state(configuration, &window_state)) {
        if (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN)
            wl.flags |= QVF_FULLSCREEN;
        else
            wl.flags &= ~QVF_FULLSCREEN;
    }

    struct libdecor_state *state = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    if (libdecor_frame_is_floating(wl.frame))
        Cvar_SetByVar(vid_geometry, va("%dx%d", width, height), FROM_CODE);

    mode_changed();
}

static void frame_close(struct libdecor_frame *frame, void *user_data)
{
    Cbuf_AddText(&cmd_buffer, "quit\n");
}

static void frame_commit(struct libdecor_frame *frame, void *user_data)
{
    wl_surface_commit(wl.surface);
}

static struct libdecor_frame_interface frame_interface = {
    frame_configure,
    frame_close,
    frame_commit,
};

static void libdecor_error(struct libdecor *context, enum libdecor_error error, const char *message)
{
    Com_EPrintf("libdecor: %s\n", message);
}

static struct libdecor_interface libdecor_interface = {
    libdecor_error,
};


/*
===============================================================================

OUTPUTS

===============================================================================
*/

static void output_handle_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y,
                                   int32_t physical_width, int32_t physical_height, int32_t subpixel,
                                   const char *make, const char *model, int32_t transform)
{
}

static void output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                               int32_t width, int32_t height, int32_t refresh)
{
}

static void output_handle_done(void *data, struct wl_output *wl_output)
{
    struct output *output = data;
    if (!wl_list_empty(&output->surf))
        update_scale();
}

static void output_handle_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
    struct output *output = data;
    if (factor > 0)
        output->scale = factor;
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
    output_handle_done,
    output_handle_scale,
};


/*
===============================================================================

REGISTRY

===============================================================================
*/

static void registry_global(void *data, struct wl_registry *wl_registry,
                            uint32_t name, const char *interface, uint32_t version)
{
    if (!strcmp(interface, wl_compositor_interface.name)) {
        wl.compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 3);
        return;
    }

    if (!strcmp(interface, wl_seat_interface.name)) {
        wl.seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(wl.seat, &seat_listener, NULL);
        return;
    }

    if (!strcmp(interface, wl_shm_interface.name)) {
        wl.shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
        return;
    }

    if (!strcmp(interface, wl_output_interface.name)) {
        struct output *output = Z_Malloc(sizeof(*output));
        output->id = name;
        output->scale = 1;
        output->wl_output = wl_registry_bind(wl_registry, name, &wl_output_interface, 2);
        wl_list_init(&output->surf);
        wl_proxy_set_tag((struct wl_proxy *)output->wl_output, &proxy_tag);
        wl_output_add_listener(output->wl_output, &output_listener, output);
        wl_list_insert(&wl.outputs, &output->link);
        return;
    }

    if (!strcmp(interface, wl_data_device_manager_interface.name)) {
        wl.data_device_manager = wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, 1);
        return;
    }

    if (!strcmp(interface, zwp_relative_pointer_manager_v1_interface.name)) {
        wl.rel_pointer_manager = wl_registry_bind(wl_registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
        return;
    }

    if (!strcmp(interface, zwp_pointer_constraints_v1_interface.name)) {
        wl.pointer_constraints = wl_registry_bind(wl_registry, name, &zwp_pointer_constraints_v1_interface, 1);
        return;
    }

    if (!strcmp(interface, zwp_primary_selection_device_manager_v1_interface.name)) {
        wl.selection_device_manager = wl_registry_bind(wl_registry, name, &zwp_primary_selection_device_manager_v1_interface, 1);
        return;
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
    struct output *output;

    wl_list_for_each(output, &wl.outputs, link) {
        if (output->id == name) {
            wl_list_remove(&output->link);
            wl_list_remove(&output->surf);
            wl_output_destroy(output->wl_output);
            Z_Free(output);
            break;
        }
    }
}

static const struct wl_registry_listener wl_registry_listener = {
    registry_global,
    registry_global_remove,
};


/*
===============================================================================

INIT / SHUTDOWN

===============================================================================
*/

#define CHECK(cond, stmt)   if (!(cond)) { stmt; goto fail; }
#define CHECK_ERR(cond, what) CHECK(cond, Com_EPrintf("%s failed: %s\n", what, strerror(errno)))
#define CHECK_UNK(cond, what) CHECK(cond, Com_EPrintf("%s failed\n", what))
#define CHECK_EGL(cond, what) CHECK(cond, egl_error(what))

#define DESTROY(x, f)   if (x) f(x)

static void init_clipboard(void);

static void shutdown(void)
{
    struct output *output, *next;
    wl_list_for_each_safe(output, next, &wl.outputs, link) {
        wl_output_destroy(output->wl_output);
        Z_Free(output);
    }

    if (wl.egl_display)
        eglMakeCurrent(wl.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (wl.egl_context)
        eglDestroyContext(wl.egl_display, wl.egl_context);

    if (wl.egl_surface)
        eglDestroySurface(wl.egl_display, wl.egl_surface);

    DESTROY(wl.egl_window, wl_egl_window_destroy);
    DESTROY(wl.egl_display, eglTerminate);
    DESTROY(wl.frame, libdecor_frame_unref);
    DESTROY(wl.libdecor, libdecor_unref);
    DESTROY(wl.rel_pointer, zwp_relative_pointer_v1_destroy);
    DESTROY(wl.rel_pointer_manager, zwp_relative_pointer_manager_v1_destroy);
    DESTROY(wl.locked_pointer, zwp_locked_pointer_v1_destroy);
    DESTROY(wl.pointer_constraints, zwp_pointer_constraints_v1_destroy);
    DESTROY(wl.surface, wl_surface_destroy);
    DESTROY(wl.cursor_surface, wl_surface_destroy);
    DESTROY(wl.compositor, wl_compositor_destroy);
    DESTROY(wl.cursor_theme, wl_cursor_theme_destroy);
    DESTROY(wl.shm, wl_shm_destroy);
    DESTROY(wl.data_source, wl_data_source_destroy);
    DESTROY(wl.data_offer, wl_data_offer_destroy);
    DESTROY(wl.data_device, wl_data_device_destroy);
    DESTROY(wl.data_device_manager, wl_data_device_manager_destroy);
    DESTROY(wl.selection_offer, zwp_primary_selection_offer_v1_destroy);
    DESTROY(wl.selection_device, zwp_primary_selection_device_v1_destroy);
    DESTROY(wl.selection_device_manager, zwp_primary_selection_device_manager_v1_destroy);
    DESTROY(wl.pointer, wl_pointer_destroy);
    DESTROY(wl.keyboard, wl_keyboard_destroy);
    DESTROY(wl.seat, wl_seat_destroy);
    DESTROY(wl.registry, wl_registry_destroy);
    DESTROY(wl.display, wl_display_disconnect);

    Z_Free(wl.clipboard_data);

    memset(&wl, 0, sizeof(wl));
}

static void egl_error(const char *what)
{
    Com_EPrintf("%s failed with error %#x\n", what, eglGetError());
}

static bool choose_config(r_opengl_config_t *cfg, EGLConfig *config)
{
    EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_DEPTH_SIZE, cfg->depthbits,
        EGL_STENCIL_SIZE, cfg->stencilbits,
        EGL_SAMPLE_BUFFERS, (bool)cfg->multisamples,
        EGL_SAMPLES, cfg->multisamples,
        EGL_NONE
    };

    EGLint num_configs;
    if (!eglChooseConfig(wl.egl_display, cfg_attr, config, 1, &num_configs)) {
        egl_error("eglChooseConfig");
        return false;
    }

    if (num_configs == 0) {
        Com_EPrintf("eglChooseConfig returned 0 configs\n");
        return false;
    }

    return true;
}

static bool init(void)
{
    wl_list_init(&wl.outputs);
    wl_list_init(&wl.surface_outputs);

    wl.scale_factor = 1;
    get_default_geometry(&wl.width, &wl.height);

    wl.keyrepeat_delta = 30;
    wl.keyrepeat_delay = 600;

    CHECK_ERR(wl.display = wl_display_connect(NULL), "wl_display_connect");
    CHECK_ERR(wl.registry = wl_display_get_registry(wl.display), "wl_display_get_registry");
    wl_registry_add_listener(wl.registry, &wl_registry_listener, NULL);
    wl_display_roundtrip(wl.display);
    wl_display_roundtrip(wl.display);

    if (!wl.compositor || !wl.seat) {
        Com_EPrintf("Required wayland interfaces missing\n");
        goto fail;
    }

    CHECK_UNK(wl.egl_display = eglGetDisplay(wl.display), "eglGetDisplay");

    EGLint egl_major, egl_minor;
    CHECK_EGL(eglInitialize(wl.egl_display, &egl_major, &egl_minor), "eglInitialize");
    if (egl_major == 1 && egl_minor < 4) {
        Com_EPrintf("At least EGL 1.4 required\n");
        goto fail;
    }

    CHECK_EGL(eglBindAPI(EGL_OPENGL_API), "eglBindAPI");

    r_opengl_config_t *cfg = R_GetGLConfig();

    EGLConfig config;
    if (!choose_config(cfg, &config)) {
        Com_Printf("Falling back to failsafe config\n");
        r_opengl_config_t failsafe = { .depthbits = 24 };
        if (!choose_config(&failsafe, &config))
            goto fail;
    }

    struct output *output;
    wl_list_for_each(output, &wl.outputs, link)
        wl.scale_factor = max(wl.scale_factor, output->scale);

    CHECK_ERR(wl.surface = wl_compositor_create_surface(wl.compositor), "wl_compositor_create_surface");
    wl_surface_add_listener(wl.surface, &surface_listener, NULL);
    wl_surface_set_buffer_scale(wl.surface, wl.scale_factor);

    CHECK_UNK(wl.libdecor = libdecor_new(wl.display, &libdecor_interface), "libdecor_new");
    CHECK_UNK(wl.frame = libdecor_decorate(wl.libdecor, wl.surface, &frame_interface, NULL), "libdecor_decorate");
    libdecor_frame_set_title(wl.frame, PRODUCT);
    libdecor_frame_set_app_id(wl.frame, APPLICATION);
    libdecor_frame_set_min_content_size(wl.frame, 320, 240);

    CHECK_ERR(wl.cursor_surface = wl_compositor_create_surface(wl.compositor), "wl_compositor_create_surface");
    reload_cursor();

    EGLint ctx_attr[] = {
        EGL_CONTEXT_OPENGL_DEBUG, cfg->debug,
        EGL_NONE
    };
    if (egl_major == 1 && egl_minor < 5)
        ctx_attr[0] = EGL_NONE;

    CHECK_ERR(wl.egl_window = wl_egl_window_create(wl.surface, wl.width * wl.scale_factor, wl.height * wl.scale_factor), "wl_egl_window_create");
    CHECK_EGL(wl.egl_surface = eglCreateWindowSurface(wl.egl_display, config, wl.egl_window, NULL), "eglCreateWindowSurface");
    CHECK_EGL(wl.egl_context = eglCreateContext(wl.egl_display, config, EGL_NO_CONTEXT, ctx_attr), "eglCreateContext");
    CHECK_EGL(eglMakeCurrent(wl.egl_display, wl.egl_surface, wl.egl_surface, wl.egl_context), "eglMakeCurrent");
    CHECK_EGL(eglSwapInterval(wl.egl_display, 0), "eglSwapInterval");

    init_clipboard();

    libdecor_frame_map(wl.frame);
    wl_display_roundtrip(wl.display);

    CL_Activate(ACT_RESTORED);
    return true;

fail:
    shutdown();
    return false;
}

static void set_mode(void)
{
    if (vid_fullscreen->integer)
        libdecor_frame_set_fullscreen(wl.frame, NULL);
    else
        libdecor_frame_unset_fullscreen(wl.frame);
}

static char *get_mode_list(void)
{
    return Z_CopyString("desktop");
}

static int get_dpi_scale(void)
{
    return wl.scale_factor;
}

static void pump_events(void)
{
    libdecor_dispatch(wl.libdecor, 0);

    if (wl.lastkeydown && wl.keyrepeat_delta
        && com_eventTime - wl.keydown_time   > wl.keyrepeat_delay
        && com_eventTime - wl.keyrepeat_time > wl.keyrepeat_delta)
    {
        Key_Event2(wl.lastkeydown, true, com_eventTime);
        wl.keyrepeat_time = com_eventTime;
    }
}

static void *get_proc_addr(const char *sym)
{
    return eglGetProcAddress(sym);
}

static void swap_buffers(void)
{
    eglSwapBuffers(wl.egl_display, wl.egl_surface);
}


/*
===============================================================================

RELATIVE MOUSE

===============================================================================
*/

// input-event-codes.h defines this
#undef KEY_MENU

static void handle_relative_motion(void *data, struct zwp_relative_pointer_v1 *zwp_relative_pointer_v1,
                                   uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy,
                                   wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
    if (Key_GetDest() & KEY_MENU) {
        wl.abs_mouse_x = Q_clip(wl.abs_mouse_x + dx, 0, wl_fixed_from_int(wl.width * wl.scale_factor) - 1);
        wl.abs_mouse_y = Q_clip(wl.abs_mouse_y + dy, 0, wl_fixed_from_int(wl.height * wl.scale_factor) - 1);
        UI_MouseEvent(wl_fixed_to_int(wl.abs_mouse_x), wl_fixed_to_int(wl.abs_mouse_y));
    }

    wl.rel_mouse_x += dx_unaccel;
    wl.rel_mouse_y += dy_unaccel;
}

static const struct zwp_relative_pointer_v1_listener rel_pointer_listener = {
    handle_relative_motion,
};

static bool init_mouse(void)
{
    return true;
}

static void shutdown_mouse(void)
{
}

static void grab_mouse(bool grab)
{
    wl.rel_mouse_x = 0;
    wl.rel_mouse_y = 0;
    if (wl.mouse_grabbed == grab)
        return;

    if (grab) {
        if (!wl.pointer || !wl.rel_pointer_manager || !wl.pointer_constraints)
            return;
        wl.locked_pointer = zwp_pointer_constraints_v1_lock_pointer(
            wl.pointer_constraints, wl.surface, wl.pointer, NULL,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
        wl.rel_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(wl.rel_pointer_manager, wl.pointer);
        if (wl.rel_pointer)
            zwp_relative_pointer_v1_add_listener(wl.rel_pointer, &rel_pointer_listener, NULL);
    } else {
        if (wl.rel_pointer) {
            zwp_relative_pointer_v1_destroy(wl.rel_pointer);
            wl.rel_pointer = NULL;
        }
        if (wl.locked_pointer) {
            zwp_locked_pointer_v1_destroy(wl.locked_pointer);
            wl.locked_pointer = NULL;
        }
    }

    wl.mouse_grabbed = grab;
    set_cursor();
}

static void warp_mouse(int x, int y)
{
    wl.abs_mouse_x = wl_fixed_from_int(x);
    wl.abs_mouse_y = wl_fixed_from_int(y);
}

static bool get_mouse_motion(int *dx, int *dy)
{
    if (!wl.mouse_grabbed)
        return false;
    *dx = wl_fixed_to_int(wl.rel_mouse_x);
    *dy = wl_fixed_to_int(wl.rel_mouse_y);
    wl.rel_mouse_x = 0;
    wl.rel_mouse_y = 0;
    return true;
}


/*
===============================================================================

CLIPBOARD

===============================================================================
*/

static const char *const text_types[] = {
    "text/plain",
    "text/plain;charset=utf-8",
    "TEXT",
    "STRING",
    "UTF8_STRING",
};

static const char *find_mime_type(const char *mime_type)
{
    for (int i = 0; i < q_countof(text_types); i++)
        if (!strcmp(mime_type, text_types[i]))
            return text_types[i];
    return NULL;
}

static void data_offer_handle_offer(void *data, struct wl_data_offer *offer, const char *mime_type)
{
    wl.data_offer_type = find_mime_type(mime_type);
}

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = data_offer_handle_offer,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *data_device,
                                          struct wl_data_offer *offer)
{
    wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
}

static void data_device_handle_selection(void *data, struct wl_data_device *data_device,
                                         struct wl_data_offer *offer)
{
    if (wl.data_offer)
        wl_data_offer_destroy(wl.data_offer);
    wl.data_offer = offer;
}

static const struct wl_data_device_listener data_device_listener = {
    .data_offer = data_device_handle_data_offer,
    .selection = data_device_handle_selection,
};

static void selection_offer_handle_offer(void *data, struct zwp_primary_selection_offer_v1 *offer,
                                         const char *mime_type)
{
    wl.selection_offer_type = find_mime_type(mime_type);
}

static const struct zwp_primary_selection_offer_v1_listener selection_offer_listener = {
    .offer = selection_offer_handle_offer,
};

static void selection_device_handle_data_offer(void *data,
                                               struct zwp_primary_selection_device_v1 *device,
                                               struct zwp_primary_selection_offer_v1 *offer)
{
    zwp_primary_selection_offer_v1_add_listener(offer, &selection_offer_listener, NULL);
}

static void selection_device_handle_selection(void *data,
                                              struct zwp_primary_selection_device_v1 *device,
                                              struct zwp_primary_selection_offer_v1 *offer)
{
    if (wl.selection_offer)
        zwp_primary_selection_offer_v1_destroy(wl.selection_offer);
    wl.selection_offer = offer;
}

static const struct zwp_primary_selection_device_v1_listener selection_device_listener = {
    .data_offer = selection_device_handle_data_offer,
    .selection = selection_device_handle_selection,
};

static void init_clipboard(void)
{
    if (wl.data_device_manager) {
        wl.data_device = wl_data_device_manager_get_data_device(wl.data_device_manager, wl.seat);
        if (wl.data_device)
            wl_data_device_add_listener(wl.data_device, &data_device_listener, NULL);
    }

    if (wl.selection_device_manager) {
        wl.selection_device = zwp_primary_selection_device_manager_v1_get_device(wl.selection_device_manager, wl.seat);
        if (wl.selection_device)
            zwp_primary_selection_device_v1_add_listener(wl.selection_device, &selection_device_listener, NULL);
    }
}

static char *get_selection(int fd)
{
    wl_display_roundtrip(wl.display);

    if (!Sys_SetNonBlock(fd, true)) {
        close(fd);
        return NULL;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    if (poll(&pfd, 1, 50) < 1 || !(pfd.revents & POLLIN)) {
        close(fd);
        return NULL;
    }

    char buf[MAX_STRING_CHARS];
    int r = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (r < 1)
        return NULL;

    buf[r] = 0;
    return Z_CopyString(buf);
}

static char *get_selection_data(void)
{
    if (!wl.selection_offer || !wl.selection_offer_type)
        return NULL;

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0)
        return NULL;

    zwp_primary_selection_offer_v1_receive(wl.selection_offer, wl.selection_offer_type, fds[1]);
    close(fds[1]);

    return get_selection(fds[0]);
}

static char *get_clipboard_data(void)
{
    if (!wl.data_offer || !wl.data_offer_type)
        return NULL;

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0)
        return NULL;

    wl_data_offer_receive(wl.data_offer, wl.data_offer_type, fds[1]);
    close(fds[1]);

    return get_selection(fds[0]);
}

static void handle_data_source_send(void *data, struct wl_data_source *source, const char *mime_type, int fd)
{
    if (wl.clipboard_data && find_mime_type(mime_type) && Sys_SetNonBlock(fd, true))
        if (write(fd, wl.clipboard_data, strlen(wl.clipboard_data)) < 0)
            (void)"why should I care?";
    close(fd);
}

static void handle_data_source_cancelled(void *data, struct wl_data_source *source)
{
    if (wl.data_source) {
        wl_data_source_destroy(wl.data_source);
        wl.data_source = NULL;
    }

    Z_Freep(&wl.clipboard_data);
}

static const struct wl_data_source_listener data_source_listener = {
    .send = handle_data_source_send,
    .cancelled = handle_data_source_cancelled,
};

static void set_clipboard_data(const char *data)
{
    if (!data || !*data)
        return;
    if (!wl.data_device_manager || !wl.data_device)
        return;

    Z_Free(wl.clipboard_data);
    wl.clipboard_data = Z_CopyString(data);

    if (!wl.data_source) {
        wl.data_source = wl_data_device_manager_create_data_source(wl.data_device_manager);
        wl_data_source_add_listener(wl.data_source, &data_source_listener, NULL);
        for (int i = 0; i < q_countof(text_types); i++)
            wl_data_source_offer(wl.data_source, text_types[i]);
    }

    wl_data_device_set_selection(wl.data_device, wl.data_source, wl.keyboard_enter_serial);
}


/*
===============================================================================

ENTRY POINT

===============================================================================
*/

static bool probe(void)
{
    struct wl_display *dpy = wl_display_connect(NULL);

    if (dpy) {
        wl_display_disconnect(dpy);
        return true;
    }

    return false;
}

const vid_driver_t vid_wayland = {
    .name = "wayland",

    .probe = probe,
    .init = init,
    .shutdown = shutdown,
    .fatal_shutdown = shutdown,
    .pump_events = pump_events,

    .set_mode = set_mode,
    .get_mode_list = get_mode_list,
    .get_dpi_scale = get_dpi_scale,

    .get_proc_addr = get_proc_addr,
    .swap_buffers = swap_buffers,

    .get_selection_data = get_selection_data,
    .get_clipboard_data = get_clipboard_data,
    .set_clipboard_data = set_clipboard_data,

    .init_mouse = init_mouse,
    .shutdown_mouse = shutdown_mouse,
    .grab_mouse = grab_mouse,
    .warp_mouse = warp_mouse,
    .get_mouse_motion = get_mouse_motion,
};
