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

#include "sound.h"
#include "common/hash_map.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

typedef struct {
    AVFormatContext     *fmt_ctx;
    AVCodecContext      *dec_ctx;
    int                 stream_index;
    char                autotrack[MAX_QPATH];
} ogg_state_t;

static ogg_state_t          ogg;

static AVPacket             *ogg_pkt;
static AVFrame              *ogg_frame_in;
static AVFrame              *ogg_frame_out;
static struct SwrContext    *ogg_swr_ctx;
static bool                 ogg_swr_draining;
static bool                 ogg_manual_play;
static bool                 ogg_paused;

static cvar_t   *ogg_enable;
static cvar_t   *ogg_volume;
static cvar_t   *ogg_shuffle;
static cvar_t   *ogg_menu_track;
static cvar_t   *ogg_remap_tracks;

static hash_map_t   *trackmap;
static const char   **tracklist;
static int          trackcount;
static int          trackindex;

static char     extensions[MAX_QPATH];
static int      supported;

static const avformat_t formats[] = {
    { ".flac", "flac", AV_CODEC_ID_FLAC },
    { ".opus", "ogg", AV_CODEC_ID_OPUS },
    { ".ogg", "ogg", AV_CODEC_ID_VORBIS },
    { ".mp3", "mp3", AV_CODEC_ID_MP3 },
    { ".wav", "wav", AV_CODEC_ID_NONE }
};

static void init_formats(void)
{
    for (int i = 0; i < q_countof(formats); i++) {
        const avformat_t *f = &formats[i];
        if (!av_find_input_format(f->fmt))
            continue;
        if (f->codec_id != AV_CODEC_ID_NONE &&
            !avcodec_find_decoder(f->codec_id))
            continue;
        supported |= BIT(i);
        if (*extensions)
            Q_strlcat(extensions, ";", sizeof(extensions));
        Q_strlcat(extensions, f->ext, sizeof(extensions));
    }

    Com_DPrintf("Supported music formats: %s\n", extensions);
}

static void ogg_close(void)
{
    avcodec_free_context(&ogg.dec_ctx);
    avformat_close_input(&ogg.fmt_ctx);

    memset(&ogg, 0, sizeof(ogg));
}

static AVFormatContext *ogg_open(const char *path)
{
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, path, NULL, NULL);
    if (ret < 0)
        Com_EPrintf("Couldn't open %s: %s\n", path, av_err2str(ret));

    return fmt_ctx;
}

static bool ogg_try_play(void)
{
    AVStream        *st;
    const AVCodec   *dec;
    AVCodecContext  *dec_ctx;
    int             ret;

    ret = avformat_find_stream_info(ogg.fmt_ctx, NULL);
    if (ret < 0) {
        Com_EPrintf("Couldn't find stream info: %s\n", av_err2str(ret));
        return false;
    }

#if USE_DEBUG
    if (developer->integer)
        av_dump_format(ogg.fmt_ctx, 0, ogg.fmt_ctx->url, 0);
#endif

    ret = av_find_best_stream(ogg.fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ret < 0) {
        Com_EPrintf("Couldn't find audio stream\n");
        return false;
    }

    ogg.stream_index = ret;
    st = ogg.fmt_ctx->streams[ogg.stream_index];

    dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) {
        Com_EPrintf("Failed to find audio codec %s\n", avcodec_get_name(st->codecpar->codec_id));
        return false;
    }

    ogg.dec_ctx = dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        Com_EPrintf("Failed to allocate audio codec context\n");
        return false;
    }

    ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
    if (ret < 0) {
        Com_EPrintf("Failed to copy audio codec parameters to decoder context\n");
        return false;
    }

    ret = avcodec_open2(dec_ctx, dec, NULL);
    if (ret < 0) {
        Com_EPrintf("Failed to open audio codec\n");
        return false;
    }

    dec_ctx->pkt_timebase = st->time_base;

    Com_DPrintf("Playing %s\n", ogg.fmt_ctx->url);
    return true;
}

static bool ogg_play(AVFormatContext *fmt_ctx)
{
    if (!fmt_ctx)
        return false;

    Q_assert(!ogg.fmt_ctx);
    ogg.fmt_ctx = fmt_ctx;

    if (!ogg_try_play()) {
        ogg_close();
        return false;
    }

    return true;
}

