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
// sv_game.c -- interface to the game dll

#include "server.h"
#include "game3_proxy/game3_proxy.h"

#if USE_CLIENT
#include "client/video.h"
#endif

const game_export_t     *ge;
const game_export_ex_t  *gex;

static void PF_configstring(int index, const char *val);

/*
================
PF_FindIndex

================
*/
static int PF_FindIndex(const char *name, int start, int max, int skip, const char *func)
{
    char *string;
    int i;

    if (!name || !name[0])
        return 0;

    for (i = 1; i < max; i++) {
        if (i == skip) {
            continue;
        }
        string = sv.configstrings[start + i];
        if (!string[0]) {
            break;
        }
        if (!strcmp(string, name)) {
            return i;
        }
    }

    if (i == max) {
        if (g_features->integer & GMF_ALLOW_INDEX_OVERFLOW) {
            Com_DPrintf("%s(%s): overflow\n", func, name);
            return 0;
        }
        Com_Error(ERR_DROP, "%s(%s): overflow", func, name);
    }

    PF_configstring(i + start, name);

    return i;
}

static int PF_ModelIndex(const char *name)
{
    return PF_FindIndex(name, svs.csr.models, svs.csr.max_models, MODELINDEX_PLAYER, __func__);
}

static int PF_SoundIndex(const char *name)
{
    return PF_FindIndex(name, svs.csr.sounds, svs.csr.max_sounds, 0, __func__);
}

static int PF_ImageIndex(const char *name)
{
    return PF_FindIndex(name, svs.csr.images, svs.csr.max_images, 0, __func__);
}

/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client.
Archived in MVD stream.
===============
*/
static void PF_Unicast(edict_t *ent, bool reliable, uint32_t dupe_key)
{
    client_t    *client;
    int         cmd, flags, clientNum;

    if (!ent) {
        goto clear;
    }

    clientNum = NUM_FOR_EDICT(ent) - 1;
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
        goto clear;
    }

    client = svs.client_pool + clientNum;
    if (client->state <= cs_zombie) {
        Com_WPrintf("%s to a free/zombie client %d\n", __func__, clientNum);
        goto clear;
    }

    if (!msg_write.cursize) {
        Com_DPrintf("%s with empty data\n", __func__);
        goto clear;
    }

    cmd = msg_write.data[0];

    flags = 0;
    if (reliable) {
        flags |= MSG_RELIABLE;
    }

    if (cmd == svc_layout || (cmd == svc_configstring && RL16(&msg_write.data[1]) == CS_STATUSBAR)) {
        flags |= MSG_COMPRESS_AUTO;
    }

    SV_ClientAddMessage(client, flags);

    // fix anti-kicking exploit for broken mods
    if (cmd == svc_disconnect) {
        client->drop_hack = true;
        goto clear;
    }

    SV_MvdUnicast(ent, clientNum, reliable);

clear:
    SZ_Clear(&msg_write);
}

/*
=================
PF_bprintf

Sends text to all active clients.
Archived in MVD stream.
=================
*/
static void PF_Broadcast_Print(int level, const char *msg)
{
    char        string[MAX_STRING_CHARS];
    client_t    *client;
    size_t      len;
    int         i;

    len = Q_strlcpy(string, msg, sizeof(string));
    if (len >= sizeof(string)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }

    SV_MvdBroadcastPrint(level, string);

    MSG_WriteByte(svc_print);
    MSG_WriteByte(level);
    MSG_WriteData(string, len + 1);

    // echo to console
    if (COM_DEDICATED) {
        // mask off high bits
        for (i = 0; i < len; i++)
            string[i] &= 127;
        Com_Printf("%s", string);
    }

    FOR_EACH_CLIENT(client) {
        if (client->state != cs_spawned)
            continue;
        if (level >= client->messagelevel) {
            SV_ClientAddMessage(client, MSG_RELIABLE);
        }
    }

    SZ_Clear(&msg_write);
}

