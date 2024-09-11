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
    AVPacket            *pkt;
    AVFrame             *frame_in;
    AVFrame             *frame_out;
    struct SwrContext   *swr_ctx;
    int                 stream_index;
} ogg_state_t;

static ogg_state_t  ogg;

static cvar_t   *ogg_enable;
static cvar_t   *ogg_volume;
static cvar_t   *ogg_shuffle;

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

    Com_DPrintf("Supported music formats: %#x\n", supported);
}

static void ogg_stop(void)
{
    avcodec_free_context(&ogg.dec_ctx);
    avformat_close_input(&ogg.fmt_ctx);
    av_packet_free(&ogg.pkt);
    av_frame_free(&ogg.frame_in);
    av_frame_free(&ogg.frame_out);
    swr_free(&ogg.swr_ctx);

    memset(&ogg, 0, sizeof(ogg));
}

static AVFormatContext *ogg_open(const char *name, bool autoplay)
{
    char            normalized[MAX_QPATH];
    char            fullname[MAX_OSPATH];
    const char      *path = NULL, *ext;
    AVFormatContext *fmt_ctx = NULL;
    int             ret;

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

    if (ret == AVERROR(ENOENT) && autoplay)
        return NULL;

done:
    if (ret < 0) {
        Com_LPrintf(ret == AVERROR(ENOENT) ? PRINT_ALL : PRINT_ERROR,
                    "Couldn't open %s: %s\n", ret == AVERROR(ENOENT) ||
                    !*fullname ? normalized : fullname, av_err2str(ret));
        return NULL;
    }

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

    ogg.pkt = av_packet_alloc();
    ogg.frame_in = av_frame_alloc();
    ogg.frame_out = av_frame_alloc();
    ogg.swr_ctx = swr_alloc();
    if (!ogg.pkt || !ogg.frame_in || !ogg.frame_out || !ogg.swr_ctx) {
        Com_EPrintf("Couldn't allocate memory\n");
        return false;
    }

    Com_DPrintf("Playing %s\n", ogg.fmt_ctx->url);
    return true;
}

static void ogg_play(AVFormatContext *fmt_ctx)
{
    if (!fmt_ctx)
        return;

    Q_assert(!ogg.fmt_ctx);
    ogg.fmt_ctx = fmt_ctx;

    if (!ogg_try_play())
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

    const char *s = cl.configstrings[CS_CDTRACK];
    if (!*s || !strcmp(s, "0"))
        return;

    if (ogg_shuffle->integer && trackcount) {
        if (trackindex == 0)
            shuffle();
        s = tracklist[trackindex];
        trackindex = (trackindex + 1) % trackcount;
    } else if (COM_IsUint(s)) {
        s = va("track%02d", Q_atoi(s));
    }

    ogg_play(ogg_open(s, true));
}

void OGG_Stop(void)
{
    ogg_stop();

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
    AVPacket *pkt = ogg.pkt;

    while (1) {
        int ret = avcodec_receive_frame(dec, ogg.frame_in);

        if (ret == AVERROR(EAGAIN)) {
            ret = read_packet(pkt);
            if (ret == AVERROR_EOF) {
                ret = avcodec_send_packet(dec, NULL);
            } else if (ret >= 0) {
                ret = avcodec_send_packet(dec, pkt);
                av_packet_unref(pkt);
            }
            if (ret < 0)
                return ret;
            continue;
        }

        return ret;
    }
}

static bool decode_next_frame(void)
{
    int ret = decode_frame();
    if (ret >= 0)
        return true;

    if (ret == AVERROR_EOF)
        Com_DPrintf("%s decoding audio\n", av_err2str(ret));
    else
        Com_EPrintf("Error decoding audio: %s\n", av_err2str(ret));

    // play next file
    OGG_Play();

    return ogg.dec_ctx && decode_frame() >= 0;
}

static int convert_samples(AVFrame *in)
{
    AVFrame *out = ogg.frame_out;
    int ret;

    // exit if not configured yet
    if (out->format < 0)
        return 0;

    // get available free space
    out->nb_samples = s_api->need_raw_samples();
    Q_assert((unsigned)out->nb_samples <= MAX_RAW_SAMPLES);

    ret = swr_convert_frame(ogg.swr_ctx, out, in);
    if (ret < 0)
        return ret;
    if (!out->nb_samples)
        return 0;

    Com_DDDPrintf("%d raw samples\n", out->nb_samples);

    if (!s_api->raw_samples(out->nb_samples, out->sample_rate, 2,
                            out->ch_layout.nb_channels,
                            out->data[0], ogg_volume->value))
        s_api->drop_raw_samples();

    return 1;
}

