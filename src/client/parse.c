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
// cl_parse.c  -- parse a message received from the server

#include "client.h"
#include "shared/m_flash.h"

/*
=====================================================================

  DELTA FRAME PARSING

=====================================================================
*/

static void CL_ParseDeltaEntity(server_frame_t           *frame,
                                int                      newnum,
                                const entity_state_t     *old,
                                uint64_t                 bits)
{
    entity_state_t     *state;

    // suck up to MAX_EDICTS for servers that don't cap at MAX_PACKET_ENTITIES
    if (frame->numEntities >= cl.csr.max_edicts) {
        Com_Error(ERR_DROP, "%s: too many entities", __func__);
    }

    state = &cl.entityStates[cl.numEntityStates & PARSE_ENTITIES_MASK];
    cl.numEntityStates++;
    frame->numEntities++;

#if USE_DEBUG
    if (cl_shownet->integer > 2 && bits) {
        MSG_ShowDeltaEntityBits(bits);
        Com_LPrintf(PRINT_DEVELOPER, "\n");
    }
#endif

    *state = *old;
    MSG_ParseDeltaEntity(state, newnum, bits, cl.esFlags);

    // shuffle previous origin to old
    if (!(bits & U_OLDORIGIN) && !(state->renderfx & RF_BEAM))
        VectorCopy(old->origin, state->old_origin);

    // make sure extended indices don't overflow
    if ((state->modelindex | state->modelindex2 | state->modelindex3 | state->modelindex4) >= cl.csr.max_models)
        Com_Error(ERR_DROP, "%s: bad modelindex", __func__);

    if (state->sound >= cl.csr.max_sounds)
        Com_Error(ERR_DROP, "%s: bad sound", __func__);

    // mask off high bits for non-extended servers
    if (!cl.csr.extended) {
        state->renderfx &= RF_SHELL_LITE_GREEN - 1;
        if (state->renderfx & RF_BEAM)
            state->renderfx &= ~RF_GLOW;
    }
}

static void CL_ParsePacketEntities(server_frame_t *oldframe,
                                   server_frame_t *frame)
{
    uint64_t        bits;
    entity_state_t  *oldstate;
    int             i, oldindex, oldnum, newnum;

    frame->firstEntity = cl.numEntityStates;
    frame->numEntities = 0;

    // delta from the entities present in oldframe
    oldindex = 0;
    oldstate = NULL;
    if (!oldframe) {
        oldnum = MAX_EDICTS;
    } else {
        if (oldindex >= oldframe->numEntities) {
            oldnum = MAX_EDICTS;
        } else {
            i = oldframe->firstEntity + oldindex;
            oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
            oldnum = oldstate->number;
        }
    }

    while (1) {
        newnum = MSG_ParseEntityBits(&bits, cl.esFlags);
        if (newnum < 0 || newnum >= cl.csr.max_edicts) {
            Com_Error(ERR_DROP, "%s: bad number: %d", __func__, newnum);
        }

        if (!newnum) {
            break;
        }

        while (oldnum < newnum) {
            // one or more entities from the old packet are unchanged
            SHOWNET(3, "   unchanged: %i\n", oldnum);
            CL_ParseDeltaEntity(frame, oldnum, oldstate, 0);

            oldindex++;

            if (oldindex >= oldframe->numEntities) {
                oldnum = MAX_EDICTS;
            } else {
                i = oldframe->firstEntity + oldindex;
                oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
                oldnum = oldstate->number;
            }
        }

        if (bits & U_REMOVE) {
            // the entity present in oldframe is not in the current frame
            SHOWNET(2, "   remove: %i\n", newnum);
            if (oldnum != newnum) {
                Com_DPrintf("U_REMOVE: oldnum != newnum\n");
            }
            if (!oldframe) {
                Com_Error(ERR_DROP, "%s: U_REMOVE with NULL oldframe", __func__);
            }

            oldindex++;

            if (oldindex >= oldframe->numEntities) {
                oldnum = MAX_EDICTS;
            } else {
                i = oldframe->firstEntity + oldindex;
                oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
                oldnum = oldstate->number;
            }
            continue;
        }

        if (oldnum == newnum) {
            // delta from previous state
            SHOWNET(2, "   delta: %i ", newnum);
            CL_ParseDeltaEntity(frame, newnum, oldstate, bits);
            if (!bits) {
                SHOWNET(2, "\n");
            }

            oldindex++;

            if (oldindex >= oldframe->numEntities) {
                oldnum = MAX_EDICTS;
            } else {
                i = oldframe->firstEntity + oldindex;
                oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
                oldnum = oldstate->number;
            }
            continue;
        }

        if (oldnum > newnum) {
            // delta from baseline
            SHOWNET(2, "   baseline: %i ", newnum);
            CL_ParseDeltaEntity(frame, newnum, &cl.baselines[newnum], bits);
            if (!bits) {
                SHOWNET(2, "\n");
            }
            continue;
        }

    }

    // any remaining entities in the old frame are copied over
    while (oldnum != MAX_EDICTS) {
        // one or more entities from the old packet are unchanged
        SHOWNET(3, "   unchanged: %i\n", oldnum);
        CL_ParseDeltaEntity(frame, oldnum, oldstate, 0);

        oldindex++;

        if (oldindex >= oldframe->numEntities) {
            oldnum = MAX_EDICTS;
        } else {
            i = oldframe->firstEntity + oldindex;
            oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
            oldnum = oldstate->number;
        }
    }
}

