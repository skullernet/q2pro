/*
Copyright (C) 2010 Andrey Nazarov

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
#include "qal.h"
#include "common/jsmn.h"

// translates from AL coordinate system to quake
#define AL_UnpackVector(v)  -v[1],v[2],-v[0]
#define AL_CopyVector(a,b)  ((b)[0]=-(a)[1],(b)[1]=(a)[2],(b)[2]=-(a)[0])

// OpenAL implementation should support at least this number of sources
#define MIN_CHANNELS    16

static ALuint       s_srcnums[MAX_CHANNELS];
static ALuint       s_stream;
static ALuint       s_stream_buffers;
static ALboolean    s_loop_points;
static ALboolean    s_source_spatialize;
static unsigned     s_framecount;

static ALuint       s_underwater_filter;
static bool         s_underwater_flag;

// reverb stuff


// skips the current token entirely, making sure that
// current_token will point to the actual next logical
// token to be parsed.
static void JSON_SkipToken(jsmntok_t *tokens, size_t num_tokens, jsmntok_t **current_token)
{
    // just in case...
    if ((*current_token - tokens) >= num_tokens) {
        return;
    }

    size_t num_to_parse;

    switch ((*current_token)->type) {
    case JSMN_UNDEFINED:
    case JSMN_STRING:
    case JSMN_PRIMITIVE:
        (*current_token)++;
        break;
    case JSMN_ARRAY: {
        size_t num_to_parse = (*current_token)->size;
        (*current_token)++;
        for (size_t i = 0; i < num_to_parse; i++) {
            JSON_SkipToken(tokens, num_tokens, current_token);
        }
        break;
    }
    case JSMN_OBJECT:
        num_to_parse = (*current_token)->size;
        (*current_token)++;
        for (size_t i = 0; i < num_to_parse; i++) {
            (*current_token)++;
            JSON_SkipToken(tokens, num_tokens, current_token);
        }
        break;
    }
}

static inline bool JSON_Load(const char *filename, const char **buffer, jsmntok_t **tokens, size_t *num_tokens)
{
    *tokens = NULL;

    int64_t buffer_len;

    if ((buffer_len = FS_LoadFile(filename, (void **) buffer)) < 0)
        return false;
    
    // calculate the total token size so we can grok all of them.
    jsmn_parser p;
        
    jsmn_init(&p);
    *num_tokens = jsmn_parse(&p, *buffer, buffer_len, NULL, 0);
        
    *tokens = Z_Malloc(sizeof(jsmntok_t) * (*num_tokens));

    if (!*tokens)
        goto fail;

    // decode all tokens
    jsmn_init(&p);
    jsmn_parse(&p, *buffer, buffer_len, *tokens, (*num_tokens));

    return true;

fail:
    return false;
}

static inline void JSON_Free(jsmntok_t *tokens)
{
    Z_Free(tokens);
}

#define JSON_ENSURE(id) \
    if ((t - tokens) >= num_tokens) { ret = Q_ERR_INVALID_FORMAT; goto fail; } \
    if (t->type != id) { ret = Q_ERR_INVALID_FORMAT; goto fail; }

#define JSON_ENSURE_NEXT(id) \
    JSON_ENSURE(id); t++;

#define JSON_STRCMP(s) \
    strncmp(buffer + t->start, s, t->end - t->start)

typedef struct {
    char    material[16];
    int16_t step_id;
} al_reverb_material_t;

typedef struct {
    al_reverb_material_t    *materials; // if null, matches everything
    size_t                  num_materials;
    char                    preset[16]; // TEMP
} al_reverb_entry_t;

typedef struct {
    float               dimension; // squared
    al_reverb_entry_t   *reverbs;
    size_t              num_reverbs;
} al_reverb_environment_t;

static size_t                   s_num_reverb_environments;
static al_reverb_environment_t  *s_reverb_environments;

static int AL_LoadReverbEntry(const char *buffer, jsmntok_t *tokens, size_t num_tokens, jsmntok_t **t_out, al_reverb_entry_t *out_entry)
{
    int ret = 0;
    jsmntok_t *t = *t_out;

    size_t fields = t->size;
    JSON_ENSURE_NEXT(JSMN_OBJECT);

    for (size_t i = 0; i < fields; i++) {
        if (!JSON_STRCMP("materials")) {
            t++;

            if (t->type == JSMN_STRING) {

                if (buffer[t->start] != '*') {
                    ret = -1;
                    goto fail;
                }

                t++;
            } else {
                size_t n = t->size;
                JSON_ENSURE_NEXT(JSMN_ARRAY);
                out_entry->num_materials = n;
                out_entry->materials = Z_TagMalloc(sizeof(*out_entry->materials) * n, TAG_SOUND);

                for (size_t m = 0; m < n; m++, t++) {
                    JSON_ENSURE(JSMN_STRING);
                    Q_strnlcpy(out_entry->materials[m].material, buffer + t->start, t->end - t->start, sizeof(out_entry->materials[m].material));
                }
            }

        } else if (!JSON_STRCMP("preset")) {
            t++;

            JSON_ENSURE(JSMN_STRING);
            Q_strnlcpy(out_entry->preset, buffer + t->start, t->end - t->start, sizeof(out_entry->preset));
            t++;
        } else {
            t++;
            JSON_SkipToken(tokens, num_tokens, &t);
        }
    }

fail:
    *t_out = t;
    return ret;
}

static int AL_LoadReverbEnvironment(const char *buffer, jsmntok_t *tokens, size_t num_tokens, jsmntok_t **t_out, al_reverb_environment_t *out_environment)
{
    int ret = 0;
    jsmntok_t *t = *t_out;

    size_t fields = t->size;
    JSON_ENSURE_NEXT(JSMN_OBJECT);

    for (size_t i = 0; i < fields; i++) {
        if (!JSON_STRCMP("dimension")) {
            t++;
            JSON_ENSURE(JSMN_PRIMITIVE);
            out_environment->dimension = atof(buffer + t->start);
            out_environment->dimension *= out_environment->dimension;
            t++;
        } else if (!JSON_STRCMP("reverbs")) {
            t++;

            out_environment->num_reverbs = t->size;
            JSON_ENSURE_NEXT(JSMN_ARRAY);
            out_environment->reverbs = Z_TagMallocz(sizeof(al_reverb_entry_t) * out_environment->num_reverbs, TAG_SOUND);

            for (size_t r = 0; r < out_environment->num_reverbs; r++) {
                ret = AL_LoadReverbEntry(buffer, tokens, num_tokens, &t, &out_environment->reverbs[r]);

                if (ret < 0)
                    goto fail;
            }
        } else {
            t++;
            JSON_SkipToken(tokens, num_tokens, &t);
        }
    }

fail:
    *t_out = t;
    return ret;
}

static void AL_FreeReverbEnvironments(al_reverb_environment_t *environments, size_t num_environments)
{
    for (size_t i = 0; i < num_environments; i++) {
        for (size_t n = 0; n < environments[i].num_reverbs; n++) {
            Z_Free(environments[i].reverbs[n].materials);
        }

        Z_Free(environments[i].reverbs);
    }

    Z_Free(environments);
}

static int16_t AL_FindStepID(const char *material)
{
    if (!strcmp(material, "") || !strcmp(material, "default"))
        return FOOTSTEP_ID_DEFAULT;
    else if (!strcmp(material, "ladder"))
        return FOOTSTEP_ID_LADDER;

    mtexinfo_t *out;
    int i;

    // FIXME: can speed this up later with a hash map of some sort
    for (i = 0, out = cl.bsp->texinfo; i < cl.bsp->numtexinfo; i++, out++) {
        if (!strcmp(out->c.material, material)) {
            return out->step_id;
        }
    }

    return FOOTSTEP_ID_DEFAULT;
}

static void AL_SetReverbStepIDs(void)
{
    for (size_t i = 0; i < s_num_reverb_environments; i++) {
        for (size_t n = 0; n < s_reverb_environments[i].num_reverbs; n++) {
            al_reverb_entry_t *entry = &s_reverb_environments[i].reverbs[n];

            for (size_t e = 0; e < entry->num_materials; e++) {
                entry->materials[e].step_id = AL_FindStepID(entry->materials[e].material);
            }
        }
    }
}

static void AL_LoadReverbEnvironments(void)
{
    const char *buffer;
    jsmntok_t *tokens;
    size_t num_tokens;
    int ret = 0;

    if (!JSON_Load("sound/default.environments", &buffer, &tokens, &num_tokens)) {
        ret = Q_ERR_INVALID_FORMAT;
        goto fail;
    }

    jsmntok_t *t = tokens;

    JSON_ENSURE_NEXT(JSMN_OBJECT);

    if (JSON_STRCMP("environments")) {
        ret = Q_ERR_INVALID_FORMAT;
        goto fail;
    }

    t++;

    size_t n = t->size;
    JSON_ENSURE_NEXT(JSMN_ARRAY);

    al_reverb_environment_t *environments = Z_TagMallocz(sizeof(al_reverb_environment_t) * n, TAG_SOUND);

    for (size_t i = 0; i < n; i++) {
        ret = AL_LoadReverbEnvironment(buffer, tokens, num_tokens, &t, &environments[i]);

        if (ret < 0)
            goto fail;
    }

    s_reverb_environments = environments;
    s_num_reverb_environments = n;

    goto free_temp;

fail:
    if (ret < 0) {
        Com_WPrintf("Couldn't load sound/default.environments; invalid JSON\n");
    }

    AL_FreeReverbEnvironments(environments, n);

free_temp:
    FS_FreeFile((void *) buffer);
    JSON_Free(tokens);
}

static void AL_StreamStop(void);

static void AL_SoundInfo(void)
{
    Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_Printf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));
    Com_Printf("Number of sources: %d\n", s_numchannels);
}

static void s_underwater_gain_hf_changed(cvar_t *self)
{
    if (s_underwater_flag) {
        for (int i = 0; i < s_numchannels; i++)
            qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, 0);
        s_underwater_flag = false;
    }

    qalFilterf(s_underwater_filter, AL_LOWPASS_GAINHF, Cvar_ClampValue(self, 0, 1));
}

static bool AL_Init(void)
{
    int i;

    Com_DPrintf("Initializing OpenAL\n");

    if (!QAL_Init()) {
        goto fail0;
    }

    Com_DPrintf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_DPrintf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_DPrintf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_DDPrintf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));

    // check for linear distance extension
    if (!qalIsExtensionPresent("AL_EXT_LINEAR_DISTANCE")) {
        Com_SetLastError("AL_EXT_LINEAR_DISTANCE extension is missing");
        goto fail1;
    }

    // generate source names
    qalGetError();
    qalGenSources(1, &s_stream);
    for (i = 0; i < MAX_CHANNELS; i++) {
        qalGenSources(1, &s_srcnums[i]);
        if (qalGetError() != AL_NO_ERROR) {
            break;
        }
    }

    Com_DPrintf("Got %d AL sources\n", i);

    if (i < MIN_CHANNELS) {
        Com_SetLastError("Insufficient number of AL sources");
        goto fail1;
    }

    s_numchannels = i;

    s_loop_points = qalIsExtensionPresent("AL_SOFT_loop_points");
    s_source_spatialize = qalIsExtensionPresent("AL_SOFT_source_spatialize");

    // init stream source
    qalSourcef(s_stream, AL_ROLLOFF_FACTOR, 0.0f);
    qalSourcei(s_stream, AL_SOURCE_RELATIVE, AL_TRUE);
    if (s_source_spatialize)
        qalSourcei(s_stream, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);

    if (qalIsExtensionPresent("AL_SOFT_direct_channels_remix"))
        qalSourcei(s_stream, AL_DIRECT_CHANNELS_SOFT, AL_REMIX_UNMATCHED_SOFT);
    else if (qalIsExtensionPresent("AL_SOFT_direct_channels"))
        qalSourcei(s_stream, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);

    // init underwater filter
    if (qalGenFilters && qalGetEnumValue("AL_FILTER_LOWPASS")) {
        qalGenFilters(1, &s_underwater_filter);
        qalFilteri(s_underwater_filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        s_underwater_gain_hf->changed = s_underwater_gain_hf_changed;
        s_underwater_gain_hf_changed(s_underwater_gain_hf);
    }

    Com_Printf("OpenAL initialized.\n");
    return true;

fail1:
    QAL_Shutdown();
fail0:
    Com_EPrintf("Failed to initialize OpenAL: %s\n", Com_GetLastError());
    return false;
}

static void AL_Shutdown(void)
{
    Com_Printf("Shutting down OpenAL.\n");

    if (s_numchannels) {
        // delete source names
        qalDeleteSources(s_numchannels, s_srcnums);
        memset(s_srcnums, 0, sizeof(s_srcnums));
        s_numchannels = 0;
    }

    if (s_stream) {
        AL_StreamStop();
        qalDeleteSources(1, &s_stream);
        s_stream = 0;
    }

    if (s_underwater_filter) {
        qalDeleteFilters(1, &s_underwater_filter);
        s_underwater_filter = 0;
    }

    AL_FreeReverbEnvironments(s_reverb_environments, s_num_reverb_environments);
    s_reverb_environments = NULL;
    s_num_reverb_environments = 0;

    s_underwater_flag = false;
    s_underwater_gain_hf->changed = NULL;

    QAL_Shutdown();
}

static sfxcache_t *AL_UploadSfx(sfx_t *s)
{
    byte *converted_data = NULL;
    int sample_width = s_info.width;
     if (s_info.width == 3) {
        /* 24-bit sounds: Sample down to 16-bit.
         * Alternatively, could use AL_EXT_float32 and upload as float. */
        size_t numsamples = s_info.samples * s_info.channels;
        converted_data = Z_Malloc(numsamples * sizeof(uint16_t));
        const byte *input_ptr = s_info.data;
        uint16_t *output_ptr = (uint16_t *)converted_data;
        for (size_t i = 0; i < numsamples; i++) {
            *output_ptr = input_ptr[1] | (input_ptr[2] << 8);
            output_ptr++;
            input_ptr += 3;
        }
        sample_width = 2;
     }

    ALsizei size = s_info.samples * sample_width * s_info.channels;
    ALenum format = AL_FORMAT_MONO8 + (s_info.channels - 1) * 2 + (sample_width - 1);
    ALuint buffer = 0;

    qalGetError();
    qalGenBuffers(1, &buffer);
    if (qalGetError())
        goto fail;

    qalBufferData(buffer, format, converted_data ? converted_data : s_info.data, size, s_info.rate);
    if (qalGetError()) {
        qalDeleteBuffers(1, &buffer);
        goto fail;
    }

    // specify OpenAL-Soft style loop points
    if (s_info.loopstart > 0 && s_loop_points) {
        ALint points[2] = { s_info.loopstart, s_info.samples };
        qalBufferiv(buffer, AL_LOOP_POINTS_SOFT, points);
    }

    // allocate placeholder sfxcache
    sfxcache_t *sc = s->cache = S_Malloc(sizeof(*sc));
    sc->length = s_info.samples * 1000LL / s_info.rate; // in msec
    sc->loopstart = s_info.loopstart;
    sc->width = sample_width;
    sc->channels = s_info.channels;
    sc->size = size;
    sc->bufnum = buffer;

    return sc;

