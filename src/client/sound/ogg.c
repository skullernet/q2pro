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
    int channels;
    int rate;
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
    ov_clear(&ogg.vf);
    FS_FCloseFile(ogg.f);
    memset(&ogg, 0, sizeof(ogg));
}

static void ogg_play(void)
{
    int ret = ov_open_callbacks(&ogg, &ogg.vf, NULL, 0, (ov_callbacks){ my_read });
    if (ret < 0) {
        Com_EPrintf("%s does not appear to be an Ogg bitstream (error %d)\n", ogg.path, ret);
        goto fail;
    }

    vorbis_info *vi = ov_info(&ogg.vf, -1);
    if (!vi) {
        Com_EPrintf("Couldn't get info on %s\n", ogg.path);
        goto fail;
    }

    if (vi->channels < 1 || vi->channels > 2) {
        Com_EPrintf("%s has bad number of channels\n", ogg.path);
        goto fail;
    }

    Com_DPrintf("Playing %s\n", ogg.path);

    ogg.initialized = true;
    ogg.channels = vi->channels;
    ogg.rate = vi->rate;
    return;

fail:
    ogg_stop();
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

    ogg_play();
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
        if (samples == 0 && (OGG_Play(), ogg.initialized))
            samples = ov_read(&ogg.vf, (char *)buffer, sizeof(buffer), USE_BIG_ENDIAN, 2, 1, NULL);

        if (samples <= 0)
            break;

        vorbis_info *vi = ov_info(&ogg.vf, -1);
        if (!vi || vi->channels < 1 || vi->channels > 2)
            break;

        ogg.channels = vi->channels;
        ogg.rate = vi->rate;

        if (!s_api.raw_samples(samples >> vi->channels, vi->rate, 2,
                               vi->channels, buffer, ogg_volume->value)) {
            s_api.drop_raw_samples();
            break;
        }
    }
}

static size_t my_read_sz(void *buf, size_t size, size_t nmemb, void *datasource)
{
    sizebuf_t *sz = datasource;
    size_t bytes;

    if (!size || !nmemb) {
        errno = 0;
        return 0;
    }

    if (size > SIZE_MAX / nmemb) {
        errno = EINVAL;
        return 0;
    }

    bytes = min(size * nmemb, sz->cursize - sz->readcount);
    if (bytes) {
        memcpy(buf, sz->data + sz->readcount, bytes);
        sz->readcount += bytes;
    }

    errno = 0;
    return bytes / size;
}

static int my_seek_sz(void *datasource, ogg_int64_t offset, int whence)
{
    sizebuf_t *sz = datasource;

    switch (whence) {
    case SEEK_SET:
        if (offset < 0)
            goto fail;
        sz->readcount = min(offset, sz->cursize);
        break;
    case SEEK_CUR:
        if (offset < -(ogg_int64_t)sz->readcount)
            goto fail;
        sz->readcount += min(offset, (ogg_int64_t)(sz->cursize - sz->readcount));
        break;
    case SEEK_END:
        sz->readcount = sz->cursize;
        break;
    default:
    fail:
        errno = EINVAL;
        return -1;
    }

    errno = 0;
    return 0;
}

static long my_tell_sz(void *datasource)
{
    sizebuf_t *sz = datasource;
    return sz->readcount;
}