static void CL_ParseFrame(int extrabits)
{
    uint32_t bits, extraflags;
    int     currentframe, deltaframe,
            delta, suppressed;
    server_frame_t  frame, *oldframe;
    player_state_t  *from;
    int     length;

    memset(&frame, 0, sizeof(frame));

    cl.frameflags = 0;

    extraflags = 0;
    if (cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) {
        bits = MSG_ReadLong();

        currentframe = bits & FRAMENUM_MASK;
        delta = bits >> FRAMENUM_BITS;

        if (delta == 31) {
            deltaframe = -1;
        } else {
            deltaframe = currentframe - delta;
        }

        bits = MSG_ReadByte();

        suppressed = bits & SUPPRESSCOUNT_MASK;
        if (cls.serverProtocol == PROTOCOL_VERSION_Q2PRO || cls.serverProtocol == PROTOCOL_VERSION_RERELEASE) {
            if (suppressed & FF_CLIENTPRED) {
                // CLIENTDROP is implied, don't draw both
                suppressed &= ~FF_CLIENTDROP;
            }
            cl.frameflags |= suppressed;
        } else if (suppressed) {
            cl.frameflags |= FF_SUPPRESSED;
        }

        if (cls.serverProtocol == PROTOCOL_VERSION_RERELEASE)
            extraflags = MSG_ReadByte();
        else
            extraflags = (extrabits << 4) | (bits >> SUPPRESSCOUNT_BITS);
    } else {
        currentframe = MSG_ReadLong();
        deltaframe = MSG_ReadLong();

        // BIG HACK to let old demos continue to work
        if (cls.serverProtocol != PROTOCOL_VERSION_OLD) {
            suppressed = MSG_ReadByte();
            if (suppressed) {
                cl.frameflags |= FF_SUPPRESSED;
            }
        }
    }

    frame.number = currentframe;
    frame.delta = deltaframe;

    if (cls.netchan.dropped) {
        cl.frameflags |= FF_SERVERDROP;
    }

    // if the frame is delta compressed from data that we no longer have
    // available, we must suck up the rest of the frame, but not use it, then
    // ask for a non-compressed message
    if (deltaframe > 0) {
        oldframe = &cl.frames[deltaframe & UPDATE_MASK];
        from = &oldframe->ps;
        if (deltaframe == currentframe) {
            // old servers may cause this on map change
            Com_DPrintf("%s: delta from current frame\n", __func__);
            cl.frameflags |= FF_BADFRAME;
        } else if (oldframe->number != deltaframe) {
            // the frame that the server did the delta from
            // is too old, so we can't reconstruct it properly.
            Com_DPrintf("%s: delta frame was never received or too old\n", __func__);
            cl.frameflags |= FF_OLDFRAME;
        } else if (!oldframe->valid) {
            // should never happen
            Com_DPrintf("%s: delta from invalid frame\n", __func__);
            cl.frameflags |= FF_BADFRAME;
        } else if (cl.numEntityStates - oldframe->firstEntity >
                   MAX_PARSE_ENTITIES - MAX_PACKET_ENTITIES) {
            Com_DPrintf("%s: delta entities too old\n", __func__);
            cl.frameflags |= FF_OLDENT;
        } else {
            frame.valid = true; // valid delta parse
        }
        if (!frame.valid && cl.frame.valid && cls.demo.playback) {
            Com_DPrintf("%s: recovering broken demo\n", __func__);
            oldframe = &cl.frame;
            from = &oldframe->ps;
            frame.valid = true;
        }
    } else {
        oldframe = NULL;
        from = NULL;
        frame.valid = true; // uncompressed frame
        cl.frameflags |= FF_NODELTA;
    }

    // read areabits
    length = MSG_ReadByte();
    if (length) {
        if (length > sizeof(frame.areabits)) {
            Com_Error(ERR_DROP, "%s: invalid areabits length", __func__);
        }
        memcpy(frame.areabits, MSG_ReadData(length), length);
        frame.areabytes = length;
    } else {
        frame.areabytes = 0;
    }

    if (cls.serverProtocol <= PROTOCOL_VERSION_DEFAULT) {
        if (MSG_ReadByte() != svc_playerinfo) {
            Com_Error(ERR_DROP, "%s: not playerinfo", __func__);
        }
    }

    SHOWNET(2, "%3u:playerinfo\n", msg_read.readcount - 1);

    // parse playerstate
    bits = MSG_ReadWord();
    if (cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) {
        MSG_ParseDeltaPlayerstate_Enhanced(from, &frame.ps, bits, extraflags, cl.psFlags);
#if USE_DEBUG
        if (cl_shownet->integer > 2 && (bits || extraflags)) {
            Com_LPrintf(PRINT_DEVELOPER, "   ");
            MSG_ShowDeltaPlayerstateBits_Enhanced(bits, extraflags);
            Com_LPrintf(PRINT_DEVELOPER, "\n");
        }
#endif
        if (cls.serverProtocol == PROTOCOL_VERSION_Q2PRO) {
            // parse clientNum
            if (extraflags & EPS_CLIENTNUM) {
                if (cls.protocolVersion < PROTOCOL_VERSION_Q2PRO_CLIENTNUM_SHORT) {
                    frame.clientNum = MSG_ReadByte();
                } else {
                    frame.clientNum = MSG_ReadShort();
                }
                if (!VALIDATE_CLIENTNUM(&cl.csr, frame.clientNum)) {
                    Com_Error(ERR_DROP, "%s: bad clientNum", __func__);
                }
            } else if (oldframe) {
                frame.clientNum = oldframe->clientNum;
            }
        } else {
            frame.clientNum = cl.clientNum;
        }
    } else {
        MSG_ParseDeltaPlayerstate_Default(from, &frame.ps, bits, cl.psFlags);
#if USE_DEBUG
        if (cl_shownet->integer > 2 && bits) {
            Com_LPrintf(PRINT_DEVELOPER, "   ");
            MSG_ShowDeltaPlayerstateBits_Default(bits);
            Com_LPrintf(PRINT_DEVELOPER, "\n");
        }
#endif
        frame.clientNum = cl.clientNum;
    }

    // parse packetentities
    if (cls.serverProtocol <= PROTOCOL_VERSION_DEFAULT) {
        if (MSG_ReadByte() != svc_packetentities) {
            Com_Error(ERR_DROP, "%s: not packetentities", __func__);
        }
    }

    SHOWNET(2, "%3u:packetentities\n", msg_read.readcount - 1);

    CL_ParsePacketEntities(oldframe, &frame);

    // save the frame off in the backup array for later delta comparisons
    cl.frames[currentframe & UPDATE_MASK] = frame;

#if USE_DEBUG
    if (cl_shownet->integer > 2) {
        int seq = cls.netchan.incoming_acknowledged & CMD_MASK;
        int rtt = cls.demo.playback ? 0 : cls.realtime - cl.history[seq].sent;
        Com_LPrintf(PRINT_DEVELOPER, "%3u:frame:%d  delta:%d  rtt:%d\n",
                    msg_read.readcount - 1, frame.number, frame.delta, rtt);
    }
#endif

    if (!frame.valid) {
        cl.frame.valid = false;
#if USE_FPS
        cl.keyframe.valid = false;
#endif
        return; // do not change anything
    }

    if (!frame.ps.fov) {
        // fail out early to prevent spurious errors later
        Com_Error(ERR_DROP, "%s: bad fov", __func__);
    }

    if (cls.state < ca_precached)
        return;

    cl.oldframe = cl.frame;
    cl.frame = frame;

#if USE_FPS
    if (CL_FRAMESYNC) {
        cl.oldkeyframe = cl.keyframe;
        cl.keyframe = cl.frame;
    }
#endif

    cls.demo.frames_read++;

    if (!cls.demo.seeking)
        CL_DeltaFrame();
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

static void CL_ParseConfigstring(int index)
{
    size_t  len, maxlen;
    char    *s;

    if (index < 0 || index >= cl.csr.end) {
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);
    }

    s = cl.configstrings[index];
    maxlen = CS_SIZE(&cl.csr, index);
    len = MSG_ReadString(s, maxlen);

    SHOWNET(2, "    %d \"%s\"\n", index, Com_MakePrintable(s));

    if (len >= maxlen) {
        Com_WPrintf(
            "%s: index %d overflowed: %zu > %zu\n",
            __func__, index, len, maxlen - 1);
    }

    if (cls.demo.seeking) {
        Q_SetBit(cl.dcs, index);
        return;
    }

    if (cls.demo.recording && cls.demo.paused) {
        Q_SetBit(cl.dcs, index);
    }

    // do something apropriate
    CL_UpdateConfigstring(index);
}