fail:
    Z_Free(converted_data);
    s->error = Q_ERR_LIBRARY_ERROR;
    return NULL;
}

static void AL_DeleteSfx(sfx_t *s)
{
    sfxcache_t *sc = s->cache;
    if (sc) {
        ALuint name = sc->bufnum;
        qalDeleteBuffers(1, &name);
    }
}

static int AL_GetBeginofs(float timeofs)
{
    return s_paintedtime + timeofs * 1000;
}

static void AL_Spatialize(channel_t *ch)
{
    vec3_t      origin;

    // anything coming from the view entity will always be full volume
    // no attenuation = no spatialization
    if (S_IsFullVolume(ch)) {
        VectorCopy(listener_origin, origin);
    } else if (ch->fixed_origin) {
        VectorCopy(ch->origin, origin);
    } else {
        CL_GetEntitySoundOrigin(ch->entnum, origin);
    }

    if (s_source_spatialize) {
        qalSourcei(ch->srcnum, AL_SOURCE_SPATIALIZE_SOFT, !S_IsFullVolume(ch));
    }

    qalSource3f(ch->srcnum, AL_POSITION, AL_UnpackVector(origin));
}

static void AL_StopChannel(channel_t *ch)
{
    if (!ch->sfx)
        return;

#if USE_DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name);
#endif

    // stop it
    qalSourceStop(ch->srcnum);
    qalSourcei(ch->srcnum, AL_BUFFER, AL_NONE);
    memset(ch, 0, sizeof(*ch));
}

