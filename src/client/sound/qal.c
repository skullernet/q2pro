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

#include "shared/shared.h"
#include "system/system.h"
#include "common/cvar.h"
#include "common/common.h"
#include "common/files.h"

#ifdef __APPLE__
#include <OpenAL/alc.h>
#else
#include <AL/alc.h>
#endif

#define QALAPI
#include "qal.h"

static LPALCCLOSEDEVICE qalcCloseDevice;
static LPALCCREATECONTEXT qalcCreateContext;
static LPALCDESTROYCONTEXT qalcDestroyContext;
static LPALCISEXTENSIONPRESENT qalcIsExtensionPresent;
static LPALCMAKECONTEXTCURRENT qalcMakeContextCurrent;
static LPALCOPENDEVICE qalcOpenDevice;

typedef struct {
    const char *name;
    void *dest;
} alfunction_t;

typedef struct {
    const char *extension;
    const alfunction_t *functions;
} alsection_t;

#define QALC_FN(x)  { "alc"#x, &qalc##x }
#define QAL_FN(x)   { "al"#x, &qal##x }

static const alsection_t sections[] = {
    {
        .functions = (const alfunction_t []) {
            QALC_FN(CloseDevice),
            QALC_FN(CreateContext),
            QALC_FN(DestroyContext),
            QALC_FN(IsExtensionPresent),
            QALC_FN(MakeContextCurrent),
            QALC_FN(OpenDevice),
            QAL_FN(BufferData),
            QAL_FN(Bufferiv),
            QAL_FN(DeleteBuffers),
            QAL_FN(DeleteSources),
            QAL_FN(Disable),
            QAL_FN(DistanceModel),
            QAL_FN(Enable),
            QAL_FN(GenBuffers),
            QAL_FN(GenSources),
            QAL_FN(GetEnumValue),
            QAL_FN(GetError),
            QAL_FN(GetProcAddress),
            QAL_FN(GetSourcef),
            QAL_FN(GetSourcei),
            QAL_FN(GetString),
            QAL_FN(IsExtensionPresent),
            QAL_FN(Listener3f),
            QAL_FN(Listenerf),
            QAL_FN(Listenerfv),
            QAL_FN(Source3f),
            QAL_FN(SourcePlay),
            QAL_FN(SourceQueueBuffers),
            QAL_FN(SourceStop),
            QAL_FN(SourceUnqueueBuffers),
            QAL_FN(Sourcef),
            QAL_FN(Sourcei),
            { NULL }
        }
    },
    {
        .extension = "ALC_EXT_EFX",
        .functions = (const alfunction_t []) {
            QAL_FN(DeleteFilters),
            QAL_FN(Filterf),
            QAL_FN(Filteri),
            QAL_FN(GenFilters),
            { NULL }
        }
    },
};

static cvar_t   *al_driver;
static cvar_t   *al_device;

static void *handle;
static ALCdevice *device;
static ALCcontext *context;

void QAL_Shutdown(void)
{
    if (context) {
        qalcMakeContextCurrent(NULL);
        qalcDestroyContext(context);
        context = NULL;
    }
    if (device) {
        qalcCloseDevice(device);
        device = NULL;
    }

    for (int i = 0; i < q_countof(sections); i++) {
        const alsection_t *sec = &sections[i];
        const alfunction_t *func;

        for (func = sec->functions; func->name; func++)
            *(void **)func->dest = NULL;
    }

    if (handle) {
        Sys_FreeLibrary(handle);
        handle = NULL;
    }

    if (al_driver)
        al_driver->flags &= ~CVAR_SOUND;
    if (al_device)
        al_device->flags &= ~CVAR_SOUND;
}

bool QAL_Init(void)
{
    const alsection_t *sec;
    const alfunction_t *func;
    int i;

    al_driver = Cvar_Get("al_driver", LIBAL, 0);
    al_device = Cvar_Get("al_device", "", 0);

    // don't allow absolute or relative paths
    FS_SanitizeFilenameVariable(al_driver);

    Sys_LoadLibrary(al_driver->string, NULL, &handle);
    if (!handle)
        return false;

    for (i = 0, sec = sections; i < q_countof(sections); i++, sec++) {
        if (sec->extension)
            continue;

        for (func = sec->functions; func->name; func++) {
            void *addr = Sys_GetProcAddress(handle, func->name);
            if (!addr)
                goto fail;
            *(void **)func->dest = addr;
        }
    }

    device = qalcOpenDevice(al_device->string[0] ? al_device->string : NULL);
    if (!device) {
        Com_SetLastError(va("alcOpenDevice(%s) failed", al_device->string));
        goto fail;
    }

    context = qalcCreateContext(device, NULL);
    if (!context) {
        Com_SetLastError("alcCreateContext failed");
        goto fail;
    }

    if (!qalcMakeContextCurrent(context)) {
        Com_SetLastError("alcMakeContextCurrent failed");
        goto fail;
    }

    for (i = 0, sec = sections; i < q_countof(sections); i++, sec++) {
        if (!sec->extension)
            continue;
        if (!qalcIsExtensionPresent(device, sec->extension))
            continue;

        for (func = sec->functions; func->name; func++) {
            void *addr = qalGetProcAddress(func->name);
            if (!addr)
                break;
            *(void **)func->dest = addr;
        }

        if (func->name) {
            for (func = sec->functions; func->name; func++)
                *(void **)func->dest = NULL;

            Com_EPrintf("Couldn't load extension %s\n", sec->extension);
            continue;
        }

        Com_Printf("Loaded extension %s\n", sec->extension);
    }

    al_driver->flags |= CVAR_SOUND;
    al_device->flags |= CVAR_SOUND;

    return true;

fail:
    QAL_Shutdown();
    return false;
}