static void CL_ParseBaseline(int index, uint64_t bits)
{
    entity_state_t *base;

    if (index < 1 || index >= cl.csr.max_edicts) {
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);
    }

#if USE_DEBUG
    if (cl_shownet->integer > 2) {
        Com_LPrintf(PRINT_DEVELOPER, "   baseline: %i ", index);
        MSG_ShowDeltaEntityBits(bits);
        Com_LPrintf(PRINT_DEVELOPER, "\n");
    }
#endif

    base = &cl.baselines[index];
    MSG_ParseDeltaEntity(base, index, bits, cl.esFlags);
}

// instead of wasting space for svc_configstring and svc_spawnbaseline
// bytes, entire game state is compressed into a single stream.
static void CL_ParseGamestateQ2PRO(int cmd)
{
    int         index;
    uint64_t    bits;

    if (cmd == svc_q2pro_gamestate || cmd == svc_q2pro_configstringstream) {
        while (1) {
            index = MSG_ReadWord();
            if (index == cl.csr.end) {
                break;
            }
            CL_ParseConfigstring(index);
        }
    }

    if (cmd == svc_q2pro_gamestate || cmd == svc_q2pro_baselinestream) {
        while (1) {
            index = MSG_ParseEntityBits(&bits, cl.esFlags);
            if (!index) {
                break;
            }
            CL_ParseBaseline(index, bits);
        }
    }
}

static void CL_ParseGamestateRR(int cmd)
{
    int         index;
    uint64_t    bits;

    if (cmd == svc_rr_gamestate || cmd == svc_rr_configstringstream) {
        while (1) {
            index = MSG_ReadWord();
            if (index == cl.csr.end) {
                break;
            }
            CL_ParseConfigstring(index);
        }
    }

    if (cmd == svc_rr_gamestate || cmd == svc_rr_baselinestream) {
        while (1) {
            index = MSG_ParseEntityBits(&bits, cl.esFlags);
            if (!index) {
                break;
            }
            CL_ParseBaseline(index, bits);
        }
    }
}

static void read_q2pro_protocol_flags(void)
{
    int i = MSG_ReadWord();
    if (i & Q2PRO_PF_STRAFEJUMP_HACK) {
        Com_DPrintf("Q2PRO strafejump hack enabled\n");
        cl.pmp.strafehack = true;
    }
    if (i & Q2PRO_PF_QW_MODE) {
        Com_DPrintf("Q2PRO QW mode enabled\n");
        PmoveEnableQW(&cl.pmp);
    }
    if (i & Q2PRO_PF_WATERJUMP_HACK) {
        Com_DPrintf("Q2PRO waterjump hack enabled\n");
        cl.pmp.waterhack = true;
    }
    if (i & Q2PRO_PF_EXTENSIONS) {
        Com_DPrintf("Q2PRO protocol extensions enabled\n");
        cl.csr = cs_remap_q2pro_new;
    }
}

