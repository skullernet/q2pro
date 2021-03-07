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

#include "g_local.h"

game_locals_t   game;
level_locals_t  level;
edict_pool_t    *pool;
spawn_temp_t    st;

int sm_meat_index;
int snd_fry;
int meansOfDeath;

edict_t     *g_edicts;

cvar_t  *deathmatch;
cvar_t  *coop;
cvar_t  *dmflags;
cvar_t  *skill;
cvar_t  *fraglimit;
cvar_t  *timelimit;
cvar_t  *password;
cvar_t  *spectator_password;
cvar_t  *needpass;
cvar_t  *maxclients;
cvar_t  *maxspectators;
cvar_t  *maxentities;
cvar_t  *g_select_empty;
cvar_t  *dedicated;

cvar_t  *filterban;

cvar_t  *sv_maxvelocity;
cvar_t  *sv_gravity;

cvar_t  *sv_rollspeed;
cvar_t  *sv_rollangle;
cvar_t  *gun_x;
cvar_t  *gun_y;
cvar_t  *gun_z;

cvar_t  *run_pitch;
cvar_t  *run_roll;
cvar_t  *bob_up;
cvar_t  *bob_pitch;
cvar_t  *bob_roll;

cvar_t  *sv_cheats;

cvar_t  *flood_msgs;
cvar_t  *flood_persecond;
cvar_t  *flood_waitdelay;

cvar_t  *sv_maplist;

cvar_t  *sv_features;


//===================================================================

// We must only return caps before InitGame has run.
static qboolean query_lock = false;

/*
============
QueryGameCapability

The engine will call this function to query the game DLL
for additional capabilities.
============
*/
game_capability_t QueryGameCapability(const char *capability)
{
    // if it's too late to receive queries, return only false.
    if (query_lock)
        return CAPABILITY_FALSE;

    return CAPABILITY_FALSE;
}