/*
===============
PF_dprintf

Debug print to server console.
===============
*/
static void PF_Com_Print(const char *msg)
{
    Con_SkipNotify(true);
    Com_Printf("%s", msg);
    Con_SkipNotify(false);
}

/*
===============
PF_cprintf

Print to a single client if the level passes.
Archived in MVD stream.
===============
*/
static void PF_Client_Print(edict_t *ent, int level, const char *msg)
{
    int         clientNum;
    client_t    *client;

    if (!ent) {
        Com_LPrintf(level == PRINT_CHAT ? PRINT_TALK : PRINT_ALL, "%s", msg);
        return;
    }

    clientNum = NUM_FOR_EDICT(ent) - 1;
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP, "%s to a non-client %d", __func__, clientNum);
    }

    client = svs.client_pool + clientNum;
    if (client->state <= cs_zombie) {
        Com_WPrintf("%s to a free/zombie client %d\n", __func__, clientNum);
        return;
    }

    MSG_WriteByte(svc_print);
    MSG_WriteByte(level);
    MSG_WriteData(msg, strlen(msg) + 1);

    if (level >= client->messagelevel) {
        SV_ClientAddMessage(client, MSG_RELIABLE);
    }

    SV_MvdUnicast(ent, clientNum, true);

    SZ_Clear(&msg_write);
}

/*
===============
PF_centerprintf

Centerprint to a single client.
Archived in MVD stream.
===============
*/
static void PF_Center_Print(edict_t *ent, const char *msg)
{
    int         n;

    if (!ent) {
        return;
    }

    n = NUM_FOR_EDICT(ent);
    if (n < 1 || n > sv_maxclients->integer) {
        Com_WPrintf("%s to a non-client %d\n", __func__, n - 1);
        return;
    }

    MSG_WriteByte(svc_centerprint);
    MSG_WriteData(msg, strlen(msg) + 1);

    PF_Unicast(ent, true, 0);
}

/*
===============
PF_error

Abort the server with a game error
===============
*/
static q_noreturn void PF_error(const char *msg)
{
    Com_Error(ERR_DROP, "Game Error: %s", msg);
}

static trace_t PF_Clip(edict_t *entity, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentmask)
{
    return SV_Clip(start, mins, maxs, end, entity, contentmask);
}

/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
static void PF_setmodel(edict_t *ent, const char *name)
{
    mmodel_t    *mod;

    if (!ent || !name)
        Com_Error(ERR_DROP, "PF_setmodel: NULL");

    ent->s.modelindex = PF_ModelIndex(name);

// if it is an inline model, get the size information for it
    if (name[0] == '*') {
        mod = CM_InlineModel(&sv.cm, name);
        VectorCopy(mod->mins, ent->mins);
        VectorCopy(mod->maxs, ent->maxs);
        PF_LinkEdict(ent);
    }
}

/*
===============
PF_configstring

If game is actively running, broadcasts configstring change.
Archived in MVD stream.
===============
*/
static void PF_configstring(int index, const char *val)
{
    size_t len, maxlen;
    client_t *client;
    char *dst;

    if (index < 0 || index >= svs.csr.end)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);

    if (sv.state == ss_dead) {
        Com_WPrintf("%s: not yet initialized\n", __func__);
        return;
    }

    if (!val)
        val = "";

    // error out entirely if it exceedes array bounds
    len = strlen(val);
    maxlen = (svs.csr.end - index) * MAX_QPATH;
    if (len >= maxlen) {
        Com_Error(ERR_DROP,
                  "%s: index %d overflowed: %zu > %zu",
                  __func__, index, len, maxlen - 1);
    }

    // print a warning and truncate everything else
    maxlen = CS_SIZE(&svs.csr, index);
    if (len >= maxlen) {
        Com_WPrintf(
            "%s: index %d overflowed: %zu > %zu\n",
            __func__, index, len, maxlen - 1);
        len = maxlen - 1;
    }

    dst = sv.configstrings[index];
    if (!strncmp(dst, val, maxlen)) {
        return;
    }

    // change the string in sv
    memcpy(dst, val, len);
    dst[len] = 0;

    if (sv.state == ss_loading) {
        return;
    }

    SV_MvdConfigstring(index, val, len);

    // send the update to everyone
    MSG_WriteByte(svc_configstring);
    MSG_WriteShort(index);
    MSG_WriteData(val, len);
    MSG_WriteByte(0);

    FOR_EACH_CLIENT(client) {
        if (client->state < cs_primed) {
            continue;
        }
        SV_ClientAddMessage(client, MSG_RELIABLE);
    }

    SZ_Clear(&msg_write);
}