static void AL_PlayChannel(channel_t *ch)
{
    sfxcache_t *sc = ch->sfx->cache;

#if USE_DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name);
#endif

    ch->srcnum = s_srcnums[ch - s_channels];
    qalGetError();
    qalSourcei(ch->srcnum, AL_BUFFER, sc->bufnum);
    qalSourcei(ch->srcnum, AL_LOOPING, ch->autosound || sc->loopstart >= 0);
    qalSourcef(ch->srcnum, AL_GAIN, ch->master_vol);
    qalSourcef(ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME);
    qalSourcef(ch->srcnum, AL_MAX_DISTANCE, 8192);
    qalSourcef(ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * (8192 - SOUND_FULLVOLUME));

    AL_Spatialize(ch);

    // play it
    qalSourcePlay(ch->srcnum);
    if (qalGetError() != AL_NO_ERROR) {
        AL_StopChannel(ch);
    }
}

static void AL_IssuePlaysounds(void)
{
    // start any playsounds
    while (1) {
        playsound_t *ps = PS_FIRST(&s_pendingplays);
        if (PS_TERM(ps, &s_pendingplays))
            break;  // no more pending sounds
        if (ps->begin > s_paintedtime)
            break;
        S_IssuePlaysound(ps);
    }
}

static void AL_StopAllSounds(void)
{
    int         i;
    channel_t   *ch;

    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->sfx)
            continue;
        AL_StopChannel(ch);
    }
}

