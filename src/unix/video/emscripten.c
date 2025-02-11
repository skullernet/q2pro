/*
Copyright (C) 2025 Andrey Nazarov

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
#include "common/common.h"
#include "common/zone.h"
#include "client/client.h"
#include "client/input.h"
#include "client/keys.h"
#include "client/ui.h"
#include "client/video.h"
#include "refresh/refresh.h"
#include "system/system.h"

#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>

#define TARGET  "canvas"

static struct {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
    bool need_resize;
    struct {
        bool initialized;
        bool grabbed;
        bool pointerlocked;
        int x;
        int y;
    } mouse;
} htm;

static char *get_mode_list(void)
{
    return Z_CopyString("fullscreen");
}

static int get_dpi_scale(void)
{
    return Q_rint(emscripten_get_device_pixel_ratio());
}

static void mode_changed(void)
{
    EmscriptenFullscreenChangeEvent status;
    emscripten_get_fullscreen_status(&status);

    int w, h;
    emscripten_webgl_get_drawing_buffer_size(htm.context, &w, &h);

    Cvar_SetInteger(vid_fullscreen, status.isFullscreen, FROM_CODE);
    R_ModeChanged(w, h, status.isFullscreen ? QVF_FULLSCREEN : 0);
    SCR_ModeChanged();
}

static bool canvas_resized_cb(int type, const void *reserved, void *data)
{
    mode_changed();
    return true;
}

static bool window_resized_cb(int type, const EmscriptenUiEvent *event, void *data)
{
    double w, h, scale = emscripten_get_device_pixel_ratio();
    emscripten_get_element_css_size(TARGET, &w, &h);
    emscripten_set_canvas_element_size(TARGET, w * scale, h * scale);

    mode_changed();
    return false;
}

static void set_mode(void)
{
    if (vid_fullscreen->integer) {
        EmscriptenFullscreenStrategy strategy = {
            .scaleMode                 = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH,
            .canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF,
            .filteringMode             = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT,
            .canvasResizedCallback     = canvas_resized_cb,
        };

        if (emscripten_request_fullscreen_strategy(TARGET, false, &strategy) >= 0)
            return;

        Cvar_SetInteger(vid_fullscreen, 0, FROM_CODE);
    }

    emscripten_exit_fullscreen();

    if (htm.need_resize) {
        window_resized_cb(0, NULL, NULL);
        htm.need_resize = false;
    }
}

static bool fullscreen_cb(int type, const EmscriptenFullscreenChangeEvent *event, void *data)
{
    if (!event->isFullscreen)
        htm.need_resize = true;
    return false;
}

static const struct {
    const char *name;
    int key;
} key_table[] = {
    #include "keytables/html5.h"
};

static bool keydown_cb(int type, const EmscriptenKeyboardEvent *event, void *data)
{
    int key = 0;

    for (int i = 0; i < q_countof(key_table); i++) {
        if (!strcmp(key_table[i].name, event->code)) {
            key = key_table[i].key;
            break;
        }
    }

    if (!key) {
        Com_DPrintf("%s: unknown keycode %s\n", __func__, event->code);
        return false;
    }

    // browsers usually override Escape key, use Alt+F1 to simulate
    if (key == K_F1 && event->altKey)
        key = K_ESCAPE;

    Key_Event2(key, type == EMSCRIPTEN_EVENT_KEYDOWN, event->timestamp);
    return true;
}

static bool mousedown_cb(int type, const EmscriptenMouseEvent *event, void *data)
{
    static const byte keys[] = { K_MOUSE1, K_MOUSE3, K_MOUSE2 };

    if (event->button >= q_countof(keys))
        return false;

    if (htm.mouse.grabbed && !htm.mouse.pointerlocked)
        emscripten_request_pointerlock(TARGET, false);

    Key_Event(keys[event->button], type == EMSCRIPTEN_EVENT_MOUSEDOWN, event->timestamp);
    return true;
}

static bool mousemove_cb(int type, const EmscriptenMouseEvent *event, void *data)
{
    float scale = emscripten_get_device_pixel_ratio();
    UI_MouseEvent(event->targetX * scale, event->targetY * scale);
    htm.mouse.x += event->movementX;
    htm.mouse.y += event->movementY;
    return true;
}

static bool mousewheel_cb(int type, const EmscriptenWheelEvent *event, void *data)
{
    unsigned time = Sys_Milliseconds();

    if (event->deltaX > 0) {
        Key_Event(K_MWHEELRIGHT, true, time);
        Key_Event(K_MWHEELRIGHT, false, time);
    } else if (event->deltaX < 0) {
        Key_Event(K_MWHEELLEFT, true, time);
        Key_Event(K_MWHEELLEFT, false, time);
    }

    if (event->deltaY < 0) {
        Key_Event(K_MWHEELUP, true, time);
        Key_Event(K_MWHEELUP, false, time);
    } else if (event->deltaY > 0) {
        Key_Event(K_MWHEELDOWN, true, time);
        Key_Event(K_MWHEELDOWN, false, time);
    }

    return true;
}

static bool pointerlock_change_cb(int type, const EmscriptenPointerlockChangeEvent *event, void *data)
{
    htm.mouse.pointerlocked = event->isActive;
    htm.mouse.x = 0;
    htm.mouse.y = 0;
    return false;
}

static void shutdown(void)
{
    if (htm.context)
        emscripten_webgl_destroy_context(htm.context);

    emscripten_html5_remove_all_event_listeners();

    memset(&htm, 0, sizeof(htm));
}

static bool init(void)
{
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = false;
    attr.stencil = true;
    attr.antialias = false;
    attr.majorVersion = 2;
    attr.minorVersion = 0;
    htm.context = emscripten_webgl_create_context(TARGET, &attr);
    if (!htm.context) {
        Com_EPrintf("Couldn't create OpenGL context\n");
        return false;
    }

    if (emscripten_webgl_make_context_current(htm.context) < 0) {
        Com_EPrintf("Couldn't make OpenGL context current\n");
        shutdown();
        return false;
    }

    emscripten_set_keydown_callback          (EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, keydown_cb);
    emscripten_set_keyup_callback            (EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, keydown_cb);
    emscripten_set_resize_callback           (EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, window_resized_cb);
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, pointerlock_change_cb);
    emscripten_set_mousedown_callback        (TARGET, NULL, true, mousedown_cb);
    emscripten_set_mouseup_callback          (TARGET, NULL, true, mousedown_cb);
    emscripten_set_mousemove_callback        (TARGET, NULL, true, mousemove_cb);
    emscripten_set_wheel_callback            (TARGET, NULL, true, mousewheel_cb);
    emscripten_set_fullscreenchange_callback (TARGET, NULL, true, fullscreen_cb);

    CL_Activate(ACT_ACTIVATED);
    htm.need_resize = true;

    return true;
}

static void pump_events(void)
{
    if (htm.need_resize) {
        window_resized_cb(0, NULL, NULL);
        htm.need_resize = false;
    }
}

static void swap_buffers(void)
{
}

static void swap_interval(int val)
{
}

static bool get_mouse_motion(int *dx, int *dy)
{
    if (!htm.mouse.grabbed || !htm.mouse.pointerlocked)
        return false;

    *dx = htm.mouse.x;
    *dy = htm.mouse.y;
    htm.mouse.x = 0;
    htm.mouse.y = 0;
    return true;
}

static void shutdown_mouse(void)
{
    if (htm.mouse.pointerlocked)
        emscripten_exit_pointerlock();

    memset(&htm.mouse, 0, sizeof(htm.mouse));
}

static bool init_mouse(void)
{
    htm.mouse.initialized = true;
    return true;
}

static void grab_mouse(bool grab)
{
    if (!htm.mouse.initialized)
        return;

    if (grab)
        emscripten_request_pointerlock(TARGET, false);
    else
        emscripten_exit_pointerlock();

    htm.mouse.grabbed = grab;
}

static bool probe(void)
{
    return true;
}

const vid_driver_t vid_emscripten = {
    .name = "emscripten",

    .probe = probe,
    .init = init,
    .shutdown = shutdown,
    .pump_events = pump_events,

    .get_mode_list = get_mode_list,
    .get_dpi_scale = get_dpi_scale,
    .set_mode = set_mode,

    .get_proc_addr = emscripten_webgl_get_proc_address,
    .swap_buffers = swap_buffers,
    .swap_interval = swap_interval,

    .init_mouse = init_mouse,
    .shutdown_mouse = shutdown_mouse,
    .grab_mouse = grab_mouse,
    .get_mouse_motion = get_mouse_motion,
};