static const char *PF_GetConfigstring(int index)
{
    if (index < 0 || index >= svs.csr.end)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);

    return sv.configstrings[index];
}

static void PF_WriteFloat(float f)
{
    Com_Error(ERR_DROP, "PF_WriteFloat not implemented");
}

typedef enum {
    VIS_PVS     = 0,
    VIS_PHS     = 1,
    VIS_NOAREAS = 2     // can be OR'ed with one of above
} vis_t;

static qboolean PF_inVIS(const vec3_t p1, const vec3_t p2, vis_t vis)
{
    mleaf_t *leaf1, *leaf2;
    byte mask[VIS_MAX_BYTES];

    leaf1 = CM_PointLeaf(&sv.cm, p1);
    BSP_ClusterVis(sv.cm.cache, mask, leaf1->cluster, vis & VIS_PHS);

    leaf2 = CM_PointLeaf(&sv.cm, p2);
    if (leaf2->cluster == -1)
        return false;
    if (!Q_IsBitSet(mask, leaf2->cluster))
        return false;
    if (!(vis & VIS_NOAREAS) && !CM_AreasConnected(&sv.cm, leaf1->area, leaf2->area))
        return false;       // a door blocks it
    return true;
}


/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
static qboolean PF_inPVS(const vec3_t p1, const vec3_t p2, bool portals)
{
    return PF_inVIS(p1, p2, VIS_PVS | (portals ? 0 : VIS_NOAREAS));
}