static int reconfigure_swr(void)
{
    AVFrame *in = ogg.frame_in;
    AVFrame *out = ogg.frame_out;
    int sample_rate = S_GetSampleRate();

    if (!sample_rate)
        sample_rate = out->sample_rate;
    if (!sample_rate)
        sample_rate = in->sample_rate;

    swr_close(ogg.swr_ctx);
    av_frame_unref(out);

    out->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    out->format = AV_SAMPLE_FMT_S16;
    out->sample_rate = sample_rate;
    out->nb_samples = MAX_RAW_SAMPLES;

    return av_frame_get_buffer(out, 0);
}

void OGG_Update(void)
{
    if (!s_started || !s_active || !ogg.dec_ctx)
        return;

    while (s_api->need_raw_samples() > 0) {
        int ret = convert_samples(NULL);

        // if swr buffer is empty, decode more input
        if (ret == 0) {
            if (!decode_next_frame())
                break;

            // now that we have a frame, configure output
            if (ogg.frame_out->format < 0)
                ret = reconfigure_swr();

            if (ret >= 0) {
                ret = convert_samples(ogg.frame_in);
                if (ret == AVERROR_INPUT_CHANGED) {
                    ret = reconfigure_swr();
                    if (ret >= 0)
                        ret = convert_samples(ogg.frame_in);
                }
            }
        }

        if (ret < 0) {
            Com_EPrintf("Error converting audio: %s\n", av_err2str(ret));
            OGG_Stop();
            break;
        }
    }
}

static int sz_read_packet(void *opaque, uint8_t *buf, int size)
{
    sizebuf_t *sz = opaque;

    if (size < 0)
        return AVERROR(EINVAL);

    size = min(size, sz->cursize - sz->readcount);
    if (!size)
        return AVERROR_EOF;

    memcpy(buf, sz->data + sz->readcount, size);
    sz->readcount += size;
    return size;
}

static int64_t sz_seek(void *opaque, int64_t offset, int whence)
{
    sizebuf_t *sz = opaque;

    switch (whence) {
    case SEEK_SET:
        if (offset < 0)
            return AVERROR(EINVAL);
        sz->readcount = min(offset, sz->cursize);
        break;
    case SEEK_CUR:
        if (offset < -(int64_t)sz->readcount)
            return AVERROR(EINVAL);
        sz->readcount += min(offset, (int64_t)(sz->cursize - sz->readcount));
        break;
    case SEEK_END:
        sz->readcount = sz->cursize;
        break;
    default:
        return AVERROR(EINVAL);
    }

    return sz->readcount;
}

