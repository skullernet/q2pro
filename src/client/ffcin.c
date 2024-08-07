/*
Copyright (C) 2023 Andrey Nazarov

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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/fifo.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#define MAX_PACKETS     2048    // max packets in queue

typedef struct {
    AVFifo      *pkt_list;
    int         nb_packets;
    int64_t     duration;
} PacketQueue;

typedef struct {
    AVCodecContext  *dec_ctx;
    PacketQueue     queue;
    unsigned        timestamp;
    int             stream_idx;
    AVFrame         *frame;
    bool            eof;
} DecoderState;

typedef struct {
    int         width;
    int         height;
    int         pix_fmt;
    int         crop;

    qhandle_t   static_pic;

    AVFormatContext     *fmt_ctx;
    AVPacket            *pkt;
    AVFrame             *frame;
    struct SwsContext   *sws_ctx;
    struct SwrContext   *swr_ctx;

    DecoderState        video;
    DecoderState        audio;

    int64_t     filesize;
    unsigned    framenum;
    unsigned    start_time;
    bool        eof;
} cinematic_t;

static cinematic_t  cin;

static const avformat_t formats[] = {
    { ".ogv", "ogg", AV_CODEC_ID_THEORA },
    { ".mkv", "matroska", AV_CODEC_ID_NONE },
    { ".mp4", "mp4", AV_CODEC_ID_H264 },
    { ".cin", "idcin", AV_CODEC_ID_IDCIN },
};

static char extensions[MAX_QPATH];
static int  supported;

/*
==================
SCR_InitCinematics
==================
*/
void SCR_InitCinematics(void)
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

    Com_DPrintf("Supported cinematic formats: %#x\n", supported);
}

static void packet_queue_destroy(PacketQueue *q);

/*
==================
SCR_StopCinematic
==================
*/
void SCR_StopCinematic(void)
{
    if (cin.video.frame)
        R_UpdateRawPic(0, 0, NULL);

    avcodec_free_context(&cin.video.dec_ctx);
    avcodec_free_context(&cin.audio.dec_ctx);

    avformat_close_input(&cin.fmt_ctx);
    av_packet_free(&cin.pkt);
    av_frame_free(&cin.frame);

    sws_freeContext(cin.sws_ctx);
    swr_free(&cin.swr_ctx);

    av_frame_free(&cin.video.frame);
    av_frame_free(&cin.audio.frame);

    packet_queue_destroy(&cin.video.queue);
    packet_queue_destroy(&cin.audio.queue);

    memset(&cin, 0, sizeof(cin));
}