static channel_t *AL_FindLoopingSound(int entnum, sfx_t *sfx)
{
    int         i;
    channel_t   *ch;

    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->autosound)
            continue;
        if (entnum && ch->entnum != entnum)
            continue;
        if (ch->sfx != sfx)
            continue;
        return ch;
    }

    return NULL;
}

static void AL_AddLoopSounds(void)
{
    int         i;
    int         sounds[MAX_EDICTS];
    channel_t   *ch, *ch2;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t *ent;

    if (cls.state != ca_active || sv_paused->integer || !s_ambient->integer)
        return;

    S_BuildSoundList(sounds);

    for (i = 0; i < cl.frame.numEntities; i++) {
        if (!sounds[i])
            continue;

        sfx = S_SfxForHandle(cl.sound_precache[sounds[i]]);
        if (!sfx)
            continue;       // bad sound effect
        sc = sfx->cache;
        if (!sc)
            continue;

        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];

        ch = AL_FindLoopingSound(ent->number, sfx);
        if (ch) {
            ch->autoframe = s_framecount;
            ch->end = s_paintedtime + sc->length;
            continue;
        }

        // allocate a channel
        ch = S_PickChannel(0, 0);
        if (!ch)
            continue;

        // attempt to synchronize with existing sounds of the same type
        ch2 = AL_FindLoopingSound(0, sfx);
        if (ch2) {
            ALfloat offset = 0;
            qalGetSourcef(ch2->srcnum, AL_SAMPLE_OFFSET, &offset);
            qalSourcef(s_srcnums[ch - s_channels], AL_SAMPLE_OFFSET, offset);
        }

        ch->autosound = true;   // remove next frame
        ch->autoframe = s_framecount;
        ch->sfx = sfx;
        ch->entnum = ent->number;
        ch->master_vol = S_GetEntityLoopVolume(ent);
        ch->dist_mult = S_GetEntityLoopDistMult(ent);
        ch->end = s_paintedtime + sc->length;

        AL_PlayChannel(ch);
    }
}