bool OGG_Load(sizebuf_t *sz)
{
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *avio_ctx_buffer = NULL;
    const size_t avio_ctx_buffer_size = 4096;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL, *out = NULL;
    struct SwrContext *swr_ctx = NULL;
    AVCodecContext *dec_ctx = NULL;
    AVStream *st;
    bool res = false;
    int ret, sample_rate;

    const AVInputFormat *fmt = av_find_input_format("ogg");
    if (!fmt) {
        Com_SetLastError("Ogg input format not found");
        return false;
    }

    const AVCodec *dec = avcodec_find_decoder(AV_CODEC_ID_VORBIS);
    if (!dec) {
        Com_SetLastError("Vorbis decoder not found");
        return false;
    }

    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        Com_SetLastError("Failed to allocate format context");
        return false;
    }

    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        Com_SetLastError("Failed to allocate avio buffer");
        goto fail;
    }

    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, sz, sz_read_packet, NULL, sz_seek);
    if (!avio_ctx) {
        Com_SetLastError("Failed to allocate avio context");
        goto fail;
    }

    fmt_ctx->pb = avio_ctx;

    ret = avformat_open_input(&fmt_ctx, NULL, fmt, NULL);
    if (ret < 0) {
        Com_SetLastError(av_err2str(ret));
        goto fail;
    }

    if (fmt_ctx->nb_streams != 1) {
        Com_SetLastError("Multiple Ogg streams are not supported");
        goto fail;
    }

    st = fmt_ctx->streams[0];
    if (st->codecpar->codec_id != AV_CODEC_ID_VORBIS) {
        Com_SetLastError("First stream is not Vorbis");
        goto fail;
    }

    if (st->codecpar->ch_layout.nb_channels < 1 || st->codecpar->ch_layout.nb_channels > 2) {
        Com_SetLastError("Unsupported number of channels");
        goto fail;
    }

    if (st->codecpar->sample_rate < 6000 || st->codecpar->sample_rate > 48000) {
        Com_SetLastError("Unsupported sample rate");
        goto fail;
    }

    if (st->duration < 1 || st->duration > MAX_SFX_SAMPLES) {
        Com_SetLastError("Unsupported duration");
        goto fail;
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        Com_SetLastError("Failed to allocate codec context");
        goto fail;
    }

    ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
    if (ret < 0) {
        Com_SetLastError("Failed to copy codec parameters to decoder context");
        goto fail;
    }

    ret = avcodec_open2(dec_ctx, dec, NULL);
    if (ret < 0) {
        Com_SetLastError("Failed to open codec");
        goto fail;
    }

    dec_ctx->pkt_timebase = st->time_base;

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    out = av_frame_alloc();
    swr_ctx = swr_alloc();
    if (!pkt || !frame || !out || !swr_ctx) {
        Com_SetLastError("Failed to allocate memory");
        goto fail;
    }

    sample_rate = S_GetSampleRate();
    if (!sample_rate)
        sample_rate = dec_ctx->sample_rate;

    ret = av_channel_layout_copy(&out->ch_layout, &dec_ctx->ch_layout);
    if (ret < 0) {
        Com_SetLastError("Failed to copy channel layout");
        goto fail;
    }
    out->format = AV_SAMPLE_FMT_S16;
    out->sample_rate = sample_rate;
    out->nb_samples = MAX_RAW_SAMPLES;

    ret = av_frame_get_buffer(out, 0);
    if (ret < 0) {
        Com_SetLastError("Failed to allocate audio buffer");
        goto fail;
    }

    int64_t nb_samples = st->duration;

    if (out->sample_rate != dec_ctx->sample_rate)
        nb_samples = av_rescale_rnd(st->duration + 2, out->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP) + 2;

    int bufsize = nb_samples << out->ch_layout.nb_channels;
    int offset = 0;
    bool eof = false;

    s_info.channels = out->ch_layout.nb_channels;
    s_info.rate = out->sample_rate;
    s_info.width = 2;
    s_info.loopstart = -1;
    s_info.data = FS_AllocTempMem(bufsize);

    while (!eof) {
        ret = avcodec_receive_frame(dec_ctx, frame);

        if (ret == AVERROR(EAGAIN)) {
            ret = av_read_frame(fmt_ctx, pkt);
            if (ret == AVERROR_EOF) {
                ret = avcodec_send_packet(dec_ctx, NULL);
            } else if (ret >= 0) {
                ret = avcodec_send_packet(dec_ctx, pkt);
                av_packet_unref(pkt);
            }
            if (ret < 0)
                break;
            continue;
        }

        out->nb_samples = MAX_RAW_SAMPLES;
        if (ret == AVERROR_EOF) {
            ret = swr_convert_frame(swr_ctx, out, NULL);
            eof = true;
        } else if (ret >= 0) {
            ret = swr_convert_frame(swr_ctx, out, frame);
        }
        if (ret < 0)
            break;

        int size = out->nb_samples << out->ch_layout.nb_channels;
        if (size > bufsize - offset) {
            size = bufsize - offset;
            eof = true;
        }

        memcpy(s_info.data + offset, out->data[0], size);
        offset += size;
    }

    if (ret < 0) {
        Com_SetLastError(av_err2str(ret));
        Z_Freep(&s_info.data);
        goto fail;
    }

    s_info.samples = offset >> s_info.channels;
    res = true;

fail:
    avformat_close_input(&fmt_ctx);
    if (avio_ctx)
        av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
    avcodec_free_context(&dec_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&out);
    swr_free(&swr_ctx);

    return res;
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

    OGG_Stop();

    ogg_play(fmt_ctx);
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
        OGG_Play();
    else
        Com_Printf("Usage: %s <info|play|stop|next>\n", Cmd_Argv(0));
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

    init_formats();

    OGG_LoadTrackList();
}

void OGG_Shutdown(void)
{
    ogg_stop();

    FS_FreeList(tracklist);
    tracklist = NULL;
    trackcount = trackindex = 0;
}
