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

static cvar_t   *ogg_enable;
static cvar_t   *ogg_volume;
static cvar_t   *ogg_shuffle;
static cvar_t   *ogg_menu_track;

static void     **tracklist;
static int      trackcount;
static int      trackindex;

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

static AVFormatContext *ogg_open(const char *name, bool autoplay)
{
    char            normalized[MAX_QPATH];
    char            fullname[MAX_OSPATH];
    const char      *path = NULL, *ext;
    AVFormatContext *fmt_ctx = NULL;
    int             ret;
    print_type_t    level = PRINT_ERROR;

    fullname[0] = 0;

    if (FS_NormalizePathBuffer(normalized, name, sizeof(normalized)) >= sizeof(normalized)) {
        ret = AVERROR(ENAMETOOLONG);
        goto done;
    }

    ext = COM_FileExtension(normalized);

    // open from filesystem only. since packfiles are downloadable, music from
    // packfiles can pose security risk due to huge lavf/lavc attack surface.
    while (1) {
        path = FS_NextPath(path);
        if (!path) {
            ret = AVERROR(ENOENT);
            break;
        }

        // try original filename if it has an extension
        if (*ext) {
            if (Q_snprintf(fullname, sizeof(fullname), "%s/music/%s", path, normalized) >= sizeof(fullname)) {
                ret = AVERROR(ENAMETOOLONG);
                break;
            }

            ret = avformat_open_input(&fmt_ctx, fullname, NULL, NULL);
            if (ret != AVERROR(ENOENT))
                break;
        }

        // try to append different extensions
        for (int i = 0; i < q_countof(formats); i++) {
            if (!(supported & BIT(i)))
                continue;

            if (!Q_stricmp(ext, formats[i].ext))
                continue;

            if (Q_snprintf(fullname, sizeof(fullname), "%s/music/%s%s",
                           path, normalized, formats[i].ext) >= sizeof(fullname)) {
                ret = AVERROR(ENAMETOOLONG);
                goto done;
            }

            ret = avformat_open_input(&fmt_ctx, fullname, NULL, NULL);
            if (ret != AVERROR(ENOENT))
                goto done;
        }
    }

done:
    if (ret >= 0)
        return fmt_ctx;

    if (ret == AVERROR(ENOENT)) {
        if (autoplay) {
            if (!COM_DEVELOPER)
                return NULL;
            level = PRINT_DEVELOPER;
        } else {
            level = PRINT_ALL;
        }
    }

    Com_LPrintf(level, "Couldn't open %s: %s\n", ret == AVERROR(ENOENT) ||
                !*fullname ? normalized : fullname, av_err2str(ret));
    return NULL;
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
        SWAP(void *, tracklist[i], tracklist[j]);
    }
}

void OGG_Play(void)
{
    const char *s;

    if (!s_started || !ogg_enable->integer || cls.state == ca_cinematic)
        return;

    // don't interfere with manual playback
    if (ogg_manual_play)
        return;

    if (cls.state >= ca_connected)
        s = cl.configstrings[CS_CDTRACK];
    else
        s = ogg_menu_track->string;

    if (!*s || !strcmp(s, "0")) {
        OGG_Stop();
        return;
    }

    if (COM_IsUint(s))
        s = va("track%02d", Q_atoi(s));

    // don't restart the same track
    if (!strcmp(ogg.autotrack, s))
        return;

    // drop samples if we were playing something
    if (ogg.fmt_ctx)
        OGG_Stop();

    Q_strlcpy(ogg.autotrack, s, sizeof(ogg.autotrack));

    if (ogg_shuffle->integer && trackcount) {
        for (int i = 0; i < trackcount; i++) {
            if (trackindex == 0)
                shuffle();
            s = tracklist[trackindex];
            trackindex = (trackindex + 1) % trackcount;
            if (ogg_play(ogg_open(s, true)))
                break;
        }
    } else {
        ogg_play(ogg_open(s, true));
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
    out->format = AV_SAMPLE_FMT_S16;
    out->sample_rate = sample_rate;
    out->nb_samples = MAX_RAW_SAMPLES;

    char buf[MAX_QPATH];
    av_channel_layout_describe(&in->ch_layout, buf, sizeof(buf));

    Com_DDPrintf("Initializing SWR\n"
                 "Input : %d Hz, %s, %s\n"
                 "Output: %d Hz, stereo, s16\n",
                 in->sample_rate, buf,
                 av_get_sample_fmt_name(in->format),
                 out->sample_rate);

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

    if (!s_api->raw_samples(out->nb_samples, out->sample_rate, 2,
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

void OGG_LoadTrackList(void)
{
    FS_FreeList(tracklist);
    tracklist = FS_ListFiles("music", extensions, FS_SEARCH_STRIPEXT | FS_TYPE_REAL, &trackcount);
    trackindex = 0;
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

    AVFormatContext *fmt_ctx = ogg_open(Cmd_Argv(2), false);
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
        Com_Printf("Playing %s, %s, %d Hz, %d ch\n",
                   COM_SkipPath(ogg.fmt_ctx->url), dec->codec->name,
                   dec->sample_rate, dec->ch_layout.nb_channels);
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
        return;
    }

    if (argnum == 2 && !strcmp(Cmd_Argv(1), "play"))
        FS_File_g("music", extensions, FS_SEARCH_STRIPEXT | FS_TYPE_REAL, ctx);
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
    else
        Com_Printf("Usage: %s <info|play|stop|next>\n", Cmd_Argv(0));
}

static void ogg_enable_changed(cvar_t *self)
{
    if (cls.state == ca_cinematic)
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

static void ogg_menu_track_changed(cvar_t *self)
{
    if (cls.state < ca_connected)
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

    FS_FreeList(tracklist);
    tracklist = NULL;
    trackcount = trackindex = 0;
}