static void CL_ParseServerData(void)
{
    char    levelname[MAX_QPATH];
    int     i, protocol, attractloop q_unused;
    bool    cinematic;

    Cbuf_Execute(&cl_cmdbuf);          // make sure any stuffed commands are done

    // wipe the client_state_t struct
    CL_ClearState();

    // parse protocol version number
    protocol = MSG_ReadLong();
    cl.servercount = MSG_ReadLong();
    attractloop = MSG_ReadByte();
    
    Com_DPrintf("Serverdata packet received "
                "(protocol=%d, servercount=%d, attractloop=%d)\n",
                protocol, cl.servercount, attractloop);

    cl.csr = cs_remap_old;

    // check protocol
    if (cls.serverProtocol != protocol) {
        if (!cls.demo.playback) {
            Com_Error(ERR_DROP, "Requested protocol version %d, but server returned %d.",
                      cls.serverProtocol, protocol);
        }
        // BIG HACK to let demos from release work with the 3.0x patch!!!
        if (protocol == PROTOCOL_VERSION_EXTENDED) {
            cl.csr = cs_remap_q2pro_new;
            protocol = PROTOCOL_VERSION_DEFAULT;
        } else if (protocol < PROTOCOL_VERSION_OLD || protocol > PROTOCOL_VERSION_DEFAULT) {
            Com_Error(ERR_DROP, "Demo uses unsupported protocol version %d.", protocol);
        }
        cls.serverProtocol = protocol;
    }

    // game directory
    if (MSG_ReadString(cl.gamedir, sizeof(cl.gamedir)) >= sizeof(cl.gamedir)) {
        Com_Error(ERR_DROP, "Oversize gamedir string");
    }

    // never allow demos to change gamedir
    // do not change gamedir if connected to local sever either,
    // as it was already done by SV_InitGame, and changing it
    // here will not work since server is now running
    if (!cls.demo.playback && !sv_running->integer) {
        // pretend it has been set by user, so that 'changed' hook
        // gets called and filesystem is restarted
        Cvar_UserSet("game", cl.gamedir);

        // protect it from modifications while we are connected
        fs_game->flags |= CVAR_ROM;
    }

    // parse player entity number
    cl.clientNum = MSG_ReadShort();

    // get the full level name
    MSG_ReadString(levelname, sizeof(levelname));

    // setup default pmove parameters
    PmoveInit(&cl.pmp);

#if USE_FPS
    // setup default frame times
    cl.frametime = Com_ComputeFrametime(BASE_FRAMERATE);
    cl.frametime_inv = cl.frametime.div * BASE_1_FRAMETIME;
#endif
    cl.sv_frametime = 100;
    cl.sv_frametime_inv = 1.0f / cl.sv_frametime;
    cl.sv_framediv = 1;

    // setup default server state
    cl.serverstate = ss_game;
    cinematic = cl.clientNum == -1;

    if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2) {
        i = MSG_ReadByte();
        if (i) {
            Com_Error(ERR_DROP, "'Enhanced' R1Q2 servers are not supported");
        }
        i = MSG_ReadWord();
        // for some reason, R1Q2 servers always report the highest protocol
        // version they support, while still using the lower version
        // client specified in the 'connect' packet. oh well...
        if (!R1Q2_SUPPORTED(i)) {
            Com_WPrintf(
                "R1Q2 server reports unsupported protocol version %d.\n"
                "Assuming it really uses our current client version %d.\n"
                "Things will break if it does not!\n", i, PROTOCOL_VERSION_R1Q2_CURRENT);
            i = Q_clip(i, PROTOCOL_VERSION_R1Q2_MINIMUM, PROTOCOL_VERSION_R1Q2_CURRENT);
        }
        Com_DPrintf("Using minor R1Q2 protocol version %d\n", i);
        cls.protocolVersion = i;
        MSG_ReadByte(); // used to be advanced deltas
        i = MSG_ReadByte();
        if (i) {
            Com_DPrintf("R1Q2 strafejump hack enabled\n");
            cl.pmp.strafehack = true;
        }
        cl.esFlags |= MSG_ES_BEAMORIGIN;
        if (cls.protocolVersion >= PROTOCOL_VERSION_R1Q2_LONG_SOLID) {
            cl.esFlags |= MSG_ES_LONGSOLID;
        }
        cl.pmp.speedmult = 2;
    } else if (cls.serverProtocol == PROTOCOL_VERSION_Q2PRO) {
        i = MSG_ReadWord();
        if (!Q2PRO_SUPPORTED(i)) {
            Com_Error(ERR_DROP,
                      "Q2PRO server reports unsupported protocol version %d.\n"
                      "Current client version is %d.", i, PROTOCOL_VERSION_Q2PRO_CURRENT);
        }
        Com_DPrintf("Using minor Q2PRO protocol version %d\n", i);
        cls.protocolVersion = i;
        i = MSG_ReadByte();
        if (cls.protocolVersion >= PROTOCOL_VERSION_Q2PRO_SERVER_STATE) {
            Com_DPrintf("Q2PRO server state %d\n", i);
            cl.serverstate = i;
            cinematic = i == ss_pic || i == ss_cinematic;
        }
        if (cls.protocolVersion >= PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS) {
            read_q2pro_protocol_flags();
        } else {
            if (MSG_ReadByte()) {
                Com_DPrintf("Q2PRO strafejump hack enabled\n");
                cl.pmp.strafehack = true;
            }
            if (MSG_ReadByte()) {
                Com_DPrintf("Q2PRO QW mode enabled\n");
                PmoveEnableQW(&cl.pmp);
            }
            if (MSG_ReadByte()) {
                Com_DPrintf("Q2PRO waterjump hack enabled\n");
                cl.pmp.waterhack = true;
            }
        }
        cl.esFlags |= MSG_ES_UMASK | MSG_ES_LONGSOLID;
        if (cls.protocolVersion >= PROTOCOL_VERSION_Q2PRO_BEAM_ORIGIN) {
            cl.esFlags |= MSG_ES_BEAMORIGIN;
        }
        if (cls.protocolVersion >= PROTOCOL_VERSION_Q2PRO_SHORT_ANGLES) {
            cl.esFlags |= MSG_ES_SHORTANGLES;
        }
        cl.pmp.speedmult = 2;
        cl.pmp.flyhack = true; // fly hack is unconditionally enabled
        cl.pmp.flyfriction = 4;
    } else if (cls.serverProtocol == PROTOCOL_VERSION_RERELEASE) {
        cls.protocolVersion = MSG_ReadWord();
        cl.serverstate = MSG_ReadByte();
        cinematic = cl.serverstate == ss_pic || cl.serverstate == ss_cinematic;
        // FIXME: These shouldn't really matter, as pmove should be handled by the game/client library...
        read_q2pro_protocol_flags();
        cl.csr = cs_remap_rerelease;
        cl.psFlags |= MSG_PS_FLOAT_COORDS | MSG_PS_NEW_STATS;
        cl.esFlags |= MSG_ES_FLOAT_COORDS;
        int32_t rate = MSG_ReadByte();
        cl.sv_frametime = (1.0f / rate) * 1000;
        cl.sv_frametime_inv = 1.0f / cl.sv_frametime;
        cl.sv_framediv = rate / 10;

        cl.pmp.speedmult = 2;
        cl.pmp.flyhack = true; // fly hack is unconditionally enabled
        cl.pmp.flyfriction = 4;
    } else {
        cls.protocolVersion = 0;
    }

    if (cl.csr.extended) {
        cl.esFlags |= CL_ES_EXTENDED_MASK;
        cl.psFlags |= MSG_PS_EXTENSIONS;
    }

    cls.demo.esFlags = cl.csr.extended ? CL_ES_EXTENDED_MASK : 0;

    if (cinematic) {
        SCR_PlayCinematic(levelname);
    } else {
        // seperate the printfs so the server message can have a color
        Con_Printf(
            "\n\n"
            "\35\36\36\36\36\36\36\36\36\36\36\36"
            "\36\36\36\36\36\36\36\36\36\36\36\36"
            "\36\36\36\36\36\36\36\36\36\36\36\37"
            "\n\n");

        Com_SetColor(COLOR_ALT);
        Com_Printf("%s\n", levelname);
        Com_SetColor(COLOR_NONE);
    }

    // make sure clientNum is in range
    if (!VALIDATE_CLIENTNUM(&cl.csr, cl.clientNum)) {
        Com_WPrintf("Serverdata has invalid playernum %d\n", cl.clientNum);
        cl.clientNum = -1;
    }
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

tent_params_t   te;
mz_params_t     mz;
snd_params_t    snd;