static void shuffle(void)
{
    for (int i = trackcount - 1; i > 0; i--) {
        int j = Q_rand_uniform(i + 1);
        SWAP(const char *, tracklist[i], tracklist[j]);
    }
}

static int remap_track(int track)
{
    if (ogg_remap_tracks->integer && track >= 2 && track <= 11) {
        if (!Q_stricmp(cl.gamedir, "rogue"))
            return track + 10;

        if (!Q_stricmp(cl.gamedir, "xatrix")) {
            static const byte remap[10] = { 9, 13, 14, 7, 16, 2, 15, 3, 4, 18 };
            return remap[track - 2];
        }
    }

    return track;
}

static const char *lookup_track(const char *name)
{
    const char **path = HashMap_Lookup(const char *, trackmap, &name);
    return path ? *path : NULL;
}

static bool is_known_ext(const char *ext)
{
    for (int i = 0; i < q_countof(formats); i++)
        if (!Q_stricmp(ext, formats[i].ext))
            return true;
    return false;
}

static const char *lookup_track_path(const char *name)
{
    if (!trackcount)
        return NULL;

    if (COM_IsUint(name)) {
        int track = remap_track(Q_atoi(name));
        if (track <= 0)
            return NULL;
        const char *path = lookup_track(va("track%02d", track));
        if (!path)
            path = lookup_track(va("%02d", track));
        return path;
    } else {
        char normalized[MAX_OSPATH];
        if (FS_NormalizePathBuffer(normalized, name, sizeof(normalized)) >= sizeof(normalized))
            return NULL;
        // strip `.ogg' and lookup first possible format
        char *ext = COM_FileExtension(normalized);
        if (is_known_ext(ext))
            *ext = 0;
        return lookup_track(normalized);
    }
}

void OGG_Play(void)
{
    const char *s, *path;

    if (!s_started || cls.state == ca_cinematic || ogg_manual_play)
        return;

    if (cls.state >= ca_connected)
        s = cl.configstrings[CS_CDTRACK];
    else
        s = ogg_menu_track->string;

    if (!*s || !strcmp(s, "0")) {
        OGG_Stop();
        return;
    }

    // don't restart the same track
    if (!Q_stricmp(ogg.autotrack, s))
        return;

    // drop samples if we were playing something
    if (ogg.fmt_ctx)
        OGG_Stop();

    // don't start new track if auto playback disabled
    if (!ogg_enable->integer)
        return;

    Q_strlcpy(ogg.autotrack, s, sizeof(ogg.autotrack));

    if (ogg_shuffle->integer && trackcount) {
        for (int i = 0; i < trackcount; i++) {
            if (trackindex == 0)
                shuffle();
            path = tracklist[trackindex];
            trackindex = (trackindex + 1) % trackcount;
            if (ogg_play(ogg_open(path)))
                break;
        }
    } else {
        path = lookup_track_path(s);
        if (path)
            ogg_play(ogg_open(path));
        else
            Com_DPrintf("No such track: %s\n", s);
    }
}

void OGG_Stop(void)
{
    Com_DPrintf("Stopping music playback\n");
    ogg_close();

    av_frame_unref(ogg_frame_in);
    av_frame_unref(ogg_frame_out);
    if (ogg_swr_ctx)
        swr_close(ogg_swr_ctx);

    ogg_swr_draining = false;
    ogg_manual_play = false;
    ogg_paused = false;

    if (s_api)
        s_api->drop_raw_samples();
}

static int read_packet(AVPacket *pkt)
{
    while (1) {
        int ret = av_read_frame(ogg.fmt_ctx, pkt);
        if (ret < 0)
            return ret;
        if (pkt->stream_index == ogg.stream_index)
            return ret;
        av_packet_unref(pkt);
    }
}

static int decode_frame(void)
{
    AVCodecContext *dec = ogg.dec_ctx;
    AVPacket *pkt = ogg_pkt;

    while (1) {
        int ret = avcodec_receive_frame(dec, ogg_frame_in);
        if (ret != AVERROR(EAGAIN))
            return ret;

        ret = read_packet(pkt);
        if (ret == AVERROR_EOF) {
            ret = avcodec_send_packet(dec, NULL);
        } else if (ret >= 0) {
            ret = avcodec_send_packet(dec, pkt);
            av_packet_unref(pkt);
        }
        if (ret < 0)
            return ret;
    }
}