/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
static qboolean PF_inPHS(const vec3_t p1, const vec3_t p2, bool portals)
{
    return PF_inVIS(p1, p2, VIS_PHS | (portals ? 0 : VIS_NOAREAS));
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

If channel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Timeofs can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/
static void SV_StartSound(const vec3_t origin, edict_t *edict,
                          int channel, int soundindex, float volume,
                          float attenuation, float timeofs)
{
    int         i, ent, vol, att, ofs, flags, sendchan;
    vec3_t      origin_v;
    client_t    *client;
    byte        mask[VIS_MAX_BYTES];
    mleaf_t     *leaf1, *leaf2;
    message_packet_t    *msg;
    bool        force_pos;

    if (!edict)
        Com_Error(ERR_DROP, "%s: edict = NULL", __func__);
    if (volume < 0 || volume > 1)
        Com_Error(ERR_DROP, "%s: volume = %f", __func__, volume);
    if (attenuation < 0 || attenuation > 4)
        Com_Error(ERR_DROP, "%s: attenuation = %f", __func__, attenuation);
    if (timeofs < 0 || timeofs > 0.255f)
        Com_Error(ERR_DROP, "%s: timeofs = %f", __func__, timeofs);
    if (soundindex < 0 || soundindex >= svs.csr.max_sounds)
        Com_Error(ERR_DROP, "%s: soundindex = %d", __func__, soundindex);

    vol = volume * 255;
    att = min(attenuation * 64, 255);   // need to clip due to check above
    ofs = timeofs * 1000;

    ent = NUM_FOR_EDICT(edict);

    sendchan = (ent << 3) | (channel & 7);

    // always send the entity number for channel overrides
    flags = SND_ENT;
    if (vol != 255)
        flags |= SND_VOLUME;
    if (att != 64)
        flags |= SND_ATTENUATION;
    if (ofs)
        flags |= SND_OFFSET;
    if (soundindex > 255)
        flags |= SND_INDEX16;

    // send origin for invisible entities
    // the origin can also be explicitly set
    force_pos = (edict->svflags & SVF_NOCLIENT) || origin;

    // use the entity origin unless it is a bmodel or explicitly specified
    if (!origin) {
        if (edict->solid == SOLID_BSP) {
            VectorAvg(edict->mins, edict->maxs, origin_v);
            VectorAdd(origin_v, edict->s.origin, origin_v);
            origin = origin_v;
        } else {
            origin = edict->s.origin;
        }
    }

    // prepare multicast message
    MSG_WriteByte(svc_sound);
    MSG_WriteByte(flags | SND_POS);
    if (flags & SND_INDEX16)
        MSG_WriteShort(soundindex);
    else
        MSG_WriteByte(soundindex);

    if (flags & SND_VOLUME)
        MSG_WriteByte(vol);
    if (flags & SND_ATTENUATION)
        MSG_WriteByte(att);
    if (flags & SND_OFFSET)
        MSG_WriteByte(ofs);

    MSG_WriteShort(sendchan);
    MSG_WritePos(origin);

    // if the sound doesn't attenuate, send it to everyone
    // (global radio chatter, voiceovers, etc)
    if (attenuation == ATTN_NONE)
        channel |= CHAN_NO_PHS_ADD;

    // multicast if force sending origin
    if (force_pos) {
        if (channel & CHAN_NO_PHS_ADD) {
            SV_Multicast(NULL, MULTICAST_ALL, channel & CHAN_RELIABLE);
        } else {
            SV_Multicast(origin, MULTICAST_PHS, channel & CHAN_RELIABLE);
        }
        return;
    }

    leaf1 = NULL;
    if (!(channel & CHAN_NO_PHS_ADD)) {
        leaf1 = CM_PointLeaf(&sv.cm, origin);
        BSP_ClusterVis(sv.cm.cache, mask, leaf1->cluster, DVIS_PHS);
    }

    // decide per client if origin needs to be sent
    FOR_EACH_CLIENT(client) {
        // do not send sounds to connecting clients
        if (!CLIENT_ACTIVE(client)) {
            continue;
        }

        // PHS cull this sound
        if (!(channel & CHAN_NO_PHS_ADD)) {
            leaf2 = CM_PointLeaf(&sv.cm, client->edict->s.origin);
            if (!CM_AreasConnected(&sv.cm, leaf1->area, leaf2->area))
                continue;
            if (leaf2->cluster == -1)
                continue;
            if (!Q_IsBitSet(mask, leaf2->cluster))
                continue;
        }

        // reliable sounds will always have position explicitly set,
        // as no one guarantees reliables to be delivered in time
        if (channel & CHAN_RELIABLE) {
            SV_ClientAddMessage(client, MSG_RELIABLE);
            continue;
        }

        // default client doesn't know that bmodels have weird origins
        if (edict->solid == SOLID_BSP && client->protocol == PROTOCOL_VERSION_DEFAULT) {
            SV_ClientAddMessage(client, 0);
            continue;
        }

        if (LIST_EMPTY(&client->msg_free_list)) {
            Com_WPrintf("%s: %s: out of message slots\n",
                        __func__, client->name);
            continue;
        }

        msg = LIST_FIRST(message_packet_t, &client->msg_free_list, entry);

        msg->cursize = 0;
        msg->flags = flags;
        msg->index = soundindex;
        msg->volume = vol;
        msg->attenuation = att;
        msg->timeofs = ofs;
        msg->sendchan = sendchan;
        for (i = 0; i < 3; i++) {
            msg->pos[i] = COORD2SHORT(origin[i]);
        }

        List_Remove(&msg->entry);
        List_Append(&client->msg_unreliable_list, &msg->entry);
        client->msg_unreliable_bytes += MAX_SOUND_PACKET;
    }

    // clear multicast buffer
    SZ_Clear(&msg_write);

    SV_MvdStartSound(ent, channel, flags, soundindex, vol, att, ofs);
}

static void PF_StartSound(edict_t *entity, int channel,
                          int soundindex, float volume,
                          float attenuation, float timeofs)
{
    if (!entity)
        return;
    SV_StartSound(NULL, entity, channel, soundindex, volume, attenuation, timeofs);
}

// TODO: support origin/entity/volume/attenuation/timeofs
static void PF_LocalSound(edict_t *target, const vec3_t origin,
                          edict_t *entity, int channel,
                          int soundindex, float volume,
                          float attenuation, float timeofs,
                          uint32_t dupe_key)
{
    int entnum = NUM_FOR_EDICT(target);
    int sendchan = (entnum << 3) | (channel & 7);
    int flags = SND_ENT;

    if (svs.csr.extended && soundindex > 255)
        flags |= SND_INDEX16;

    MSG_WriteByte(svc_sound);
    MSG_WriteByte(flags);
    if (flags & SND_INDEX16)
        MSG_WriteShort(soundindex);
    else
        MSG_WriteByte(soundindex);
    MSG_WriteShort(sendchan);

    PF_Unicast(target, !!(channel & CHAN_RELIABLE), dupe_key);
}

void PF_Pmove(pmove_t *pm)
{
    if (sv_client) {
        Pmove(pm, &sv_client->pmp);
    } else {
        Pmove(pm, &sv_pmp);
    }
}

static void PF_WriteEntity(const edict_t *entity)
{
    MSG_WriteShort(NUM_FOR_EDICT(entity));
}

static cvar_t *PF_cvar(const char *name, const char *value, int flags)
{
    if (flags & CVAR_EXTENDED_MASK) {
        Com_WPrintf("Game attemped to set extended flags on '%s', masked out.\n", name);
        flags &= ~CVAR_EXTENDED_MASK;
    }

    return Cvar_Get(name, value, flags | CVAR_GAME);
}

static const char* PF_Argv(int idx)
{
    return Cmd_Argv(idx);
}

static const char* PF_RawArgs(void)
{
    return Cmd_RawArgs();
}

static void PF_AddCommandString(const char *string)
{
#if USE_CLIENT
    if (!strcmp(string, "menu_loadgame\n"))
        string = "pushmenu loadgame\n";
#endif
    Cbuf_AddText(&cmd_buffer, string);
}

static void PF_SetAreaPortalState(int portalnum, bool open)
{
    CM_SetAreaPortalState(&sv.cm, portalnum, open);
}

static qboolean PF_AreasConnected(int area1, int area2)
{
    return CM_AreasConnected(&sv.cm, area1, area2);
}

static void *PF_TagMalloc(size_t size, int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    return Z_TagMallocz(size, tag + TAG_MAX);
}

static void PF_FreeTags(int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    Z_FreeTags(tag + TAG_MAX);
}

static void PF_DebugGraph(float value, int color)
{
}

static int PF_LoadFile(const char *path, void **buffer, unsigned flags, unsigned tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    return FS_LoadFileEx(path, buffer, flags, tag + TAG_MAX);
}

static void *PF_TagRealloc(void *ptr, size_t size)
{
    if (!ptr && size) {
        Com_Error(ERR_DROP, "%s: untagged allocation not allowed", __func__);
    }
    return Z_Realloc(ptr, size);
}

static void *PF_GetExtension(const char *name);

static void PF_ReportMatchDetails_Multicast(bool is_end)
{
    /* "This function is solely for platforms that need match result data." -
     * somewhat unclear...
     * Anyhow, rerelease game writes some message data prior to calling this,
     * at least discard that data ... */
    SZ_Clear(&msg_write);
}

static uint32_t PF_ServerFrame(void)
{
    return sv.framenum;
}

static void PF_SendToClipboard(const char* text)
{
#if USE_CLIENT
    if(vid.set_clipboard_data)
        vid.set_clipboard_data(text);
#endif
}

static size_t PF_Info_ValueForKey (const char *s, const char *key, char *buffer, size_t buffer_len)
{
    char *infostr = Info_ValueForKey(s, key);
    return Q_strlcpy(buffer, infostr, buffer_len);
}

//==============================================

static const game_import_t game_import = {
    .multicast = SV_Multicast,
    .unicast = PF_Unicast,
    .Broadcast_Print = PF_Broadcast_Print,
    .Com_Print = PF_Com_Print,
    .Client_Print = PF_Client_Print,
    .Center_Print = PF_Center_Print,
    .Com_Error = PF_error,

    .linkentity = PF_LinkEdict,
    .unlinkentity = PF_UnlinkEdict,
    .BoxEdicts = SV_AreaEdicts,
    .trace = SV_Trace,
    .clip = PF_Clip,
    .pointcontents = SV_PointContents,
    .setmodel = PF_setmodel,
    .inPVS = PF_inPVS,
    .inPHS = PF_inPHS,

    .modelindex = PF_ModelIndex,
    .soundindex = PF_SoundIndex,
    .imageindex = PF_ImageIndex,

    .configstring = PF_configstring,
    .get_configstring = PF_GetConfigstring,
    .sound = PF_StartSound,
    .positioned_sound = SV_StartSound,
    .local_sound = PF_LocalSound,

    .WriteChar = MSG_WriteChar,
    .WriteByte = MSG_WriteByte,
    .WriteShort = MSG_WriteShort,
    .WriteLong = MSG_WriteLong,
    .WriteFloat = PF_WriteFloat,
    .WriteString = MSG_WriteString,
    .WritePosition = MSG_WritePos,
    .WriteDir = MSG_WriteDir,
    .WriteAngle = MSG_WriteAngle,
    .WriteEntity = PF_WriteEntity,

    .TagMalloc = PF_TagMalloc,
    .TagFree = Z_Free,
    .FreeTags = PF_FreeTags,

    .cvar = PF_cvar,
    .cvar_set = Cvar_UserSet,
    .cvar_forceset = Cvar_Set,

    .argc = Cmd_Argc,
    .argv = PF_Argv,
    .args = PF_RawArgs,
    .AddCommandString = PF_AddCommandString,

    .DebugGraph = PF_DebugGraph,
    .SetAreaPortalState = PF_SetAreaPortalState,
    .AreasConnected = PF_AreasConnected,
    .GetExtension = PF_GetExtension,

    .ReportMatchDetails_Multicast = PF_ReportMatchDetails_Multicast,
    .ServerFrame = PF_ServerFrame,
    .SendToClipBoard = PF_SendToClipboard,

    .Info_ValueForKey = PF_Info_ValueForKey,
    .Info_RemoveKey = Info_RemoveKey,
    .Info_SetValueForKey = Info_SetValueForKey,
};

static const filesystem_api_v1_t filesystem_api_v1 = {
    .OpenFile = FS_OpenFile,
    .CloseFile = FS_CloseFile,
    .LoadFile = PF_LoadFile,

    .ReadFile = FS_Read,
    .WriteFile = FS_Write,
    .FlushFile = FS_Flush,
    .TellFile = FS_Tell,
    .SeekFile = FS_Seek,
    .ReadLine = FS_ReadLine,

    .ListFiles = FS_ListFiles,
    .FreeFileList = FS_FreeList,

    .ErrorString = Q_ErrorString,
};

static void *PF_GetExtension(const char *name)
{
    if (!name)
        return NULL;
    if (!strcmp(name, "FILESYSTEM_API_V1"))
        return (void *)&filesystem_api_v1;
    return NULL;
}

static const game_import_ex_t game_import_ex = {
    .apiversion = GAME_API_VERSION_EX,
    .structsize = sizeof(game_import_ex),

    .TagRealloc = PF_TagRealloc,
};

static void *game_library;

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs(void)
{
    gex = NULL;
    if (ge) {
        ge->Shutdown();
        ge = NULL;
    }
    if (game_library) {
        Sys_FreeLibrary(game_library);
        game_library = NULL;
    }
    Cvar_Set("g_features", "0");

    Z_LeakTest(TAG_FREE);
}

static void *SV_LoadGameLibraryFrom(const char *path)
{
    void *entry;

    entry = Sys_LoadLibrary(path, "GetGameAPI", &game_library);
    if (!entry)
        Com_EPrintf("Failed to load game library: %s\n", Com_GetLastError());
    else
        Com_Printf("Loaded game library from %s\n", path);

    return entry;
}

static void *SV_LoadGameLibrary(const char *libdir, const char *gamedir)
{
    char path[MAX_OSPATH];

    if (Q_concat(path, sizeof(path), libdir,
                 PATH_SEP_STRING, gamedir, PATH_SEP_STRING,
                 "game" CPUSTRING LIBSUFFIX) >= sizeof(path)) {
        Com_EPrintf("Game library path length exceeded\n");
        return NULL;
    }

    if (os_access(path, X_OK)) {
        Com_Printf("Can't access %s: %s\n", path, strerror(errno));
        return NULL;
    }

    return SV_LoadGameLibraryFrom(path);
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SV_InitGameProgs(void)
{
    game_import_t   import;
    game_entry_t    entry = NULL;

    // unload anything we have now
    SV_ShutdownGameProgs();

    // for debugging or `proxy' mods
    if (sys_forcegamelib->string[0])
        entry = SV_LoadGameLibraryFrom(sys_forcegamelib->string);

    // try game first
    if (!entry && fs_game->string[0]) {
        if (sys_homedir->string[0])
            entry = SV_LoadGameLibrary(sys_homedir->string, fs_game->string);
        if (!entry)
            entry = SV_LoadGameLibrary(sys_libdir->string, fs_game->string);
    }

    // then try baseq2
    if (!entry) {
        if (sys_homedir->string[0])
            entry = SV_LoadGameLibrary(sys_homedir->string, BASEGAME);
        if (!entry)
            entry = SV_LoadGameLibrary(sys_libdir->string, BASEGAME);
    }

    // all paths failed
    if (!entry)
        Com_Error(ERR_DROP, "Failed to load game library");

    // load a new game dll
    import = game_import;

    ge = entry(&import);
    if (!ge) {
        Com_Error(ERR_DROP, "Game library returned NULL exports");
    }
    // get extended api if present
    game_entry_ex_t entry_ex = Sys_GetProcAddress(game_library, "GetGameAPIEx");

    if (ge->apiversion != GAME_API_VERSION) {
        if (ge->apiversion == 3) {
            Com_DPrintf("Detected version 3 game library, using proxy game\n");
            ge = GetGame3Proxy(&import, &game_import_ex, entry, entry_ex);
        } else {
            Com_Error(ERR_DROP, "Game library is version %d, expected %d",
                      ge->apiversion, GAME_API_VERSION);
        }
    }

    if (entry_ex)
        gex = entry_ex(&game_import_ex);

    // initialize
    ge->Init();

    if (g_features->integer & GMF_PROTOCOL_EXTENSIONS) {
        Com_Printf("Game supports Q2PRO protocol extensions.\n");
        svs.csr = cs_remap_new;
    }

    // sanitize edict_size
    unsigned min_size = svs.csr.extended ? sizeof(edict_t) : q_offsetof(edict_t, x);
    unsigned max_size = INT_MAX / svs.csr.max_edicts;

    if (ge->edict_size < min_size || ge->edict_size > max_size || ge->edict_size % q_alignof(edict_t)) {
        Com_Error(ERR_DROP, "Game library returned bad size of edict_t");
    }

    // sanitize max_edicts
    if (ge->max_edicts <= sv_maxclients->integer || ge->max_edicts > svs.csr.max_edicts) {
        Com_Error(ERR_DROP, "Game library returned bad number of max_edicts");
    }
}