static void CL_ParseTEntPacket(void)
{
    bool float_coords = cl.esFlags & MSG_ES_FLOAT_COORDS;
    te.type = MSG_ReadByte();

    switch (te.type) {
    case TE_BLOOD:
    case TE_GUNSHOT:
    case TE_SPARKS:
    case TE_BULLET_SPARKS:
    case TE_SCREEN_SPARKS:
    case TE_SHIELD_SPARKS:
    case TE_SHOTGUN:
    case TE_BLASTER:
    case TE_GREENBLOOD:
    case TE_BLASTER2:
    case TE_FLECHETTE:
    case TE_HEATBEAM_SPARKS:
    case TE_HEATBEAM_STEAM:
    case TE_MOREBLOOD:
    case TE_ELECTRIC_SPARKS:
    case TE_BLUEHYPERBLASTER_2:
    case TE_BERSERK_SLAM:
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadDir(te.dir);
        break;

    case TE_SPLASH:
    case TE_LASER_SPARKS:
    case TE_WELDING_SPARKS:
    case TE_TUNNEL_SPARKS:
        te.count = MSG_ReadByte();
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadDir(te.dir);
        te.color = MSG_ReadByte();
        break;

    case TE_BLUEHYPERBLASTER:
    case TE_RAILTRAIL:
    case TE_RAILTRAIL2:
    case TE_BUBBLETRAIL:
    case TE_DEBUGTRAIL:
    case TE_BUBBLETRAIL2:
    case TE_BFG_LASER:
    case TE_BFG_ZAP:
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadPos(te.pos2, float_coords);
        break;

    case TE_GRENADE_EXPLOSION:
    case TE_GRENADE_EXPLOSION_WATER:
    case TE_EXPLOSION2:
    case TE_PLASMA_EXPLOSION:
    case TE_ROCKET_EXPLOSION:
    case TE_ROCKET_EXPLOSION_WATER:
    case TE_EXPLOSION1:
    case TE_EXPLOSION1_NP:
    case TE_EXPLOSION1_BIG:
    case TE_BFG_EXPLOSION:
    case TE_BFG_BIGEXPLOSION:
    case TE_BOSSTPORT:
    case TE_PLAIN_EXPLOSION:
    case TE_CHAINFIST_SMOKE:
    case TE_TRACKER_EXPLOSION:
    case TE_TELEPORT_EFFECT:
    case TE_DBALL_GOAL:
    case TE_WIDOWSPLASH:
    case TE_NUKEBLAST:
    case TE_EXPLOSION1_NL:
    case TE_EXPLOSION2_NL:
        MSG_ReadPos(te.pos1, float_coords);
        break;

    case TE_PARASITE_ATTACK:
    case TE_MEDIC_CABLE_ATTACK:
    case TE_HEATBEAM:
    case TE_MONSTER_HEATBEAM:
    case TE_GRAPPLE_CABLE_2:
    case TE_LIGHTNING_BEAM:
        te.entity1 = MSG_ReadShort();
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadPos(te.pos2, float_coords);
        break;

    case TE_GRAPPLE_CABLE:
        te.entity1 = MSG_ReadShort();
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadPos(te.pos2, float_coords);
        MSG_ReadPos(te.offset, float_coords);
        break;

    case TE_LIGHTNING:
        te.entity1 = MSG_ReadShort();
        te.entity2 = MSG_ReadShort();
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadPos(te.pos2, float_coords);
        break;

    case TE_FLASHLIGHT:
        MSG_ReadPos(te.pos1, float_coords);
        te.entity1 = MSG_ReadShort();
        break;

    case TE_FORCEWALL:
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadPos(te.pos2, float_coords);
        te.color = MSG_ReadByte();
        break;

    case TE_STEAM:
        te.entity1 = MSG_ReadShort();
        te.count = MSG_ReadByte();
        MSG_ReadPos(te.pos1, float_coords);
        MSG_ReadDir(te.dir);
        te.color = MSG_ReadByte();
        te.entity2 = MSG_ReadShort();
        if (te.entity1 != -1) {
            te.time = MSG_ReadLong();
        }
        break;

    case TE_WIDOWBEAMOUT:
        te.entity1 = MSG_ReadShort();
        MSG_ReadPos(te.pos1, float_coords);
        break;

    case TE_POWER_SPLASH:
        te.entity1 = MSG_ReadShort();
        te.count = MSG_ReadByte();
        break;

    default:
        Com_Error(ERR_DROP, "%s: bad type", __func__);
    }
}

static void CL_ParseMuzzleFlashPacket(int mask, bool is_short)
{
    int entity, weapon;

    entity = MSG_ReadWord();
    weapon = is_short ? MSG_ReadWord() : MSG_ReadByte();

    if (!mask && cl.csr.extended) {
        weapon |= entity >> ENTITYNUM_BITS << 8;
        entity &= ENTITYNUM_MASK;
    }

    if (entity < 1 || entity >= cl.csr.max_edicts)
        Com_Error(ERR_DROP, "%s: bad entity", __func__);

    if (!mask && weapon >= q_countof(monster_flash_offset))
        Com_Error(ERR_DROP, "%s: bad weapon", __func__);

    mz.silenced = weapon & mask;
    mz.weapon = weapon & ~mask;
    mz.entity = entity;
}

static void CL_ParseStartSoundPacket(void)
{
    int flags, channel, entity;

    flags = MSG_ReadByte();

    if (cl.csr.extended && flags & SND_INDEX16)
        snd.index = MSG_ReadWord();
    else
        snd.index = MSG_ReadByte();

    if (snd.index >= cl.csr.max_sounds)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, snd.index);

    if (flags & SND_VOLUME)
        snd.volume = MSG_ReadByte() / 255.0f;
    else
        snd.volume = DEFAULT_SOUND_PACKET_VOLUME;

    if (flags & SND_ATTENUATION)
        snd.attenuation = MSG_ReadByte() / 64.0f;
    else
        snd.attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

    if (flags & SND_OFFSET)
        snd.timeofs = MSG_ReadByte() / 1000.0f;
    else
        snd.timeofs = 0;

    if (flags & SND_ENT) {
        // entity relative
        channel = MSG_ReadWord();
        entity = channel >> 3;
        if (entity < 0 || entity >= cl.csr.max_edicts)
            Com_Error(ERR_DROP, "%s: bad entity: %d", __func__, entity);
        snd.entity = entity;
        snd.channel = channel & 7;
    } else {
        snd.entity = 0;
        snd.channel = 0;
    }

    // positioned in space
    if (flags & SND_POS)
        MSG_ReadPos(snd.pos, cl.esFlags & MSG_ES_FLOAT_COORDS);

    snd.flags = flags;

    SHOWNET(2, "    %s\n", cl.configstrings[cl.csr.sounds + snd.index]);
}

static void CL_ParseReconnect(void)
{
    if (cls.demo.playback) {
        Com_Error(ERR_DISCONNECT, "Server disconnected");
    }

    Com_Printf("Server disconnected, reconnecting\n");

    // close netchan now to prevent `disconnect'
    // message from being sent to server
    Netchan_Close(&cls.netchan);

    CL_Disconnect(ERR_RECONNECT);

    cls.state = ca_challenging;
    cls.connect_time -= CONNECT_FAST;
    cls.connect_count = 0;

    CL_CheckForResend();
}