/*
====================
SCR_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void SCR_FinishCinematic(void)
{
    // stop cinematic, but keep static pic
    if (cin.fmt_ctx) {
        SCR_StopCinematic();
        SCR_BeginLoadingPlaque();
    }

    // tell the server to advance to the next map / cinematic
    CL_ClientCommand(va("nextserver %i\n", cl.servercount));
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacket *pkt1;
    int ret;

    if (q->nb_packets >= MAX_PACKETS) {
        av_packet_unref(pkt);
        return -1;
    }

    pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(pkt);
        return -1;
    }
    av_packet_move_ref(pkt1, pkt);

    ret = av_fifo_write(q->pkt_list, &pkt1, 1);
    if (ret < 0) {
        av_packet_free(&pkt1);
        return ret;
    }

    q->nb_packets++;
    q->duration += pkt1->duration;
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt)
{
    AVPacket *pkt1;
    int ret;

    ret = av_fifo_read(q->pkt_list, &pkt1, 1);
    if (ret < 0)
        return ret;

    q->nb_packets--;
    q->duration -= pkt1->duration;

    av_packet_move_ref(pkt, pkt1);
    av_packet_free(&pkt1);
    return 0;
}

static void packet_queue_destroy(PacketQueue *q)
{
    AVPacket *pkt1;

    if (!q->pkt_list)
        return;

    while (av_fifo_read(q->pkt_list, &pkt1, 1) >= 0)
        av_packet_free(&pkt1);

    av_fifo_freep2(&q->pkt_list);
}

static int process_video(void)
{
    AVFrame *in = cin.frame;
    AVFrame *out = cin.video.frame;
    int ret;

    if (in->width != cin.width || in->height != cin.height || in->format != cin.pix_fmt) {
        Com_EPrintf("Video parameters changed\n");
        return AVERROR_INPUT_CHANGED;
    }

    ret = sws_scale_frame(cin.sws_ctx, out, in);
    if (ret < 0) {
        Com_EPrintf("Error scaling video: %s\n", av_err2str(ret));
        return ret;
    }

    R_UpdateRawPic(cin.width, cin.height, (uint32_t *)out->data[0]);
    return 0;
}

static int process_audio(void)
{
    AVFrame *in = cin.frame;
    AVFrame *out = cin.audio.frame;
    int ret;

    out->nb_samples = MAX_RAW_SAMPLES;
    ret = swr_convert_frame(cin.swr_ctx, out, in);
    if (ret < 0) {
        Com_EPrintf("Error converting audio: %s\n", av_err2str(ret));
        return ret;
    }

    S_RawSamples(out->nb_samples, out->sample_rate, 2, out->ch_layout.nb_channels, out->data[0]);
    return 0;
}

static int decode_frames(DecoderState *s)
{
    AVFrame *frame = cin.frame;
    AVPacket *pkt = cin.pkt;
    AVCodecContext *dec = s->dec_ctx;
    int ret, video_frames = 0;

    if (!dec || s->eof)
        return 0;

    // naive decoding loop: keep reading frames until PTS >= current time
    // assume PTS starts at 0 and monotonically increases
    // no A/V synchronization
    while (s->timestamp < cls.realtime - cin.start_time) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret == AVERROR_EOF) {
            Com_DPrintf("%s from %s decoder\n", av_err2str(ret),
                        av_get_media_type_string(dec->codec->type));
            s->eof = true;
            return 0;
        }

        // do we need a packet?
        if (ret == AVERROR(EAGAIN)) {
            if (packet_queue_get(&s->queue, pkt) < 0) {
                if (cin.eof) {
                    // enter draining mode
                    ret = avcodec_send_packet(dec, NULL);
                } else {
                    // wait for more packets...
                    return 0;
                }
            } else {
                // submit the packet to the decoder
                ret = avcodec_send_packet(dec, pkt);
                av_packet_unref(pkt);
            }
            if (ret < 0) {
                Com_EPrintf("Error submitting %s packet for decoding: %s\n",
                            av_get_media_type_string(dec->codec->type), av_err2str(ret));
                return ret;
            }

            continue;
        }

        if (ret < 0) {
            Com_EPrintf("Error during decoding %s: %s\n",
                        av_get_media_type_string(dec->codec->type), av_err2str(ret));
            return ret;
        }

        // ignore AV_NOPTS_VALUE, etc
        if (frame->pts > 0)
            s->timestamp = av_rescale(frame->pts, dec->pkt_timebase.num * 1000LL, dec->pkt_timebase.den);

        // drop video if we can't keep up, but never drop audio
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO) {
            video_frames++;
        } else {
            ret = process_audio();
            if (ret < 0)
                return ret;
        }
    }

    if (video_frames) {
        if (video_frames > 1)
            Com_DPrintf("Dropped %d video frames\n", video_frames - 1);
        cin.crop = SCR_GetCinematicCrop(cin.framenum, cin.filesize);
        cin.framenum += video_frames;
        return process_video();
    }

    return 0;
}

// buffer 1.5 seconds worth of packets
static int min_duration(AVCodecContext *dec)
{
    if (dec) {
        AVRational *r = &dec->pkt_timebase;
        if (r->num)
            return (r->den + r->den / 2) / r->num;
    }
    return 0;
}

static bool need_more_packets(void)
{
    return
        cin.video.queue.duration < min_duration(cin.video.dec_ctx) ||
        cin.audio.queue.duration < min_duration(cin.audio.dec_ctx);
}

/*
==================
SCR_ReadNextFrame
==================
*/
static bool SCR_ReadNextFrame(void)
{
    AVPacket *pkt = cin.pkt;
    int ret;

    // read frames from the file
    while (!cin.eof && need_more_packets()) {
        ret = av_read_frame(cin.fmt_ctx, pkt);
        // idcin demuxer returns AVERROR(EIO) on EOF packet...
        if (ret == AVERROR_EOF || ret == AVERROR(EIO)) {
            Com_DPrintf("%s from demuxer\n", av_err2str(ret));
            cin.eof = true;
            break;
        }
        if (ret < 0) {
            Com_EPrintf("Error reading packet: %s\n", av_err2str(ret));
            return false;
        }

        // check if the packet belongs to a stream we are interested in,
        // otherwise skip it
        if (pkt->stream_index == cin.video.stream_idx)
            ret = packet_queue_put(&cin.video.queue, pkt);
        else if (pkt->stream_index == cin.audio.stream_idx)
            ret = packet_queue_put(&cin.audio.queue, pkt);
        else
            av_packet_unref(pkt);
        if (ret < 0) {
            Com_EPrintf("Failed to queue packet\n");
            return false;
        }
    }

    if (decode_frames(&cin.video) < 0)
        return false;
    if (decode_frames(&cin.audio) < 0)
        return false;
    if (cin.video.eof && cin.audio.eof)
        return false;

    return true;
}