bool OGG_Load(sizebuf_t *sz)
{
    ov_callbacks cb = {
        my_read_sz,
        my_seek_sz,
        NULL,
        my_tell_sz
    };

    OggVorbis_File vf;
    int ret = ov_open_callbacks(sz, &vf, NULL, 0, cb);
    if (ret < 0) {
        Com_DPrintf("%s does not appear to be an Ogg bitstream (error %d)\n", s_info.name, ret);
        return false;
    }

    vorbis_info *vi = ov_info(&vf, 0);
    if (!vi) {
        Com_DPrintf("Couldn't get info on %s\n", s_info.name);
        goto fail;
    }

    if (vi->channels < 1 || vi->channels > 2) {
        Com_DPrintf("%s has bad number of channels\n", s_info.name);
        goto fail;
    }

    if (vi->rate < 8000 || vi->rate > 48000) {
        Com_DPrintf("%s has bad rate\n", s_info.name);
        goto fail;
    }

    ogg_int64_t samples = ov_pcm_total(&vf, 0);
    if (samples < 1 || samples > MAX_LOADFILE >> vi->channels) {
        Com_DPrintf("%s has bad number of samples\n", s_info.name);
        goto fail;
    }

    int size = samples << vi->channels;
    int offset = 0;

    s_info.channels = vi->channels;
    s_info.rate = vi->rate;
    s_info.width = 2;
    s_info.loopstart = -1;
    s_info.data = FS_AllocTempMem(size);

    while (offset < size) {
        int bitstream;

        ret = ov_read(&vf, (char *)s_info.data + offset, size - offset, USE_BIG_ENDIAN, 2, 1, &bitstream);
        if (ret == 0 || bitstream)
            break;

        if (ret < 0) {
            Com_DPrintf("Error %d decoding %s\n", ret, s_info.name);
            FS_FreeTempMem(s_info.data);
            goto fail;
        }

        offset += ret;
    }

    s_info.samples = offset >> s_info.channels;

    ov_clear(&vf);
    return true;

fail:
    ov_clear(&vf);
    return false;
}

void OGG_LoadTrackList(void)
{
    FS_FreeList(tracklist);
    tracklist = FS_ListFiles("music", ".ogg", FS_SEARCH_SAVEPATH, &trackcount);
    trackindex = 0;
}

static void OGG_Play_f(void)
{
    char buffer[MAX_QPATH];
    qhandle_t f;

    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: %s %s <track>\n", Cmd_Argv(0), Cmd_Argv(1));
        return;
    }

    if (!s_started) {
        Com_Printf("Sound system not started.\n");
        return;
    }

    if (cls.state == ca_cinematic) {
        Com_Printf("Can't play music in cinematic mode.\n");
        return;
    }

    f = FS_EasyOpenFile(buffer, sizeof(buffer), FS_MODE_READ,
                        "music/", Cmd_Argv(2), ".ogg");
    if (!f)
        return;

    OGG_Stop();

    Q_strlcpy(ogg.path, buffer, sizeof(ogg.path));
    ogg.f = f;
    ogg_play();
}

static void OGG_Info_f(void)
{
    if (ogg.initialized) {
        Com_Printf("Playing %s, %d Hz, %d ch\n", ogg.path, ogg.rate, ogg.channels);
    } else if (ogg.path[0]) {
        Com_Printf("Would play %s, but it failed to load.\n", ogg.path);
    } else {
        Com_Printf("Playback stopped.\n");
    }
}

static void OGG_Cmd_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        Prompt_AddMatch(ctx, "info");
        Prompt_AddMatch(ctx, "play");
        Prompt_AddMatch(ctx, "stop");
        return;
    }

    if (argnum == 2 && !strcmp(Cmd_Argv(1), "play"))
        FS_File_g("music", ".ogg", FS_SEARCH_STRIPEXT, ctx);
}

static void OGG_Cmd_f(void)
{
    const char *cmd = Cmd_Argv(1);

    if (!strcmp(cmd, "info"))
        OGG_Info_f();
    else if (!strcmp(cmd, "play"))
        OGG_Play_f();
    else if (!strcmp(cmd, "stop"))
        OGG_Stop();
    else
        Com_Printf("Usage: %s <info|play|stop>\n", Cmd_Argv(0));
}

static void ogg_enable_changed(cvar_t *self)
{
    if (cls.state < ca_precached || cls.state > ca_active)
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

static const cmdreg_t c_ogg[] = {
    { "ogg", OGG_Cmd_f, OGG_Cmd_c },
    { NULL }
};

void OGG_Init(void)
{
    ogg_enable = Cvar_Get("ogg_enable", "1", 0);
    ogg_enable->changed = ogg_enable_changed;
    ogg_volume = Cvar_Get("ogg_volume", "1", 0);
    ogg_volume->changed = ogg_volume_changed;
    ogg_shuffle = Cvar_Get("ogg_shuffle", "0", 0);

    Cmd_Register(c_ogg);

    OGG_LoadTrackList();
}

void OGG_Shutdown(void)
{
    ogg_stop();

    FS_FreeList(tracklist);
    tracklist = NULL;
    trackcount = trackindex = 0;
}