#if USE_AUTOREPLY
static void CL_CheckForVersion(const char *s)
{
    char *p;

    p = strstr(s, ": ");
    if (!p) {
        return;
    }

    if (strncmp(p + 2, "!version", 8)) {
        return;
    }

    if (cl.reply_time && cls.realtime - cl.reply_time < 120000) {
        return;
    }

    cl.reply_time = cls.realtime;
    cl.reply_delta = 1024 + (Q_rand() & 1023);
}
#endif

// attempt to scan out an IP address in dotted-quad notation and
// add it into circular array of recent addresses
static void CL_CheckForIP(const char *s)
{
    unsigned b1, b2, b3, b4, port;
    netadr_t *a;
    int n;

    while (*s) {
        n = sscanf(s, "%3u.%3u.%3u.%3u:%u", &b1, &b2, &b3, &b4, &port);
        if (n >= 4 && (b1 | b2 | b3 | b4) < 256) {
            if (n == 5) {
                if (port < 1024 || port > 65535) {
                    break; // privileged or invalid port
                }
            } else {
                port = PORT_SERVER;
            }

            a = &cls.recent_addr[cls.recent_head++ & RECENT_MASK];
            a->type = NA_IP;
            a->ip.u8[0] = b1;
            a->ip.u8[1] = b2;
            a->ip.u8[2] = b3;
            a->ip.u8[3] = b4;
            a->port = BigShort(port);
            break;
        }

        s++;
    }
}

static void CL_ParsePrint(void)
{
    int level;
    char s[MAX_STRING_CHARS];
    const char *fmt;

    level = MSG_ReadByte();
    MSG_ReadString(s, sizeof(s));

    SHOWNET(2, "    %i \"%s\"\n", level, Com_MakePrintable(s));

    if (level != PRINT_CHAT) {
        if (cl.csr.extended && (level == PRINT_TYPEWRITER || level == PRINT_CENTER))
            SCR_CenterPrint(s, level == PRINT_TYPEWRITER);
        else
        Com_Printf("%s", s);
        if (!cls.demo.playback && cl.serverstate != ss_broadcast) {
            COM_strclr(s);
            Cmd_ExecTrigger(s);
        }
        return;
    }

    if (CL_CheckForIgnore(s)) {
        return;
    }

#if USE_AUTOREPLY
    if (!cls.demo.playback && cl.serverstate != ss_broadcast) {
        CL_CheckForVersion(s);
    }
#endif

    CL_CheckForIP(s);

    // disable notify
    if (!cl_chat_notify->integer) {
        Con_SkipNotify(true);
    }

    // filter text
    if (cl_chat_filter->integer) {
        COM_strclr(s);
        fmt = "%s\n";
    } else {
        fmt = "%s";
    }

    Com_LPrintf(PRINT_TALK, fmt, s);

    Con_SkipNotify(false);

    SCR_AddToChatHUD(s);

    // silence MVD spectator chat
    if (cl.serverstate == ss_broadcast && !strncmp(s, "[MVD] ", 6))
        return;

    // play sound
    if (cl_chat_sound->integer > 1)
        S_StartLocalSoundOnce("misc/talk1.wav");
    else if (cl_chat_sound->integer > 0)
        S_StartLocalSoundOnce("misc/talk.wav");
}

static void CL_ParseCenterPrint(void)
{
    char s[MAX_STRING_CHARS];

    MSG_ReadString(s, sizeof(s));
    SHOWNET(2, "    \"%s\"\n", Com_MakePrintable(s));
    SCR_CenterPrint(s, false);

    if (!cls.demo.playback && cl.serverstate != ss_broadcast) {
        COM_strclr(s);
        Cmd_ExecTrigger(s);
    }
}

static void CL_ParseStuffText(void)
{
    char s[MAX_STRING_CHARS];

    MSG_ReadString(s, sizeof(s));
    SHOWNET(2, "    \"%s\"\n", Com_MakePrintable(s));
    Cbuf_AddText(&cl_cmdbuf, s);
}

static void CL_ParseLayout(void)
{
    MSG_ReadString(cl.layout, sizeof(cl.layout));
    SHOWNET(2, "    \"%s\"\n", Com_MakePrintable(cl.layout));
}

static void CL_ParseInventory(void)
{
    int        i;

    for (i = 0; i < MAX_ITEMS; i++) {
        cl.inventory[i] = MSG_ReadShort();
    }
}

static void CL_ParseDownload(int cmd)
{
    int size, percent, decompressed_size;

    if (!cls.download.temp[0]) {
        Com_Error(ERR_DROP, "%s: no download requested", __func__);
    }

    // read the data
    size = MSG_ReadShort();
    percent = MSG_ReadByte();
    if (size == -1) {
        CL_HandleDownload(NULL, size, percent, 0);
        return;
    }

    svc_ops_t svc_zdownload = cls.serverProtocol == PROTOCOL_VERSION_RERELEASE ? svc_rr_zdownload : svc_q2pro_zdownload;

    // read optional decompressed packet size
    if (cmd == svc_zdownload) {
#if USE_ZLIB
        if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2) {
            decompressed_size = MSG_ReadShort();
        } else {
            decompressed_size = -1;
        }
#else
        Com_Error(ERR_DROP, "Compressed server packet received, "
                  "but no zlib support linked in.");
#endif
    } else {
        decompressed_size = 0;
    }

    if (size < 0) {
        Com_Error(ERR_DROP, "%s: bad size: %d", __func__, size);
    }

    CL_HandleDownload(MSG_ReadData(size), size, percent, decompressed_size);
}

static void CL_ParseZPacket(void)
{
#if USE_ZLIB
    sizebuf_t   temp;
    byte        buffer[MAX_MSGLEN];
    uInt        inlen, outlen;
    int         ret;

    if (msg_read.data != msg_read_buffer) {
        Com_Error(ERR_DROP, "%s: recursively entered", __func__);
    }

    inlen = MSG_ReadWord();
    outlen = MSG_ReadWord();
    if (outlen > MAX_MSGLEN) {
        Com_Error(ERR_DROP, "%s: invalid output length", __func__);
    }

    inflateReset(&cls.z);

    cls.z.next_in = MSG_ReadData(inlen);
    cls.z.avail_in = inlen;
    cls.z.next_out = buffer;
    cls.z.avail_out = outlen;
    ret = inflate(&cls.z, Z_FINISH);
    if (ret != Z_STREAM_END) {
        Com_Error(ERR_DROP, "%s: inflate() failed with error %d", __func__, ret);
    }

    temp = msg_read;
    SZ_Init(&msg_read, buffer, sizeof(buffer));
    msg_read.cursize = outlen;

    CL_ParseServerMessage();

    msg_read = temp;
#else
    Com_Error(ERR_DROP, "Compressed server packet received, "
              "but no zlib support linked in.");
#endif
}