void ShutdownGame(void)
{
    gi_dprintf("==== ShutdownGame ====\n");

    gi_FreeTags(TAG_LEVEL);
    gi_FreeTags(TAG_GAME);
}

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
void InitGame(void)
{
    query_lock = true;

    gi_dprintf("==== InitGame ====\n");

    Q_srand(time(NULL));

    gun_x = gi_cvar("gun_x", "0", 0);
    gun_y = gi_cvar("gun_y", "0", 0);
    gun_z = gi_cvar("gun_z", "0", 0);

    //FIXME: sv_ prefix is wrong for these
    sv_rollspeed = gi_cvar("sv_rollspeed", "200", 0);
    sv_rollangle = gi_cvar("sv_rollangle", "2", 0);
    sv_maxvelocity = gi_cvar("sv_maxvelocity", "2000", 0);
    sv_gravity = gi_cvar("sv_gravity", "800", 0);

    // noset vars
    dedicated = gi_cvar("dedicated", "0", CVAR_NOSET);

    // latched vars
    sv_cheats = gi_cvar("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);
    gi_cvar("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
    gi_cvar("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);

    maxclients = gi_cvar("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
    maxspectators = gi_cvar("maxspectators", "4", CVAR_SERVERINFO);
    deathmatch = gi_cvar("deathmatch", "0", CVAR_LATCH);
    coop = gi_cvar("coop", "0", CVAR_LATCH);
    skill = gi_cvar("skill", "1", CVAR_LATCH);
    maxentities = gi_cvar("maxentities", "1024", CVAR_LATCH);

    // change anytime vars
    dmflags = gi_cvar("dmflags", "0", CVAR_SERVERINFO);
    fraglimit = gi_cvar("fraglimit", "0", CVAR_SERVERINFO);
    timelimit = gi_cvar("timelimit", "0", CVAR_SERVERINFO);
    password = gi_cvar("password", "", CVAR_USERINFO);
    spectator_password = gi_cvar("spectator_password", "", CVAR_USERINFO);
    needpass = gi_cvar("needpass", "0", CVAR_SERVERINFO);
    filterban = gi_cvar("filterban", "1", 0);

    g_select_empty = gi_cvar("g_select_empty", "0", CVAR_ARCHIVE);

    run_pitch = gi_cvar("run_pitch", "0.002", 0);
    run_roll = gi_cvar("run_roll", "0.005", 0);
    bob_up  = gi_cvar("bob_up", "0.005", 0);
    bob_pitch = gi_cvar("bob_pitch", "0.002", 0);
    bob_roll = gi_cvar("bob_roll", "0.002", 0);

    // flood control
    flood_msgs = gi_cvar("flood_msgs", "4", 0);
    flood_persecond = gi_cvar("flood_persecond", "4", 0);
    flood_waitdelay = gi_cvar("flood_waitdelay", "10", 0);

    // dm map list
    sv_maplist = gi_cvar("sv_maplist", "", 0);

    // obtain server features
    sv_features = gi_cvar("sv_features", NULL, 0);

    // export our own features
    gi_cvar_forceset("g_features", va("%d", G_FEATURES));

    // items
    InitItems();

    game.helpmessage1[0] = 0;
    game.helpmessage2[0] = 0;

    // initialize all entities for this game
    game.maxentities = maxentities->value;
    clamp(game.maxentities, (int)maxclients->value + 1, MAX_EDICTS);
    g_edicts = gi_TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
    pool->edicts = g_edicts;
    pool->max_edicts = game.maxentities;

    // initialize all clients for this game
    game.maxclients = maxclients->value;
    game.clients = gi_TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_GAME);
    pool->num_edicts = game.maxclients + 1;
}

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c can link
void Com_LPrintf(print_type_t type, const char *fmt, ...)
{
    va_list     argptr;
    char        text[MAX_STRING_CHARS];

    if (type == PRINT_DEVELOPER) {
        return;
    }

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi_dprintf("%s", text);
}

void Com_Error(error_type_t type, const char *fmt, ...)
{
    va_list     argptr;
    char        text[MAX_STRING_CHARS];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi_error("%s", text);
}
#endif

//======================================================================


/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames(void)
{
    int     i;
    edict_t *ent;

    // calc the player views now that all pushing
    // and damage has been added
    for (i = 0 ; i < maxclients->value ; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse || !ent->client)
            continue;
        ClientEndServerFrame(ent);
    }

}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
    edict_t *ent;

    ent = G_Spawn();
    ent->classname = "target_changelevel";
    Q_snprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
    ent->map = level.nextmap;
    return ent;
}

/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel(void)
{
    edict_t     *ent;
    char *s, *t, *f;
    static const char *seps = " ,\n\r";

    // stay on same level flag
    if ((int)dmflags->value & DF_SAME_LEVEL) {
        BeginIntermission(CreateTargetChangeLevel(level.mapname));
        return;
    }

    // see if it's in the map list
    if (*sv_maplist->string) {
        s = strdup(sv_maplist->string);
        f = NULL;
        t = strtok(s, seps);
        while (t != NULL) {
            if (Q_stricmp(t, level.mapname) == 0) {
                // it's in the list, go to the next one
                t = strtok(NULL, seps);
                if (t == NULL) { // end of list, go to first one
                    if (f == NULL) // there isn't a first one, same level
                        BeginIntermission(CreateTargetChangeLevel(level.mapname));
                    else
                        BeginIntermission(CreateTargetChangeLevel(f));
                } else
                    BeginIntermission(CreateTargetChangeLevel(t));
                free(s);
                return;
            }
            if (!f)
                f = t;
            t = strtok(NULL, seps);
        }
        free(s);
    }

    if (level.nextmap[0]) // go to a specific map
        BeginIntermission(CreateTargetChangeLevel(level.nextmap));
    else {  // search for a changelevel
        ent = G_Find(NULL, FOFS(classname), "target_changelevel");
        if (!ent) {
            // the map designer didn't include a changelevel,
            // so create a fake ent that goes back to the same level
            BeginIntermission(CreateTargetChangeLevel(level.mapname));
            return;
        }
        BeginIntermission(ent);
    }
}


/*
=================
CheckNeedPass
=================
*/
void CheckNeedPass(void)
{
    int need;

    // if password or spectator_password has changed, update needpass
    // as needed
    if (password->modified || spectator_password->modified) {
        password->modified = spectator_password->modified = false;

        need = 0;

        if (*password->string && Q_stricmp(password->string, "none"))
            need |= 1;
        if (*spectator_password->string && Q_stricmp(spectator_password->string, "none"))
            need |= 2;

        gi_cvar_set("needpass", va("%d", need));
    }
}

/*
=================
CheckDMRules
=================
*/
void CheckDMRules(void)
{
    int         i;
    gclient_t   *cl;

    if (level.intermission_framenum)
        return;

    if (!deathmatch->value)
        return;

    if (timelimit->value) {
        if (level.time >= timelimit->value * 60) {
            gi_bprintf(PRINT_HIGH, "Timelimit hit.\n");
            EndDMLevel();
            return;
        }
    }

    if (fraglimit->value) {
        for (i = 0 ; i < maxclients->value ; i++) {
            cl = game.clients + i;
            if (!g_edicts[i + 1].inuse)
                continue;

            if (cl->resp.score >= fraglimit->value) {
                gi_bprintf(PRINT_HIGH, "Fraglimit hit.\n");
                EndDMLevel();
                return;
            }
        }
    }
}


/*
=============
ExitLevel
=============
*/
void ExitLevel(void)
{
    int     i;
    edict_t *ent;
    char    command [256];

    Q_snprintf(command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
    gi_AddCommandString(command);
    level.changemap = NULL;
    level.exitintermission = 0;
    level.intermission_framenum = 0;
    ClientEndServerFrames();

    // clear some things before going to next level
    for (i = 0 ; i < maxclients->value ; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse)
            continue;
        if (ent->health > ent->client->pers.max_health)
            ent->health = ent->client->pers.max_health;
    }

}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
void G_RunFrame(void)
{
    int     i;
    edict_t *ent;

    level.framenum++;
    level.time = level.framenum * FRAMETIME;

    // choose a client for monsters to target this frame
    AI_SetSightClient();

    // exit intermissions

    if (level.exitintermission) {
        ExitLevel();
        return;
    }

    //
    // treat each object in turn
    // even the world gets a chance to think
    //
    ent = &g_edicts[0];
    for (i = 0 ; i < pool->num_edicts ; i++, ent++) {
        if (!ent->inuse)
            continue;

        level.current_entity = ent;

        VectorCopy(ent->s.origin, ent->s.old_origin);

        // if the ground entity moved, make sure we are still on it
        if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount)) {
            ent->groundentity = NULL;
            if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svflags & SVF_MONSTER)) {
                M_CheckGround(ent);
            }
        }

        if (i > 0 && i <= maxclients->value) {
            ClientBeginServerFrame(ent);
            continue;
        }

        G_RunEntity(ent);
    }

    // see if it is time to end a deathmatch
    CheckDMRules();

    // see if needpass needs updated
    CheckNeedPass();

    // build the playerstate_t structures for all players
    ClientEndServerFrames();
}