static bool ogg_rewind(void)
{
    if (ogg_manual_play || !ogg_enable->integer || ogg_shuffle->integer)
        return false;

    int ret = av_seek_frame(ogg.fmt_ctx, ogg.stream_index, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
        return false;

    avcodec_flush_buffers(ogg.dec_ctx);
    ret = decode_frame();
    if (ret < 0)
        return false;

    Com_DPrintf("Rewind successful\n");
    return true;
}

static bool decode_next_frame(void)
{
    if (!ogg.dec_ctx)
        return false;

    int ret = decode_frame();
    if (ret >= 0)
        return true;

    if (ret == AVERROR_EOF)
        Com_DPrintf("%s decoding audio\n", av_err2str(ret));
    else
        Com_EPrintf("Error decoding audio: %s\n", av_err2str(ret));

    // try to rewind if possible
    if (ogg_rewind())
        return true;

    // play next file
    ogg_close();
    OGG_Play();

    return ogg.dec_ctx && decode_frame() >= 0;
}

static int reconfigure_swr(void)
{
    AVFrame *in = ogg_frame_in;
    AVFrame *out = ogg_frame_out;
    int sample_rate = S_GetSampleRate();
    int ret;

    if (!sample_rate)
        sample_rate = out->sample_rate;
    if (!sample_rate)
        sample_rate = in->sample_rate;

    swr_close(ogg_swr_ctx);
    av_frame_unref(out);

    out->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    out->format = s_supports_float ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
    out->sample_rate = sample_rate;
    out->nb_samples = MAX_RAW_SAMPLES;

    char buf[MAX_QPATH];
    av_channel_layout_describe(&in->ch_layout, buf, sizeof(buf));

    Com_DDPrintf("Initializing SWR\n"
                 "Input : %d Hz, %s, %s\n"
                 "Output: %d Hz, stereo, %s\n",
                 in->sample_rate, buf,
                 av_get_sample_fmt_name(in->format),
                 out->sample_rate,
                 av_get_sample_fmt_name(out->format));

    ret = swr_config_frame(ogg_swr_ctx, out, in);
    if (ret < 0)
        return ret;

    ret = swr_init(ogg_swr_ctx);
    if (ret < 0)
        return ret;

    ret = av_frame_get_buffer(out, 0);
    if (ret < 0)
        return ret;

    return 0;
}

static void flush_samples(const AVFrame *out)
{
    Com_DDDPrintf("%d raw samples\n", out->nb_samples);

    if (!s_api->raw_samples(out->nb_samples, out->sample_rate,
                            av_get_bytes_per_sample(out->format),
                            out->ch_layout.nb_channels,
                            out->data[0], ogg_volume->value))
        s_api->drop_raw_samples();
}

static int convert_frame(AVFrame *out, AVFrame *in)
{
    int ret = swr_convert_frame(ogg_swr_ctx, out, in);
    if (ret < 0)
        return ret;
    in->nb_samples = 0; // don't convert again
    return 0;
}

static int convert_audio(void)
{
    AVFrame *in = ogg_frame_in;
    AVFrame *out = ogg_frame_out;
    int ret = 0, need, have = 0;

    if (ogg_paused)
        return 0;

    // get available free space
    need = s_api->need_raw_samples();
    if (need <= 0)
        return 0;

    Q_assert(need <= MAX_RAW_SAMPLES);
    out->nb_samples = need;

drain:
    if (ogg_swr_draining) {
        ret = swr_convert_frame(ogg_swr_ctx, out, NULL);
        if (ret < 0)
            return ret;
        if (out->nb_samples) {
            flush_samples(out);
            return 0;
        }

        Com_DDPrintf("Draining done\n");
        ogg_swr_draining = false;

        if (!ogg.dec_ctx) {
            // playback just stopped
            swr_close(ogg_swr_ctx);
            av_frame_unref(in);
            av_frame_unref(out);
            return 0;
        }

        ret = reconfigure_swr();
        if (ret < 0)
            return ret;
        ret = convert_frame(NULL, in);
        if (ret < 0)
            return ret;
    }

    // see how much we buffered
    if (swr_is_initialized(ogg_swr_ctx)) {
        have = swr_get_out_samples(ogg_swr_ctx, 0);
        if (have < 0)
            return have;
    }

    // buffer more frames to fill available space
    while (have < need) {
        if (!decode_next_frame()) {
            if (!swr_is_initialized(ogg_swr_ctx))
                return 0;
            Com_DDPrintf("No next frame, draining\n");
            ogg_swr_draining = true;
            goto drain;
        }

        // work around swr channel layout bug
        if (in->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&in->ch_layout, in->ch_layout.nb_channels);

        // now that we have a frame, configure swr
        if (!swr_is_initialized(ogg_swr_ctx)) {
            ret = reconfigure_swr();
            if (ret < 0)
                return ret;
        }

        have = swr_get_out_samples(ogg_swr_ctx, in->nb_samples);
        if (have < 0)
            return have;
        if (have < need) {
            ret = convert_frame(NULL, in);
            if (ret < 0)
                break;
        }
    }

    // now output what we have
    if (ret >= 0)
        ret = convert_frame(out, in);

    if (ret == AVERROR_INPUT_CHANGED) {
        // wait for swr buffer to drain, then reconfigure
        Com_DDPrintf("Input changed, draining\n");
        ogg_swr_draining = true;
        goto drain;
    }

    if (ret < 0)
        return ret;

    if (out->nb_samples)
        flush_samples(out);

    return 0;
}

void OGG_Update(void)
{
    if (!s_started || !s_active)
        return;

    if (!ogg.dec_ctx && !ogg_swr_draining) {
        // resume auto playback if manual playback just stopped
        if (ogg_manual_play && !s_api->have_raw_samples()) {
            ogg_manual_play = false;
            OGG_Play();
        }
        if (!ogg.dec_ctx)
            return;
    }

    int ret = convert_audio();
    if (ret < 0) {
        Com_EPrintf("Error converting audio: %s\n", av_err2str(ret));
        OGG_Stop();
    }
}

static void add_music_dir(const char *path)
{
    char fullpath[MAX_OSPATH];
    size_t len;

    len = Q_snprintf(fullpath, sizeof(fullpath), "%s/music", path);
    if (len >= sizeof(fullpath))
        return;

    listfiles_t list = {
        .filter = extensions,
        .flags = FS_SEARCH_RECURSIVE,
    };
    Sys_ListFiles_r(&list, fullpath, 0);
    FS_FinalizeList(&list);

    if (HashMap_Size(trackmap) > MAX_LISTED_FILES - list.count) {
        FS_FreeList(list.files);
        return;
    }

    for (int i = 0; i < list.count; i++) {
        char *val = list.files[i];
        char base[MAX_OSPATH];

        COM_StripExtension(base, val + len + 1, sizeof(base));
        if (!lookup_track(base)) {
            char *key = Z_CopyString(base);
            HashMap_Insert(trackmap, &key, &val);
            Com_DDPrintf("Adding %s\n", val);
        } else {
            Z_Free(val);
        }
    }

    Z_Free(list.files);
}

static void free_track_list(void)
{
    if (trackmap) {
        for (int i = 0; i < trackcount; i++) {
            Z_Free(*HashMap_GetKey  (char *, trackmap, i));
            Z_Free(*HashMap_GetValue(char *, trackmap, i));
        }

        HashMap_Destroy(trackmap);
        trackmap = NULL;
    }

    Z_Free(tracklist);
    tracklist  = NULL;
    trackcount = trackindex = 0;
}

void OGG_LoadTrackList(void)
{
    if (!*extensions)
        return;

    free_track_list();

    trackmap = HashMap_Create(char *, char *, HashCaseStr, HashCaseStrCmp);

    const char *path = NULL;
    while ((path = FS_NextPath(path)))
        add_music_dir(path);

    // GOG hacks
    if (sys_homedir->string[0])
        add_music_dir(sys_homedir->string);

    add_music_dir(sys_basedir->string);

    // prepare tracklist for shuffling
    trackcount = HashMap_Size(trackmap);
    tracklist  = Z_Malloc(trackcount * sizeof(tracklist[0]));

    for (int i = 0; i < trackcount; i++)
        tracklist[i] = *HashMap_GetValue(const char *, trackmap, i);

    Com_DPrintf("Found %d music tracks.\n", trackcount);
}

static void OGG_Play_f(void)
{
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

    const char *path = lookup_track_path(Cmd_Argv(2));
    if (!path) {
        Com_Printf("No such track: %s\n", Cmd_Argv(2));
        return;
    }

    AVFormatContext *fmt_ctx = ogg_open(path);
    if (!fmt_ctx)
        return;

    if (!strcmp(Cmd_Argv(3), "soft"))
        ogg_close();
    else
        OGG_Stop();

    ogg_manual_play = ogg_play(fmt_ctx);
}

static void OGG_Info_f(void)
{
    AVCodecContext *dec = ogg.dec_ctx;

    if (dec) {
        Com_Printf("Playing %s, %s, %d Hz, %d ch%s\n",
                   COM_SkipPath(ogg.fmt_ctx->url), dec->codec->name,
                   dec->sample_rate, dec->ch_layout.nb_channels,
                   ogg_paused ? " [PAUSED]" : "");
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
        Prompt_AddMatch(ctx, "next");
        Prompt_AddMatch(ctx, "pause");
        return;
    }

    if (argnum == 2 && !strcmp(Cmd_Argv(1), "play")) {
        ctx->ignorecase = true;
        for (int i = 0; i < trackcount; i++)
            Prompt_AddMatch(ctx, *HashMap_GetKey(const char *, trackmap, i));
    }
}

static void OGG_Next_f(void)
{
    if (!s_started) {
        Com_Printf("Sound system not started.\n");
        return;
    }

    if (cls.state == ca_cinematic) {
        Com_Printf("Can't play music in cinematic mode.\n");
        return;
    }

    ogg_manual_play = false;
    ogg.autotrack[0] = 0;

    OGG_Play();
}

static void OGG_Pause_f(void)
{
    if (!ogg.dec_ctx) {
        Com_Printf("Playback stopped.\n");
        return;
    }

    if (Cmd_Argc() > 2)
        ogg_paused = Q_atoi(Cmd_Argv(2));
    else
        ogg_paused ^= true;

    S_PauseRawSamples(ogg_paused);
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
    else if (!strcmp(cmd, "next"))
        OGG_Next_f();
    else if (!strcmp(cmd, "pause") || !strcmp(cmd, "toggle"))
        OGG_Pause_f();
    else
        Com_Printf("Usage: %s <info|play|stop|next|pause>\n", Cmd_Argv(0));
}

static void ogg_enable_changed(cvar_t *self)
{
    if (cls.state == ca_cinematic || ogg_manual_play)
        return;

    // pause/resume if already playing
    if (ogg.dec_ctx) {
        ogg_paused = !self->integer;
        S_PauseRawSamples(ogg_paused);
        return;
    }

    if (self->integer)
        OGG_Play();
    else
        OGG_Stop();
}

static void ogg_volume_changed(cvar_t *self)
{
    Cvar_ClampValue(self, 0, 1);
}

static void ogg_menu_track_changed(cvar_t *self)
{
    if (cls.state < ca_connected)
        OGG_Play();
}

static void ogg_remap_tracks_changed(cvar_t *self)
{
    if (cls.state >= ca_connected)
        OGG_Play();
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
    ogg_menu_track = Cvar_Get("ogg_menu_track", "0", 0);
    ogg_menu_track->changed = ogg_menu_track_changed;
    ogg_remap_tracks = Cvar_Get("ogg_remap_tracks", "1", 0);
    ogg_remap_tracks->changed = ogg_remap_tracks_changed;

    Cmd_Register(c_ogg);

    init_formats();

    OGG_LoadTrackList();

    Q_assert(ogg_pkt = av_packet_alloc());
    Q_assert(ogg_frame_in = av_frame_alloc());
    Q_assert(ogg_frame_out = av_frame_alloc());
    Q_assert(ogg_swr_ctx = swr_alloc());
}

void OGG_Shutdown(void)
{
    ogg_close();

    av_packet_free(&ogg_pkt);
    av_frame_free(&ogg_frame_in);
    av_frame_free(&ogg_frame_out);
    swr_free(&ogg_swr_ctx);

    ogg_swr_draining = false;
    ogg_manual_play = false;
    ogg_paused = false;

    free_track_list();
}