#if USE_FPS
static void set_server_fps(int value)
{
    cl.frametime = Com_ComputeFrametime(value);
    cl.frametime_inv = cl.frametime.div * BASE_1_FRAMETIME;

    // fix time delta
    if (cls.state == ca_active) {
        int delta = cl.frame.number - cl.servertime / cl.frametime.time;
        cl.serverdelta = Q_align(delta, cl.frametime.div);
    }

    Com_DPrintf("client framediv=%d time=%d delta=%d\n",
                cl.frametime.div, cl.servertime, cl.serverdelta);
}
#endif

static void CL_ParseSetting(void)
{
    int index q_unused;
    int value q_unused;

    index = MSG_ReadLong();
    value = MSG_ReadLong();

    switch (index) {
#if USE_FPS
    case SVS_FPS:
        set_server_fps(value);
        break;
#endif
    default:
        break;
    }
}

static void CL_SkipDamage(void)
{
    uint8_t count = MSG_ReadByte();

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t encoded = MSG_ReadByte();
        vec3_t dir;
        MSG_ReadDir(dir);
    }
}

static void CL_SkipFog(void)
{
    typedef enum
    {
        // global fog
        BIT_DENSITY     = BIT(0),
        BIT_R           = BIT(1),
        BIT_G           = BIT(2),
        BIT_B           = BIT(3),
        BIT_TIME        = BIT(4), // if set, the transition takes place over N milliseconds

        // height fog
        BIT_HEIGHTFOG_FALLOFF   = BIT(5),
        BIT_HEIGHTFOG_DENSITY   = BIT(6),
        BIT_MORE_BITS           = BIT(7), // read additional bit
        BIT_HEIGHTFOG_START_R   = BIT(8),
        BIT_HEIGHTFOG_START_G   = BIT(9),
        BIT_HEIGHTFOG_START_B   = BIT(10),
        BIT_HEIGHTFOG_START_DIST= BIT(11),
        BIT_HEIGHTFOG_END_R     = BIT(12),
        BIT_HEIGHTFOG_END_G     = BIT(13),
        BIT_HEIGHTFOG_END_B     = BIT(14),
        BIT_HEIGHTFOG_END_DIST  = BIT(15)
    } bits_t;

    bits_t bits = (bits_t) MSG_ReadByte ();

    if (bits & BIT_MORE_BITS)
        bits |= (bits_t) (MSG_ReadByte () << 8);

    if (bits & BIT_DENSITY)
    {
        MSG_ReadFloat ();
        MSG_ReadByte ();
    }
    if (bits & BIT_R)
        MSG_ReadByte ();
    if (bits & BIT_G)
        MSG_ReadByte ();
    if (bits & BIT_B)
        MSG_ReadByte ();
    if (bits & BIT_TIME)
        MSG_ReadWord ();

    if (bits & BIT_HEIGHTFOG_FALLOFF)
        MSG_ReadFloat ();
    if (bits & BIT_HEIGHTFOG_DENSITY)
        MSG_ReadFloat ();

    if (bits & BIT_HEIGHTFOG_START_R)
        MSG_ReadByte ();
    if (bits & BIT_HEIGHTFOG_START_G)
        MSG_ReadByte ();
    if (bits & BIT_HEIGHTFOG_START_B)
        MSG_ReadByte ();
    if (bits & BIT_HEIGHTFOG_START_DIST)
        MSG_ReadLong ();

    if (bits & BIT_HEIGHTFOG_END_R)
        MSG_ReadByte ();
    if (bits & BIT_HEIGHTFOG_END_G)
        MSG_ReadByte ();
    if (bits & BIT_HEIGHTFOG_END_B)
        MSG_ReadByte ();
    if (bits & BIT_HEIGHTFOG_END_DIST)
        MSG_ReadLong ();
}

/*
=====================
CL_ParseServerMessage
=====================
*/
typedef enum svc_handle_result_e
{
    svch_unknown,
    svch_handled,
    svch_break_loop,
    svch_continue_loop
} svc_handle_result_t;

static svc_handle_result_t handle_svc_common(int cmd, int extrabits)
{
    int index;
    uint64_t    bits;

    switch (cmd) {
    case svc_nop:
        return svch_handled;

    case svc_disconnect:
        Com_Error(ERR_DISCONNECT, "Server disconnected");
        return svch_handled;

    case svc_reconnect:
        CL_ParseReconnect();
        return svch_break_loop;

    case svc_print:
        CL_ParsePrint();
        return svch_handled;

    case svc_centerprint:
        CL_ParseCenterPrint();
        return svch_handled;

    case svc_stufftext:
        CL_ParseStuffText();
        return svch_handled;

    case svc_serverdata:
        CL_ParseServerData();
        return svch_continue_loop;

    case svc_configstring:
        index = MSG_ReadWord();
        CL_ParseConfigstring(index);
        return svch_handled;

    case svc_sound:
        CL_ParseStartSoundPacket();
        S_ParseStartSound();
        return svch_handled;

    case svc_spawnbaseline:
        index = MSG_ParseEntityBits(&bits, cl.esFlags);
        CL_ParseBaseline(index, bits);
        return svch_handled;

    case svc_temp_entity:
        CL_ParseTEntPacket();
        CL_ParseTEnt();
        return svch_handled;

    case svc_muzzleflash:
        CL_ParseMuzzleFlashPacket(MZ_SILENCED, false);
        CL_MuzzleFlash();
        return svch_handled;

    case svc_muzzleflash2:
        CL_ParseMuzzleFlashPacket(0, false);
        CL_MuzzleFlash2();
        return svch_handled;

    case svc_download:
        CL_ParseDownload(cmd);
        return svch_continue_loop;

    case svc_frame:
        CL_ParseFrame(extrabits);
        return svch_continue_loop;

    case svc_inventory:
        CL_ParseInventory();
        return svch_handled;

    case svc_layout:
        CL_ParseLayout();
        return svch_handled;

    }

    return svch_unknown;
}

static svc_handle_result_t handle_svc_q2pro(int cmd)
{
    switch (cmd) {
    case svc_q2pro_zpacket:
        CL_ParseZPacket();
        return svch_continue_loop;

    case svc_q2pro_zdownload:
        CL_ParseDownload(cmd);
        return svch_continue_loop;

    case svc_q2pro_gamestate:
    case svc_q2pro_configstringstream:
    case svc_q2pro_baselinestream:
        if (cls.serverProtocol != PROTOCOL_VERSION_Q2PRO) {
            return svch_unknown;
        }
        CL_ParseGamestateQ2PRO(cmd);
        return svch_continue_loop;

    case svc_q2pro_setting:
        CL_ParseSetting();
        return svch_continue_loop;
    }

    return svch_unknown;
}