/*
==================
SCR_RunCinematic
==================
*/
void SCR_RunCinematic(void)
{
    if (cls.state != ca_cinematic)
        return;

    if (!cin.video.frame)
        return;     // static image

    if (cls.key_dest != KEY_GAME) {
        // pause if menu or console is up
        cin.start_time = cls.realtime - cin.video.timestamp;
        return;
    }

    if (!SCR_ReadNextFrame()) {
        SCR_FinishCinematic();
        return;
    }
}

/*
==================
SCR_DrawCinematic
==================
*/
void SCR_DrawCinematic(void)
{
    R_DrawFill8(0, 0, r_config.width, r_config.height, 0);

    if (cin.width > 0 && cin.height > cin.crop && !cin.video.eof) {
        float scale_w = (float)r_config.width / cin.width;
        float scale_h = (float)r_config.height / (cin.height - cin.crop);
        float scale = min(scale_w, scale_h);

        int w = Q_rint(cin.width * scale);
        int h = Q_rint(cin.height * scale);
        int x = (r_config.width - w) / 2;
        int y = (r_config.height - h) / 2;

        if (cin.video.frame)
            R_DrawStretchRaw(x, y, w, h);
        else if (cin.static_pic)
            R_DrawStretchPic(x, y, w, h, cin.static_pic);
    }
}

