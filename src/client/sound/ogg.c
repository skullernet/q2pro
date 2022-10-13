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

#define OV_EXCLUDE_STATIC_CALLBACKS

#include "sound.h"
#include <vorbis/vorbisfile.h>

#ifndef USE_BIG_ENDIAN
#define USE_BIG_ENDIAN 0
#endif

typedef struct {
    bool initialized;
    OggVorbis_File vf;
    qhandle_t f;
    char path[MAX_QPATH];
} ogg_state_t;

static ogg_state_t  ogg;

static cvar_t   *ogg_enable;
static cvar_t   *ogg_volume;
static cvar_t   *ogg_shuffle;

static void     **tracklist;
static int      trackcount;
static int      trackindex;

static size_t my_read(void *buf, size_t size, size_t nmemb, void *stream)
{
    if (!size || !nmemb) {
        errno = 0;
        return 0;
    }

    if (size > INT_MAX / nmemb) {
        errno = EINVAL;
        return 0;
    }

    int ret = FS_Read(buf, size * nmemb, ogg.f);
    if (ret < 0) {
        errno = EIO;
        return 0;
    }

    errno = 0;
    return ret / size;
}

static void ogg_stop(void)
{
    if (ogg.initialized)
        ov_clear(&ogg.vf);

    if (ogg.f)
        FS_FCloseFile(ogg.f);

    memset(&ogg, 0, sizeof(ogg));
}

static void shuffle(void)
{
    for (int i = trackcount - 1; i > 0; i--) {
        int j = Q_rand_uniform(i + 1);
        SWAP(void *, tracklist[i], tracklist[j]);
    }
}

void OGG_Play(void)
{
    ogg_stop();

    if (!s_started)
        return;

    if (!ogg_enable->integer)
        return;

    int track = atoi(cl.configstrings[CS_CDTRACK]);
    if (track < 1)
        return;

    if (ogg_shuffle->integer && trackcount) {
        if (trackindex == 0)
            shuffle();
        Q_strlcpy(ogg.path, tracklist[trackindex], sizeof(ogg.path));
        trackindex = (trackindex + 1) % trackcount;
    } else {
        Q_snprintf(ogg.path, sizeof(ogg.path), "music/track%02d.ogg", track);
    }

    int ret = FS_FOpenFile(ogg.path, &ogg.f, FS_MODE_READ);
    if (!ogg.f) {
        if (ret != Q_ERR_NOENT)
            Com_EPrintf("Couldn't open %s: %s\n", ogg.path, Q_ErrorString(ret));
        return;
    }

    ret = ov_open_callbacks(&ogg, &ogg.vf, NULL, 0, (ov_callbacks){ my_read });
    if (ret < 0) {
        Com_EPrintf("%s does not appear to be an Ogg bitstream (error %d)\n", ogg.path, ret);
        ogg_stop();
        return;
    }

    Com_DPrintf("Playing %s\n", ogg.path);
    ogg.initialized = true;
}

void OGG_Stop(void)
{
    ogg_stop();

    if (s_started)
        s_api.drop_raw_samples();
}

void OGG_Update(void)
{
    if (!ogg.initialized)
        return;

    if (!s_started)
        return;

    if (!s_active)
        return;

    while (s_api.need_raw_samples()) {
        byte    buffer[4096];
        int     samples;

        samples = ov_read(&ogg.vf, (char *)buffer, sizeof(buffer), USE_BIG_ENDIAN, 2, 1, NULL);
        if (samples == 0) {
            OGG_Play();
            if (ogg.initialized)
                samples = ov_read(&ogg.vf, (char *)buffer, sizeof(buffer), USE_BIG_ENDIAN, 2, 1, NULL);
        }

        if (samples <= 0)
            break;

        vorbis_info *vi = ov_info(&ogg.vf, -1);
        if (!vi || vi->channels > 2)
            break;

        s_api.raw_samples(samples >> vi->channels, vi->rate, 2,
                          vi->channels, buffer, ogg_volume->value);
    }
}

void OGG_Reload(void)
{
    FS_FreeList(tracklist);
    tracklist = FS_ListFiles("music", ".ogg", FS_SEARCH_SAVEPATH, &trackcount);
    trackindex = 0;
}

static void OGG_Info_f(void)
{
    if (ogg.initialized) {
        vorbis_info *vi = ov_info(&ogg.vf, -1);
        Com_Printf("Playing %s, %ld Hz, %d ch\n", ogg.path,
                   vi ? vi->rate : -1, vi ? vi->channels : -1);
    } else {
        Com_Printf("Playback stopped.\n");
    }
}

static void ogg_enable_changed(cvar_t *self)
{
    if (cls.state < ca_precached)
        return;
    if (self->integer)
        OGG_Play();
    else
        OGG_Stop();
}

static void ogg_volume_changed(cvar_t *self)
{
    Cvar_ClampValue(self, 0, 1);
}

void OGG_Init(void)
{
    ogg_enable = Cvar_Get("ogg_enable", "1", 0);
    ogg_enable->changed = ogg_enable_changed;
    ogg_volume = Cvar_Get("ogg_volume", "1", 0);
    ogg_volume->changed = ogg_volume_changed;
    ogg_shuffle = Cvar_Get("ogg_shuffle", "0", 0);
    Cmd_AddCommand("ogginfo", OGG_Info_f);

    OGG_Reload();
}

void OGG_Shutdown(void)
{
    ogg_stop();

    FS_FreeList(tracklist);
    tracklist = NULL;
    trackcount = trackindex = 0;
}