static void AL_StreamUpdate(void)
{
    ALint num_buffers = 0;
    qalGetSourcei(s_stream, AL_BUFFERS_PROCESSED, &num_buffers);
    while (num_buffers-- > 0) {
        ALuint buffer = 0;
        qalSourceUnqueueBuffers(s_stream, 1, &buffer);
        qalDeleteBuffers(1, &buffer);
        s_stream_buffers--;
    }
}

static void AL_StreamStop(void)
{
    qalSourceStop(s_stream);
    AL_StreamUpdate();
    Q_assert(!s_stream_buffers);
}

static int AL_NeedRawSamples(void)
{
    return s_stream_buffers < 32 ? MAX_RAW_SAMPLES : 0;
}

static bool AL_RawSamples(int samples, int rate, int width, int channels, const byte *data, float volume)
{
    ALenum format = AL_FORMAT_MONO8 + (channels - 1) * 2 + (width - 1);
    ALuint buffer = 0;

    if (AL_NeedRawSamples()) {
        qalGetError();
        qalGenBuffers(1, &buffer);
        if (qalGetError())
            return false;

        qalBufferData(buffer, format, data, samples * width * channels, rate);
        if (qalGetError()) {
            qalDeleteBuffers(1, &buffer);
            return false;
        }

        qalSourceQueueBuffers(s_stream, 1, &buffer);
        if (qalGetError()) {
            qalDeleteBuffers(1, &buffer);
            return false;
        }
        s_stream_buffers++;
    }

    qalSourcef(s_stream, AL_GAIN, volume);

    ALint state = AL_PLAYING;
    qalGetSourcei(s_stream, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING)
        qalSourcePlay(s_stream);
    return true;
}