static bool open_codec_context(enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec;
    AVCodecContext *dec_ctx;
    AVFrame *out;

    ret = av_find_best_stream(cin.fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        if (type == AVMEDIA_TYPE_VIDEO) {
            Com_EPrintf("Couldn't find video stream\n");
            return false;
        }
        // if there is no audio, pretend it hit EOF
        cin.audio.eof = true;
        return true;
    }

    stream_index = ret;
    st = cin.fmt_ctx->streams[stream_index];

    dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) {
        Com_EPrintf("Failed to find %s codec %s\n", av_get_media_type_string(type), avcodec_get_name(st->codecpar->codec_id));
        return false;
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        Com_EPrintf("Failed to allocate %s codec context\n", av_get_media_type_string(type));
        return false;
    }

    ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
    if (ret < 0) {
        Com_EPrintf("Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
        avcodec_free_context(&dec_ctx);
        return false;
    }

    ret = avcodec_open2(dec_ctx, dec, NULL);
    if (ret < 0) {
        Com_EPrintf("Failed to open %s codec\n", av_get_media_type_string(type));
        avcodec_free_context(&dec_ctx);
        return false;
    }

    dec_ctx->pkt_timebase = st->time_base;

    if (type == AVMEDIA_TYPE_VIDEO) {
        cin.video.stream_idx = stream_index;
        cin.video.dec_ctx = dec_ctx;
        cin.width = dec_ctx->width;
        cin.height = dec_ctx->height;
        cin.pix_fmt = dec_ctx->pix_fmt;

        cin.sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                                     dec_ctx->width, dec_ctx->height, AV_PIX_FMT_RGBA,
                                     0, NULL, NULL, NULL);
        if (!cin.sws_ctx) {
            Com_EPrintf("Failed to allocate sws context\n");
            return false;
        }

        cin.video.frame = out = av_frame_alloc();
        if (!out) {
            Com_EPrintf("Failed to allocate video frame\n");
            return false;
        }

        out->width = dec_ctx->width;
        out->height = dec_ctx->height;
        out->format = AV_PIX_FMT_RGBA;

        ret = av_frame_get_buffer(out, 0);
        if (ret < 0) {
            Com_EPrintf("Failed to allocate video buffer\n");
            return false;
        }

        cin.video.queue.pkt_list = av_fifo_alloc2(1, sizeof(AVPacket *), AV_FIFO_FLAG_AUTO_GROW);
        if (!cin.video.queue.pkt_list) {
            Com_EPrintf("Failed to allocate video packet queue\n");
            return false;
        }
    } else {
        cin.audio.stream_idx = stream_index;
        cin.audio.dec_ctx = dec_ctx;

        cin.swr_ctx = swr_alloc();
        if (!cin.swr_ctx) {
            Com_EPrintf("Failed to allocate swr context\n");
            return false;
        }

        cin.audio.frame = out = av_frame_alloc();
        if (!out) {
            Com_EPrintf("Failed to allocate audio frame\n");
            return false;
        }

        int sample_rate = S_GetSampleRate();
        if (!sample_rate)
            sample_rate = dec_ctx->sample_rate;

        if (dec_ctx->ch_layout.nb_channels >= 2)
            out->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
        else
            out->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
        out->format = AV_SAMPLE_FMT_S16;
        out->sample_rate = sample_rate;
        out->nb_samples = MAX_RAW_SAMPLES;

        ret = av_frame_get_buffer(out, 0);
        if (ret < 0) {
            Com_EPrintf("Failed to allocate audio buffer\n");
            return false;
        }

        cin.audio.queue.pkt_list = av_fifo_alloc2(1, sizeof(AVPacket *), AV_FIFO_FLAG_AUTO_GROW);
        if (!cin.audio.queue.pkt_list) {
            Com_EPrintf("Failed to allocate audio packet queue\n");
            return false;
        }
    }

    return true;
}