static svc_handle_result_t handle_svc_rerelease(int cmd)
{
    switch (cmd) {
    case svc_rr_zpacket:
        CL_ParseZPacket();
        return svch_continue_loop;

    case svc_rr_zdownload:
        CL_ParseDownload(cmd);
        return svch_continue_loop;

    case svc_rr_gamestate:
    case svc_rr_configstringstream:
    case svc_rr_baselinestream:
        CL_ParseGamestateRR(cmd);
        return svch_continue_loop;

    case svc_rr_setting:
        CL_ParseSetting();
        return svch_continue_loop;

    // KEX
    case svc_damage:
        CL_SkipDamage();
        return svch_continue_loop;
    case svc_fog:
        CL_SkipFog();
        return svch_continue_loop;
    case svc_muzzleflash3:
        CL_ParseMuzzleFlashPacket(0, true);
        CL_MuzzleFlash2();
        break;
    // KEX
    }

    return svch_unknown;
}

void CL_ParseServerMessage(void)
{
    int         cmd, last_cmd = -1, extrabits;
    size_t      readcount;

#if USE_DEBUG
    if (cl_shownet->integer == 1) {
        Com_LPrintf(PRINT_DEVELOPER, "%zu ", msg_read.cursize);
    } else if (cl_shownet->integer > 1) {
        Com_LPrintf(PRINT_DEVELOPER, "------------------\n");
    }
#endif

    msg_read.allowunderflow = false;

//
// parse the message
//
    while (1) {
        readcount = msg_read.readcount;
        if (readcount == msg_read.cursize) {
            SHOWNET(1, "%3zu:END OF MESSAGE\n", readcount);
            break;
        }

        cmd = MSG_ReadByte();
        if (cls.serverProtocol != PROTOCOL_VERSION_RERELEASE) {
            if (cmd & ~SVCMD_MASK && (cls.serverProtocol < PROTOCOL_VERSION_R1Q2 || (cmd & SVCMD_MASK) != svc_frame))
                goto badbyte;
            extrabits = cmd >> SVCMD_BITS;
            cmd &= SVCMD_MASK;
        } else {
            // extrabits are stored inside another byte in svc_frame
            extrabits = 0;
        }

        SHOWNET(1, "%3zu:%s\n", msg_read.readcount - 1, MSG_ServerCommandString(cmd, cls.serverProtocol));

        svc_handle_result_t handle_result = handle_svc_common(cmd, extrabits);
        if (handle_result == svch_unknown) {
            if (cls.serverProtocol == PROTOCOL_VERSION_RERELEASE)
                handle_result = handle_svc_rerelease(cmd);
            else if (cls.serverProtocol >= PROTOCOL_VERSION_R1Q2)
                handle_result = handle_svc_q2pro(cmd);
        }

        switch(handle_result)
        {
        default:
        case svch_unknown:
        badbyte:
            Com_Error(ERR_DROP, "%s: illegible server message: %d, last good = %d", __func__, cmd, last_cmd);
            break;
        case svch_handled:
            break;
        case svch_break_loop:
            return;
        case svch_continue_loop:
            continue;
        }

        // if recording demos, copy off protocol invariant stuff
        if (cls.demo.recording && !cls.demo.paused) {
            uint32_t len = msg_read.readcount - readcount;

            // it is very easy to overflow standard 1390 bytes
            // demo frame with modern servers... attempt to preserve
            // reliable messages at least, assuming they come first
            if (cls.demo.buffer.cursize + len < cls.demo.buffer.maxsize) {
                SZ_Write(&cls.demo.buffer, msg_read.data + readcount, len);
            } else {
                cls.demo.others_dropped++;
            }
        }

        // if running GTV server, add current message
        CL_GTV_WriteMessage(msg_read.data + readcount,
                            msg_read.readcount - readcount);

        last_cmd = cmd;
    }
}

/*
=====================
CL_SeekDemoMessage

A variant of ParseServerMessage that skips over non-important action messages,
used for seeking in demos. Returns true if seeking should be aborted (got serverdata).
=====================
*/
bool CL_SeekDemoMessage(void)
{
    int         cmd, index;
    bool        serverdata = false;
    uint64_t    bits;

#if USE_DEBUG
    if (cl_shownet->integer == 1) {
        Com_LPrintf(PRINT_DEVELOPER, "%u ", msg_read.cursize);
    } else if (cl_shownet->integer > 1) {
        Com_LPrintf(PRINT_DEVELOPER, "------------------\n");
    }
#endif

    msg_read.allowunderflow = false;

//
// parse the message
//
    while (1) {
        if (msg_read.readcount == msg_read.cursize) {
            SHOWNET(1, "%3u:END OF MESSAGE\n", msg_read.readcount);
            break;
        }

        cmd = MSG_ReadByte();
        SHOWNET(1, "%3zu:%s\n", msg_read.readcount - 1, MSG_ServerCommandString(cmd, cls.serverProtocol));

        // other commands
        switch (cmd) {
        default:
            Com_Error(ERR_DROP, "%s: illegible server message: %d", __func__, cmd);
            break;

        case svc_nop:
            break;

        case svc_disconnect:
        case svc_reconnect:
            Com_Error(ERR_DISCONNECT, "Server disconnected");
            break;

        case svc_print:
            MSG_ReadByte();
            // fall through

        case svc_centerprint:
        case svc_stufftext:
            MSG_ReadString(NULL, 0);
            break;

        case svc_serverdata:
            CL_ParseServerData();
            serverdata = true;
            break;

        case svc_configstring:
            index = MSG_ReadWord();
            CL_ParseConfigstring(index);
            break;

        case svc_sound:
            CL_ParseStartSoundPacket();
            break;

        case svc_spawnbaseline:
            index = MSG_ParseEntityBits(&bits, cl.esFlags);
            CL_ParseBaseline(index, bits);
            break;

        case svc_temp_entity:
            CL_ParseTEntPacket();
            break;

        case svc_muzzleflash:
            CL_ParseMuzzleFlashPacket(MZ_SILENCED, false);
            break;

        case svc_muzzleflash2:
            CL_ParseMuzzleFlashPacket(0, false);
            break;

        case svc_frame:
            CL_ParseFrame(0);
            continue;

        case svc_inventory:
            CL_ParseInventory();
            break;

        case svc_layout:
            CL_ParseLayout();
            break;
        }
    }

    return serverdata;
}