static void AL_UpdateUnderWater(void)
{
    bool underwater = S_IsUnderWater();
    ALint filter = 0;

    if (!s_underwater_filter)
        return;

    if (s_underwater_flag == underwater)
        return;

    if (underwater)
        filter = s_underwater_filter;

    for (int i = 0; i < s_numchannels; i++)
        qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, filter);

    s_underwater_flag = underwater;
}

static void AL_Update(void)
{
    int         i;
    channel_t   *ch;
    ALfloat     orientation[6];

    if (!s_active)
        return;

    s_paintedtime = cl.time;

    // set listener parameters
    qalListener3f(AL_POSITION, AL_UnpackVector(listener_origin));
    AL_CopyVector(listener_forward, orientation);
    AL_CopyVector(listener_up, orientation + 3);
    qalListenerfv(AL_ORIENTATION, orientation);
    qalListenerf(AL_GAIN, s_volume->value);
    qalDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);

    AL_UpdateUnderWater();

    // update spatialization for dynamic sounds
    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->sfx)
            continue;

        if (ch->autosound) {
            // autosounds are regenerated fresh each frame
            if (ch->autoframe != s_framecount) {
                AL_StopChannel(ch);
                continue;
            }
        } else {
            ALenum state = AL_STOPPED;
            qalGetSourcei(ch->srcnum, AL_SOURCE_STATE, &state);
            if (state == AL_STOPPED) {
                AL_StopChannel(ch);
                continue;
            }
        }

#if USE_DEBUG
        if (s_show->integer) {
            ALfloat offset = 0;
            qalGetSourcef(ch->srcnum, AL_SAMPLE_OFFSET, &offset);
            Com_Printf("%d %.1f %.1f %s\n", i, ch->master_vol, offset, ch->sfx->name);
        }
#endif

        AL_Spatialize(ch);         // respatialize channel
    }

    s_framecount++;

    // add loopsounds
    AL_AddLoopSounds();

    AL_IssuePlaysounds();

    AL_StreamUpdate();
}

static void AL_EndRegistration(void)
{
    AL_FreeReverbEnvironments(s_reverb_environments, s_num_reverb_environments);
    s_reverb_environments = NULL;
    s_num_reverb_environments = 0;

    AL_LoadReverbEnvironments();

    if (cl.bsp)
        AL_SetReverbStepIDs();
}

const sndapi_t snd_openal = {
    .init = AL_Init,
    .shutdown = AL_Shutdown,
    .update = AL_Update,
    .activate = S_StopAllSounds,
    .sound_info = AL_SoundInfo,
    .upload_sfx = AL_UploadSfx,
    .delete_sfx = AL_DeleteSfx,
    .raw_samples = AL_RawSamples,
    .need_raw_samples = AL_NeedRawSamples,
    .drop_raw_samples = AL_StreamStop,
    .get_begin_ofs = AL_GetBeginofs,
    .play_channel = AL_PlayChannel,
    .stop_channel = AL_StopChannel,
    .stop_all_sounds = AL_StopAllSounds,
    .end_registration = AL_EndRegistration,
};
