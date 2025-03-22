/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// snd_mem.c: sound caching

#include "sound.h"
#include "common/intreadwrite.h"

#define FORMAT_PCM  1

wavinfo_t s_info;

/*
===============================================================================

OGG loading

===============================================================================
*/

#ifdef USE_AVCODEC

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

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

static bool OGG_Load(sizebuf_t *sz)
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

#endif // USE_AVCODEC

/*
===============================================================================

WAV loading

===============================================================================
*/

#define TAG_RIFF    MakeLittleLong('R','I','F','F')
#define TAG_WAVE    MakeLittleLong('W','A','V','E')
#define TAG_fmt     MakeLittleLong('f','m','t',' ')
#define TAG_cue     MakeLittleLong('c','u','e',' ')
#define TAG_LIST    MakeLittleLong('L','I','S','T')
#define TAG_mark    MakeLittleLong('m','a','r','k')
#define TAG_data    MakeLittleLong('d','a','t','a')

static int FindChunk(sizebuf_t *sz, uint32_t search)
{
    while (sz->readcount + 8 < sz->cursize) {
        uint32_t chunk = SZ_ReadLong(sz);
        uint32_t len   = SZ_ReadLong(sz);

        len = min(len, sz->cursize - sz->readcount);
        if (chunk == search)
            return len;

        sz->readcount += Q_ALIGN(len, 2);
    }

    return 0;
}

static bool GetWavinfo(sizebuf_t *sz)
{
    int tag, samples, width, chunk_len, next_chunk;

    tag = SZ_ReadLong(sz);

#if USE_AVCODEC
    if (tag == MakeLittleLong('O','g','g','S') || !COM_CompareExtension(s_info.name, ".ogg")) {
        sz->readcount = 0;
        return OGG_Load(sz);
    }
#endif

// find "RIFF" chunk
    if (tag != TAG_RIFF) {
        Com_SetLastError("Missing RIFF chunk");
        return false;
    }

    sz->readcount += 4;
    if (SZ_ReadLong(sz) != TAG_WAVE) {
        Com_SetLastError("Missing WAVE chunk");
        return false;
    }

// save position after "WAVE" tag
    next_chunk = sz->readcount;

// find "fmt " chunk
    if (!FindChunk(sz, TAG_fmt)) {
        Com_SetLastError("Missing fmt chunk");
        return false;
    }

    s_info.format = SZ_ReadShort(sz);
    if (s_info.format != FORMAT_PCM) {
        Com_SetLastError("Unsupported PCM format");
        return false;
    }

    s_info.channels = SZ_ReadShort(sz);
    if (s_info.channels < 1 || s_info.channels > 2) {
        Com_SetLastError("Unsupported number of channels");
        return false;
    }

    s_info.rate = SZ_ReadLong(sz);
    if (s_info.rate < 6000 || s_info.rate > 48000) {
        Com_SetLastError("Unsupported sample rate");
        return false;
    }

    sz->readcount += 6;
    width = SZ_ReadShort(sz);
    switch (width) {
    case 8:
    case 16:
    case 24:
        s_info.width = width / 8;
        break;
    default:
        Com_SetLastError("Unsupported number of bits per sample");
        return false;
    }

// find "data" chunk
    sz->readcount = next_chunk;
    chunk_len = FindChunk(sz, TAG_data);
    if (!chunk_len) {
        Com_SetLastError("Missing data chunk");
        return false;
    }

// calculate length in samples
    s_info.samples = chunk_len / (s_info.width * s_info.channels);
    if (s_info.samples < 1) {
        Com_SetLastError("No samples");
        return false;
    }
    if (s_info.samples > MAX_SFX_SAMPLES) {
        Com_SetLastError("Too many samples");
        return false;
    }

// any errors are non-fatal from this point
    s_info.data = sz->data + sz->readcount;
    s_info.loopstart = -1;

// find "cue " chunk
    sz->readcount = next_chunk;
    chunk_len = FindChunk(sz, TAG_cue);
    if (!chunk_len) {
        return true;
    }

// save position after "cue " chunk
    next_chunk = sz->readcount + Q_ALIGN(chunk_len, 2);

    sz->readcount += 24;
    samples = SZ_ReadLong(sz);
    if (samples < 0 || samples >= s_info.samples) {
        Com_DPrintf("%s has bad loop start\n", s_info.name);
        return true;
    }
    s_info.loopstart = samples;

// if the next chunk is a "LIST" chunk, look for a cue length marker
    sz->readcount = next_chunk;
    if (!FindChunk(sz, TAG_LIST)) {
        return true;
    }

    sz->readcount += 20;
    if (SZ_ReadLong(sz) != TAG_mark) {
        return true;
    }

// this is not a proper parse, but it works with cooledit...
    sz->readcount -= 8;
    samples = SZ_ReadLong(sz);  // samples in loop
    if (samples < 1 || samples > s_info.samples - s_info.loopstart) {
        Com_DPrintf("%s has bad loop length\n", s_info.name);
        return true;
    }
    s_info.samples = s_info.loopstart + samples;

    return true;
}

static void ConvertSamples(void)
{
    uint16_t *data = (uint16_t *)s_info.data;
    int count = s_info.samples * s_info.channels;

// sigh. truncate 24 bit to 16
    if (s_info.width == 3) {
        for (int i = 0; i < count; i++)
            data[i] = RL32(&s_info.data[i * 3]) >> 8;
        s_info.width = 2;
        return;
    }

#if USE_BIG_ENDIAN
    if (s_info.width == 2) {
        for (int i = 0; i < count; i++)
            data[i] = LittleShort(data[i]);
    }
#endif
}

// ===============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound(sfx_t *s)
{
    sizebuf_t   sz;
    byte        *data;
    sfxcache_t  *sc;
    int         len;
    char        *name;

    if (s->name[0] == '*')
        return NULL;

// see if still in memory
    sc = s->cache;
    if (sc)
        return sc;

// don't retry after error
    if (s->error)
        return NULL;

// load it in
    if (s->truename)
        name = s->truename;
    else
        name = s->name;

    len = FS_LoadFile(name, (void **)&data);
    if (!data) {
        if (len != Q_ERR(ENOENT))
            Com_EPrintf("Couldn't load %s: %s\n", Com_MakePrintable(name), Q_ErrorString(len));
        s->error = len;
        return NULL;
    }

    memset(&s_info, 0, sizeof(s_info));
    s_info.name = name;

    SZ_InitRead(&sz, data, len);

    if (!GetWavinfo(&sz)) {
        s->error = Q_ERR_INVALID_FORMAT;
        goto fail;
    }

    if (s_info.format == FORMAT_PCM)
        ConvertSamples();

    sc = s_api->upload_sfx(s);

#if USE_AVCODEC
    if (s_info.format != FORMAT_PCM)
        FS_FreeTempMem(s_info.data);
#endif

fail:
    if (!sc)
        Com_EPrintf("Couldn't load %s: %s\n", Com_MakePrintable(name), Com_GetLastError());
    FS_FreeFile(data);
    return sc;
}