/*
==================
SCR_StartCinematic
==================
*/
static bool SCR_StartCinematic(const char *name)
{
    char        normalized[MAX_QPATH];
    char        fullname[MAX_OSPATH];
    const char  *path = NULL;
    int         ret;

    FS_NormalizePathBuffer(normalized, name, sizeof(normalized));
    *COM_FileExtension(normalized) = 0;

    // open from filesystem only. since packfiles are downloadable, videos from
    // packfiles can pose security risk due to huge lavf/lavc attack surface.
    while (1) {
        path = FS_NextPath(path);
        if (!path) {
            ret = AVERROR(ENOENT);
            break;
        }

        for (int i = 0; i < q_countof(formats); i++) {
            if (!(supported & BIT(i)))
                continue;

            if (Q_snprintf(fullname, sizeof(fullname), "%s/video/%s%s",
                           path, normalized, formats[i].ext) >= sizeof(fullname)) {
                ret = AVERROR(ENAMETOOLONG);
                goto done;
            }

            ret = avformat_open_input(&cin.fmt_ctx, fullname, NULL, NULL);
            if (ret != AVERROR(ENOENT))
                goto done;
        }
    }

done:
    if (ret < 0) {
        Com_EPrintf("Couldn't open %s: %s\n", ret == AVERROR(ENOENT) ? name : fullname, av_err2str(ret));
        return false;
    }

    ret = avformat_find_stream_info(cin.fmt_ctx, NULL);
    if (ret < 0) {
        Com_EPrintf("Couldn't find stream info: %s\n", av_err2str(ret));
        return false;
    }

#if USE_DEBUG
    if (developer->integer)
        av_dump_format(cin.fmt_ctx, 0, fullname, 0);
#endif

    cin.video.stream_idx = cin.audio.stream_idx = -1;

    if (!open_codec_context(AVMEDIA_TYPE_VIDEO))
        return false;

    if (!open_codec_context(AVMEDIA_TYPE_AUDIO))
        return false;

    cin.frame = av_frame_alloc();
    cin.pkt = av_packet_alloc();
    if (!cin.frame || !cin.pkt) {
        Com_EPrintf("Couldn't allocate memory\n");
        return false;
    }

    cin.framenum = 0;
    cin.start_time = cls.realtime - 1;
    cin.filesize = avio_size(cin.fmt_ctx->pb);

    return SCR_ReadNextFrame();
}

/*
==================
SCR_ReloadCinematic
==================
*/
void SCR_ReloadCinematic(void)
{
    if (cin.video.frame) {
        R_UpdateRawPic(cin.width, cin.height, (uint32_t *)cin.video.frame->data[0]);
    } else if (cl.mapname[0]) {
        cin.static_pic = R_RegisterTempPic(cl.mapname);
        R_GetPicSize(&cin.width, &cin.height, cin.static_pic);
    }
}

/*
==================
SCR_PlayCinematic
==================
*/
void SCR_PlayCinematic(const char *name)
{
    // make sure CD isn't playing music
    OGG_Stop();

    if (!COM_CompareExtension(name, ".pcx")) {
        cin.static_pic = R_RegisterTempPic(name);
        if (!cin.static_pic)
            goto finish;
        R_GetPicSize(&cin.width, &cin.height, cin.static_pic);
    } else if (!COM_CompareExtension(name, ".cin")) {
        if (!SCR_StartCinematic(name))
            goto finish;
    } else {
        goto finish;
    }

    // save picture name for reloading
    Q_strlcpy(cl.mapname, name, sizeof(cl.mapname));

    cls.state = ca_cinematic;

    SCR_EndLoadingPlaque();     // get rid of loading plaque
    Con_Close(false);           // get rid of connection screen
    return;

finish:
    SCR_FinishCinematic();
}

/*
==================
SCR_CheckForCinematic

Called by the server to check for cinematic existence.
Name should be in format "video/<something>.cin".
==================
*/
int SCR_CheckForCinematic(const char *name)
{
    int len = strlen(name);
    int ret = Q_ERR(ENOENT);

    Q_assert(len >= 4);

    for (int i = 0; i < q_countof(formats); i++) {
        if (!(supported & BIT(i)))
            continue;
        ret = FS_LoadFileEx(va("%.*s%s", len - 4, name, formats[i].ext),
                            NULL, FS_TYPE_REAL, TAG_FREE);
        if (ret != Q_ERR(ENOENT))
            break;
    }

    if (ret == Q_ERR(EFBIG))
        ret = Q_ERR_SUCCESS;

    return ret;
}

/*
==================
SCR_Cinematic_g
==================
*/
void SCR_Cinematic_g(genctx_t *ctx)
{
    const unsigned flags = FS_SEARCH_RECURSIVE | FS_SEARCH_STRIPEXT | FS_TYPE_REAL;
    int count;
    void **list;

    ctx->ignoredups = true;
    list = FS_ListFiles("video", extensions, flags, &count);
    for (int i = 0; i < count; i++)
        Prompt_AddMatch(ctx, va("%s.cin", (char *)list[i]));
    FS_FreeList(list);
}
