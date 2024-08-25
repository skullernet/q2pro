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
// cl_tent.c -- client side temporary entities

#include "client.h"
#include "common/mdfour.h"

qhandle_t   cl_sfx_ric1;
qhandle_t   cl_sfx_ric2;
qhandle_t   cl_sfx_ric3;
qhandle_t   cl_sfx_lashit;
qhandle_t   cl_sfx_spark5;
qhandle_t   cl_sfx_spark6;
qhandle_t   cl_sfx_spark7;
qhandle_t   cl_sfx_railg;
qhandle_t   cl_sfx_rockexp;
qhandle_t   cl_sfx_grenexp;
qhandle_t   cl_sfx_watrexp;

qhandle_t   cl_sfx_lightning;
qhandle_t   cl_sfx_disrexp;

qhandle_t   cl_sfx_hit_marker;

qhandle_t   cl_mod_explode;
qhandle_t   cl_mod_smoke;
qhandle_t   cl_mod_flash;
qhandle_t   cl_mod_parasite_segment;
qhandle_t   cl_mod_grapple_cable;
qhandle_t   cl_mod_explo4;
qhandle_t   cl_mod_bfg_explo;
qhandle_t   cl_mod_powerscreen;
qhandle_t   cl_mod_laser;
qhandle_t   cl_mod_dmspot;

qhandle_t   cl_mod_lightning;
qhandle_t   cl_mod_heatbeam;

qhandle_t   cl_mod_muzzles[MFLASH_TOTAL];

qhandle_t   cl_img_flare;

static cvar_t   *cl_muzzleflashes;
static cvar_t   *cl_hit_markers;

#define MAX_FOOTSTEP_SFX    9

typedef struct {
    int         num_sfx;
    qhandle_t   sfx[MAX_FOOTSTEP_SFX];
} cl_footstep_sfx_t;

static cl_footstep_sfx_t    *cl_footstep_sfx;
static int                  cl_num_footsteps;
static qhandle_t            cl_last_footstep;

extern mtexinfo_t nulltexinfo;

/*
=================
CL_FindFootstepSurface
=================
*/
static int CL_FindFootstepSurface(int entnum)
{
    int footstep_id = FOOTSTEP_ID_DEFAULT;
    centity_t *cent = &cl_entities[entnum];

    // skip if no materials loaded
    if (cl_num_footsteps <= FOOTSTEP_RESERVED_COUNT)
        return footstep_id;

    // allow custom footsteps to be disabled
    if (cl_footsteps->integer >= 2)
        return footstep_id;

    // use an X/Y only mins/maxs copy of the entity,
    // since we don't want it to get caught inside of any geometry above or below
    const vec3_t trace_mins = { cent->mins[0], cent->mins[1], 0 };
    const vec3_t trace_maxs = { cent->maxs[0], cent->maxs[1], 0 };

    // trace start position is the entity's current origin + { 0 0 1 },
    // so that entities with their mins at 0 won't get caught in the floor
    vec3_t trace_start;
    VectorCopy(cent->current.origin, trace_start);
    trace_start[2] += 1;

    // the end of the trace starts down by half of STEPSIZE
    vec3_t trace_end;
    VectorCopy(trace_start, trace_end);
    trace_end[2] -= 9;
    if (cent->current.solid && cent->current.solid != PACKED_BSP) {
        // if the entity is a bbox'd entity, the mins.z is added to the end point as well
        trace_end[2] += cent->mins[2];
    } else {
        // otherwise use a value that should cover every monster in the game
        trace_end[2] -= 66; // should you wonder: monster_guardian is the biggest boi
    }

    // first, a trace done solely against MASK_SOLID
    trace_t tr;
    CL_Trace(&tr, trace_start, trace_end, trace_mins, trace_maxs, MASK_SOLID);

    if (tr.fraction == 1.0f) {
        // if we didn't hit anything, use default step ID
        return footstep_id;
    }

    if (tr.surface != &(nulltexinfo.c)) {
        // copy over the surfaces' step ID
        footstep_id = ((mtexinfo_t *)tr.surface)->step_id;

        // do another trace that ends instead at endpos + { 0 0 1 }, and is against MASK_SOLID | MASK_WATER
        vec3_t new_end;
        VectorCopy(tr.endpos, new_end);
        new_end[2] += 1;

        CL_Trace(&tr, trace_start, new_end, trace_mins, trace_maxs, MASK_SOLID | MASK_WATER);
        // if we hit something else, use that new footstep id instead of the first traces' value
        if (tr.surface != &(nulltexinfo.c))
            footstep_id = ((mtexinfo_t *)tr.surface)->step_id;
    }

    return footstep_id;
}

/*
=================
CL_PlayFootstepSfx
=================
*/
void CL_PlayFootstepSfx(int step_id, int entnum, float volume, float attenuation)
{
    const cl_footstep_sfx_t *sfx;
    qhandle_t footstep_sfx;
    int sfx_num;

    if (!cl_num_footsteps)
        return; // should not really happen

    if (step_id == -1)
        step_id = CL_FindFootstepSurface(entnum);

    Q_assert((unsigned)step_id < cl_num_footsteps);

    sfx = &cl_footstep_sfx[step_id];
    if (!sfx->num_sfx)
        sfx = &cl_footstep_sfx[0];
    if (!sfx->num_sfx)
        return; // no footsteps, not even fallbacks

    // pick a random footstep sound, but avoid playing the same one twice in a row
    sfx_num = Q_rand_uniform(sfx->num_sfx);
    footstep_sfx = sfx->sfx[sfx_num];
    if (footstep_sfx == cl_last_footstep)
        footstep_sfx = sfx->sfx[(sfx_num + 1) % sfx->num_sfx];

    S_StartSound(NULL, entnum, CHAN_BODY, footstep_sfx, volume, attenuation, 0);
    cl_last_footstep = footstep_sfx;
}

/*
=================
CL_RegisterFootstep
=================
*/
static void CL_RegisterFootstep(cl_footstep_sfx_t *sfx, const char *material)
{
    char name[MAX_QPATH];
    size_t len;
    int i;

    Q_assert(!material || *material);

    for (i = 0; i < MAX_FOOTSTEP_SFX; i++) {
        if (material)
            len = Q_snprintf(name, sizeof(name), "#sound/player/steps/%s%i.wav", material, i + 1);
        else
            len = Q_snprintf(name, sizeof(name), "#sound/player/step%i.wav", i + 1);
        Q_assert(len < sizeof(name));
        if (FS_LoadFile(name + 1, NULL) < 0)
            break;
        sfx->sfx[i] = S_RegisterSound(name);
    }

    sfx->num_sfx = i;
}

/*
=================
CL_RegisterFootsteps
=================
*/
static void CL_RegisterFootsteps(void)
{
    mtexinfo_t *tex;
    int i;

    cl_last_footstep = 0;

    Z_Freep(&cl_footstep_sfx);
    if (!cl.bsp) {
        cl_num_footsteps = 0;
        return;
    }

    cl_num_footsteps = BSP_LoadMaterials(cl.bsp);
    Q_assert(cl_num_footsteps >= FOOTSTEP_RESERVED_COUNT);
    cl_footstep_sfx = Z_Malloc(sizeof(cl_footstep_sfx[0]) * cl_num_footsteps);

    for (i = 0; i < cl_num_footsteps; i++)
        cl_footstep_sfx[i].num_sfx = -1;

    // load reserved footsteps
    CL_RegisterFootstep(&cl_footstep_sfx[FOOTSTEP_ID_DEFAULT], NULL);
    CL_RegisterFootstep(&cl_footstep_sfx[FOOTSTEP_ID_LADDER], "ladder");

    // load the rest
    for (i = 0, tex = cl.bsp->texinfo; i < cl.bsp->numtexinfo; i++, tex++) {
        cl_footstep_sfx_t *sfx = &cl_footstep_sfx[tex->step_id];
        if (sfx->num_sfx == -1)
            CL_RegisterFootstep(sfx, tex->material);
    }
}

/*
=================
CL_RegisterTEntSounds
=================
*/
void CL_RegisterTEntSounds(void)
{
    cl_sfx_ric1 = S_RegisterSound("world/ric1.wav");
    cl_sfx_ric2 = S_RegisterSound("world/ric2.wav");
    cl_sfx_ric3 = S_RegisterSound("world/ric3.wav");
    cl_sfx_lashit = S_RegisterSound("weapons/lashit.wav");
    cl_sfx_spark5 = S_RegisterSound("world/spark5.wav");
    cl_sfx_spark6 = S_RegisterSound("world/spark6.wav");
    cl_sfx_spark7 = S_RegisterSound("world/spark7.wav");
    cl_sfx_railg = S_RegisterSound("weapons/railgf1a.wav");
    cl_sfx_rockexp = S_RegisterSound("weapons/rocklx1a.wav");
    cl_sfx_grenexp = S_RegisterSound("weapons/grenlx1a.wav");
    cl_sfx_watrexp = S_RegisterSound("weapons/xpld_wat.wav");

    S_RegisterSound("player/land1.wav");
    S_RegisterSound("player/fall2.wav");
    S_RegisterSound("player/fall1.wav");

    CL_RegisterFootsteps();

    cl_sfx_lightning = S_RegisterSound("weapons/tesla.wav");
    cl_sfx_disrexp = S_RegisterSound("weapons/disrupthit.wav");

    cl_sfx_hit_marker = S_RegisterSound("weapons/marker.wav");
}

static const char *const muzzlenames[MFLASH_TOTAL] = {
    [MFLASH_MACHN]     = "v_machn",
    [MFLASH_SHOTG2]    = "v_shotg2",
    [MFLASH_SHOTG]     = "v_shotg",
    [MFLASH_ROCKET]    = "v_rocket",
    [MFLASH_RAIL]      = "v_rail",
    [MFLASH_LAUNCH]    = "v_launch",
    [MFLASH_ETF_RIFLE] = "v_etf_rifle",
    [MFLASH_DIST]      = "v_dist",
    [MFLASH_BOOMER]    = "v_boomer",
    [MFLASH_BLAST]     = "v_blast",
    [MFLASH_BFG]       = "v_bfg",
    [MFLASH_BEAMER]    = "v_beamer",
};

/*
=================
CL_RegisterTEntModels
=================
*/
void CL_RegisterTEntModels(void)
{
    void *data;
    int len;

    cl_mod_explode = R_RegisterModel("models/objects/explode/tris.md2");
    cl_mod_smoke = R_RegisterModel("models/objects/smoke/tris.md2");
    cl_mod_flash = R_RegisterModel("models/objects/flash/tris.md2");
    cl_mod_parasite_segment = R_RegisterModel("models/monsters/parasite/segment/tris.md2");
    cl_mod_grapple_cable = R_RegisterModel("models/ctf/segment/tris.md2");
    cl_mod_explo4 = R_RegisterModel("models/objects/r_explode/tris.md2");
    cl_mod_bfg_explo = R_RegisterModel("sprites/s_bfg2.sp2");
    cl_mod_powerscreen = R_RegisterModel("models/items/armor/effect/tris.md2");
    cl_mod_laser = R_RegisterModel("models/objects/laser/tris.md2");
    cl_mod_dmspot = R_RegisterModel("models/objects/dmspot/tris.md2");

    cl_mod_lightning = R_RegisterModel("models/proj/lightning/tris.md2");
    cl_mod_heatbeam = R_RegisterModel("models/proj/beam/tris.md2");

    for (int i = 0; i < MFLASH_TOTAL; i++)
        cl_mod_muzzles[i] = R_RegisterModel(va("models/weapons/%s/flash/tris.md2", muzzlenames[i]));

    cl_img_flare = R_RegisterSprite("misc/flare.tga");

    // check for remaster powerscreen model (ugly!)
    len = FS_LoadFile("models/items/armor/effect/tris.md2", &data);
    cl.need_powerscreen_scale = len == 2300 && Com_BlockChecksum(data, len) == 0x19fca65b;
    FS_FreeFile(data);
}

/*
==============================================================

EXPLOSION MANAGEMENT

==============================================================
*/

#define MAX_EXPLOSIONS  256

typedef struct {
    enum {
        ex_free,
        ex_misc,
        ex_flash,
        ex_mflash,
        ex_poly,
        ex_poly2,
        ex_light
    } type;

    entity_t    ent;
    int         frames;
    float       light;
    vec3_t      lightcolor;
    float       start;
    int         baseframe;
} explosion_t;

static explosion_t  cl_explosions[MAX_EXPLOSIONS];

static void CL_ClearExplosions(void)
{
    memset(cl_explosions, 0, sizeof(cl_explosions));
}

static explosion_t *CL_AllocExplosion(void)
{
    explosion_t *e, *oldest;
    int     i;
    int     time;

    for (i = 0, e = cl_explosions; i < MAX_EXPLOSIONS; i++, e++) {
        if (e->type == ex_free) {
            memset(e, 0, sizeof(*e));
            return e;
        }
    }
// find the oldest explosion
    time = cl.time;
    oldest = cl_explosions;

    for (i = 0, e = cl_explosions; i < MAX_EXPLOSIONS; i++, e++) {
        if (e->start < time) {
            time = e->start;
            oldest = e;
        }
    }
    memset(oldest, 0, sizeof(*oldest));
    return oldest;
}

static explosion_t *CL_PlainExplosion(void)
{
    explosion_t *ex;

    ex = CL_AllocExplosion();
    VectorCopy(te.pos1, ex->ent.origin);
    ex->type = ex_poly;
    ex->ent.flags = RF_FULLBRIGHT;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->light = 350;
    VectorSet(ex->lightcolor, 1.0f, 0.5f, 0.5f);
    ex->ent.angles[1] = Q_rand() % 360;
    ex->ent.model = cl_mod_explo4;
    ex->baseframe = 15 * (Q_rand() & 1);
    ex->frames = 15;

    return ex;
}

static void CL_BFGExplosion(const vec3_t pos)
{
    explosion_t *ex;

    ex = CL_AllocExplosion();
    VectorCopy(pos, ex->ent.origin);
    ex->type = ex_poly;
    ex->ent.flags = RF_FULLBRIGHT;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->light = 350;
    VectorSet(ex->lightcolor, 0.0f, 1.0f, 0.0f);
    ex->ent.model = cl_mod_bfg_explo;
    ex->ent.flags |= RF_TRANSLUCENT;
    ex->ent.alpha = 0.30f;
    ex->frames = 4;
}

void CL_AddWeaponMuzzleFX(cl_muzzlefx_t fx, const vec3_t offset, float scale)
{
    if (!cl_muzzleflashes->integer)
        return;
    if (mz.entity != cl.frame.clientNum + 1)
        return;

    Q_assert(fx < q_countof(cl_mod_muzzles));

    if (!cl_mod_muzzles[fx])
        return;

    cl.weapon.muzzle.model = cl_mod_muzzles[fx];
    cl.weapon.muzzle.scale = scale;
    if (fx == MFLASH_MACHN || fx == MFLASH_BEAMER)
        cl.weapon.muzzle.roll = Q_rand() % 360;
    else
        cl.weapon.muzzle.roll = 0;
    VectorCopy(offset, cl.weapon.muzzle.offset);
    cl.weapon.muzzle.time = cl.servertime - CL_FRAMETIME;
}

void CL_AddMuzzleFX(const vec3_t origin, const vec3_t angles, cl_muzzlefx_t fx, int skin, float scale)
{
    explosion_t *ex;

    if (!cl_muzzleflashes->integer)
        return;

    Q_assert(fx < q_countof(cl_mod_muzzles));

    if (!cl_mod_muzzles[fx])
        return;

    ex = CL_AllocExplosion();
    VectorCopy(origin, ex->ent.origin);
    VectorCopy(angles, ex->ent.angles);
    ex->type = ex_mflash;
    ex->ent.flags = RF_TRANSLUCENT | RF_NOSHADOW | RF_FULLBRIGHT;
    ex->ent.alpha = 1.0f;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->ent.model = cl_mod_muzzles[fx];
    ex->ent.skinnum = skin;
    ex->ent.scale = scale;
    if (fx != MFLASH_BOOMER)
        ex->ent.angles[2] = Q_rand() % 360;
}

/*
=================
CL_SmokeAndFlash
=================
*/
void CL_SmokeAndFlash(const vec3_t origin)
{
    explosion_t *ex;

    ex = CL_AllocExplosion();
    VectorCopy(origin, ex->ent.origin);
    ex->type = ex_misc;
    ex->frames = 4;
    ex->ent.flags = RF_TRANSLUCENT | RF_NOSHADOW;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->ent.model = cl_mod_smoke;

    ex = CL_AllocExplosion();
    VectorCopy(origin, ex->ent.origin);
    ex->type = ex_flash;
    ex->ent.flags = RF_FULLBRIGHT;
    ex->frames = 2;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->ent.model = cl_mod_flash;
}

static void CL_AddExplosions(void)
{
    entity_t    *ent;
    int         i;
    explosion_t *ex;
    float       frac;
    int         f;

    for (i = 0, ex = cl_explosions; i < MAX_EXPLOSIONS; i++, ex++) {
        if (ex->type == ex_free)
            continue;

        if (ex->type == ex_mflash) {
            if (cl.time - ex->start > 50)
                ex->type = ex_free;
            else
                V_AddEntity(&ex->ent);
            continue;
        }

        frac = (cl.time - ex->start) * BASE_1_FRAMETIME;
        f = floorf(frac);

        ent = &ex->ent;

        switch (ex->type) {
        case ex_misc:
        case ex_light:
            if (f >= ex->frames - 1) {
                ex->type = ex_free;
                break;
            }
            ent->alpha = 1.0f - frac / (ex->frames - 1);
            break;
        case ex_flash:
            if (f >= 1) {
                ex->type = ex_free;
                break;
            }
            ent->alpha = 1.0f;
            break;
        case ex_poly:
            if (f >= ex->frames - 1) {
                ex->type = ex_free;
                break;
            }

            ent->alpha = (16.0f - (float)f) / 16.0f;

            if (f < 10) {
                ent->skinnum = (f >> 1);
                if (ent->skinnum < 0)
                    ent->skinnum = 0;
            } else {
                ent->flags |= RF_TRANSLUCENT;
                if (f < 13)
                    ent->skinnum = 5;
                else
                    ent->skinnum = 6;
            }
            break;
        case ex_poly2:
            if (f >= ex->frames - 1) {
                ex->type = ex_free;
                break;
            }

            ent->alpha = (5.0f - (float)f) / 5.0f;
            ent->skinnum = 0;
            ent->flags |= RF_TRANSLUCENT;
            break;
        default:
            Q_assert(!"bad type");
        }

        if (ex->type == ex_free)
            continue;

        if (ex->light)
            V_AddLight(ent->origin, ex->light * ent->alpha,
                       ex->lightcolor[0], ex->lightcolor[1], ex->lightcolor[2]);

        if (ex->type != ex_light) {
            VectorCopy(ent->origin, ent->oldorigin);

            if (f < 0)
                f = 0;
            ent->frame = ex->baseframe + f + 1;
            ent->oldframe = ex->baseframe + f;
            ent->backlerp = 1.0f - (frac - f);

            V_AddEntity(ent);
        }
    }
}

/*
==============================================================

LASER MANAGEMENT

==============================================================
*/

#define MAX_LASERS  256

typedef struct {
    vec3_t      start;
    vec3_t      end;
    int         color;
    color_t     rgba;
    int         width;
    int         lifetime, starttime;
} laser_t;

static laser_t  cl_lasers[MAX_LASERS];

static void CL_ClearLasers(void)
{
    memset(cl_lasers, 0, sizeof(cl_lasers));
}

static laser_t *CL_AllocLaser(void)
{
    laser_t *l;
    int i;

    for (i = 0, l = cl_lasers; i < MAX_LASERS; i++, l++) {
        if (cl.time - l->starttime >= l->lifetime) {
            memset(l, 0, sizeof(*l));
            l->starttime = cl.time;
            return l;
        }
    }

    return NULL;
}

static void CL_AddLasers(void)
{
    laser_t     *l;
    entity_t    ent;
    int         i;
    int         time;

    memset(&ent, 0, sizeof(ent));

    for (i = 0, l = cl_lasers; i < MAX_LASERS; i++, l++) {
        time = l->lifetime - (cl.time - l->starttime);
        if (time <= 0) {
            continue;
        }

        if (l->color == -1) {
            ent.rgba = l->rgba;
            ent.alpha = (float)time / (float)l->lifetime;
        } else {
            ent.alpha = 0.30f;
        }

        ent.skinnum = l->color;
        ent.flags = RF_TRANSLUCENT | RF_BEAM;
        VectorCopy(l->start, ent.origin);
        VectorCopy(l->end, ent.oldorigin);
        ent.frame = l->width;

        V_AddEntity(&ent);
    }
}

static void CL_ParseLaser(unsigned colors)
{
    laser_t *l;

    l = CL_AllocLaser();
    if (!l)
        return;

    VectorCopy(te.pos1, l->start);
    VectorCopy(te.pos2, l->end);
    l->lifetime = 100;
    l->color = (colors >> ((Q_rand() % 4) * 8)) & 0xff;
    l->width = 4;
}

/*
==============================================================

BEAM MANAGEMENT

==============================================================
*/

#define MAX_BEAMS   128

typedef struct {
    int         entity;
    int         dest_entity;
    qhandle_t   model;
    int         endtime;
    vec3_t      offset;
    vec3_t      start, end;
} beam_t;

static beam_t   cl_beams[MAX_BEAMS];
static beam_t   cl_playerbeams[MAX_BEAMS];

static void CL_ClearBeams(void)
{
    memset(cl_beams, 0, sizeof(cl_beams));
    memset(cl_playerbeams, 0, sizeof(cl_playerbeams));
}

static void CL_ParseBeam(qhandle_t model)
{
    beam_t  *b;
    int     i;

// override any beam with the same source AND destination entities
    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
        if (b->entity == te.entity1 && b->dest_entity == te.entity2)
            goto override;

// find a free beam
    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
        if (!b->model || b->endtime < cl.time) {
override:
            b->entity = te.entity1;
            b->dest_entity = te.entity2;
            b->model = model;
            b->endtime = cl.time + 200;
            VectorCopy(te.pos1, b->start);
            VectorCopy(te.pos2, b->end);
            VectorCopy(te.offset, b->offset);
            return;
        }
    }
}

static void CL_ParsePlayerBeam(qhandle_t model)
{
    beam_t  *b;
    int     i;

// override any beam with the same entity
    for (i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++) {
        if (b->entity == te.entity1) {
            b->model = model;
            b->endtime = cl.time + 200;
            VectorCopy(te.pos1, b->start);
            VectorCopy(te.pos2, b->end);
            VectorCopy(te.offset, b->offset);
            return;
        }
    }

// find a free beam
    for (i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++) {
        if (!b->model || b->endtime < cl.time) {
            b->entity = te.entity1;
            b->model = model;
            b->endtime = cl.time + 100;     // PMM - this needs to be 100 to prevent multiple heatbeams
            VectorCopy(te.pos1, b->start);
            VectorCopy(te.pos2, b->end);
            VectorCopy(te.offset, b->offset);
            return;
        }
    }
}

void CL_DrawBeam(const vec3_t start, const vec3_t end, qhandle_t model)
{
    int         i, steps;
    vec3_t      dist, angles;
    entity_t    ent;
    float       d, len, model_length;

    // calculate pitch and yaw
    VectorSubtract(end, start, dist);
    vectoangles2(dist, angles);

    // add new entities for the beams
    d = VectorNormalize(dist);
    if (model == cl_mod_lightning) {
        model_length = 35.0f;
        d -= 20.0f; // correction so it doesn't end in middle of tesla
    } else {
        model_length = 30.0f;
    }
    steps = ceilf(d / model_length);

    memset(&ent, 0, sizeof(ent));
    ent.model = model;

    // PMM - special case for lightning model .. if the real length is shorter than the model,
    // flip it around & draw it from the end to the start.  This prevents the model from going
    // through the tesla mine (instead it goes through the target)
    if ((model == cl_mod_lightning) && (steps <= 1)) {
        VectorCopy(end, ent.origin);
        ent.flags = RF_FULLBRIGHT;
        ent.angles[0] = angles[0];
        ent.angles[1] = angles[1];
        ent.angles[2] = Com_SlowRand() % 360;
        V_AddEntity(&ent);
        return;
    }

    if (steps > 1) {
        len = (d - model_length) / (steps - 1);
        VectorScale(dist, len, dist);
    }

    VectorCopy(start, ent.origin);
    for (i = 0; i < steps; i++) {
        if (model == cl_mod_lightning) {
            ent.flags = RF_FULLBRIGHT;
            ent.angles[0] = -angles[0];
            ent.angles[1] = angles[1] + 180.0f;
            ent.angles[2] = Com_SlowRand() % 360;
        } else {
            ent.angles[0] = angles[0];
            ent.angles[1] = angles[1];
            ent.angles[2] = Com_SlowRand() % 360;
        }

        V_AddEntity(&ent);
        VectorAdd(ent.origin, dist, ent.origin);
    }
}

/*
=================
CL_AddBeams
=================
*/
static void CL_AddBeams(void)
{
    int         i;
    beam_t      *b;
    vec3_t      org;

// update beams
    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
        if (!b->model || b->endtime < cl.time)
            continue;

        // if coming from the player, update the start position
        if (b->entity == cl.frame.clientNum + 1)
            VectorAdd(cl.playerEntityOrigin, b->offset, org);
        else
            VectorAdd(b->start, b->offset, org);

        CL_DrawBeam(org, b->end, b->model);
    }
}

/*
=================
CL_AddPlayerBeams

Draw player locked beams.
=================
*/
static void CL_AddPlayerBeams(void)
{
    int         i, j, steps;
    beam_t      *b;
    vec3_t      dist, org;
    float       d;
    entity_t    ent;
    vec3_t      angles;
    float       len;
    int         framenum;
    float       model_length;
    float       hand_multiplier;
    player_state_t  *ps, *ops;

    if (info_hand->integer == 2)
        hand_multiplier = 0;
    else if (info_hand->integer == 1)
        hand_multiplier = -1;
    else
        hand_multiplier = 1;

// update beams
    for (i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++) {
        if (!b->model || b->endtime < cl.time)
            continue;

        // if coming from the player, update the start position
        if (b->entity == cl.frame.clientNum + 1) {
            // set up gun position
            ps = CL_KEYPS;
            ops = CL_OLDKEYPS;

            for (j = 0; j < 3; j++)
                b->start[j] = cl.refdef.vieworg[j] + ops->gunoffset[j] +
                    CL_KEYLERPFRAC * (ps->gunoffset[j] - ops->gunoffset[j]);

            VectorMA(b->start, (hand_multiplier * b->offset[0]), cl.v_right, org);
            VectorMA(org, b->offset[1], cl.v_forward, org);
            VectorMA(org, b->offset[2], cl.v_up, org);
            if (info_hand->integer == 2)
                VectorMA(org, -1, cl.v_up, org);

            // calculate pitch and yaw
            VectorSubtract(b->end, org, dist);

            if (b->model != cl_mod_grapple_cable) {
                // FIXME: don't add offset twice?
                d = VectorLength(dist);
                VectorScale(cl.v_forward, d, dist);
                VectorMA(dist, (hand_multiplier * b->offset[0]), cl.v_right, dist);
                VectorMA(dist, b->offset[1], cl.v_forward, dist);
                VectorMA(dist, b->offset[2], cl.v_up, dist);
                if (info_hand->integer == 2)
                    VectorMA(dist, -1, cl.v_up, dist);
            }

            // FIXME: use cl.refdef.viewangles?
            vectoangles2(dist, angles);

            // if it's the heatbeam, draw the particle effect
            if (cl_mod_heatbeam && b->model == cl_mod_heatbeam)
                CL_Heatbeam(org, dist);

            framenum = 1;
        } else {
            VectorCopy(b->start, org);

            // calculate pitch and yaw
            VectorSubtract(b->end, org, dist);
            vectoangles2(dist, angles);

            // if it's a non-origin offset, it's a player, so use the hardcoded player offset
            if (!VectorEmpty(b->offset)) {
                vec3_t  tmp, f, r, u;

                tmp[0] = -angles[0];
                tmp[1] = angles[1] + 180.0f;
                tmp[2] = 0;
                AngleVectors(tmp, f, r, u);

                VectorMA(org, -b->offset[0] + 1, r, org);
                VectorMA(org, -b->offset[1], f, org);
                VectorMA(org, -b->offset[2] - 10, u, org);
            } else if (cl_mod_heatbeam && b->model == cl_mod_heatbeam) {
                // if it's a monster, do the particle effect
                CL_MonsterPlasma_Shell(b->start);
            }

            framenum = 2;
        }

        // add new entities for the beams
        d = VectorNormalize(dist);
        if (b->model == cl_mod_heatbeam) {
            model_length = 32.0f;
        } else if (b->model == cl_mod_lightning) {
            model_length = 35.0f;
            d -= 20.0f; // correction so it doesn't end in middle of tesla
        } else {
            model_length = 30.0f;
        }

        // correction for grapple cable model, which has origin in the middle
        if (b->entity == cl.frame.clientNum + 1 && b->model == cl_mod_grapple_cable && hand_multiplier) {
            VectorMA(org, model_length * 0.5f, dist, org);
            d -= model_length * 0.5f;
        }

        steps = ceilf(d / model_length);

        memset(&ent, 0, sizeof(ent));
        ent.model = b->model;

        // PMM - special case for lightning model .. if the real length is shorter than the model,
        // flip it around & draw it from the end to the start.  This prevents the model from going
        // through the tesla mine (instead it goes through the target)
        if ((b->model == cl_mod_lightning) && (steps <= 1)) {
            VectorCopy(b->end, ent.origin);
            ent.flags = RF_FULLBRIGHT;
            ent.angles[0] = angles[0];
            ent.angles[1] = angles[1];
            ent.angles[2] = Com_SlowRand() % 360;
            V_AddEntity(&ent);
            continue;
        }

        if (steps > 1) {
            len = (d - model_length) / (steps - 1);
            VectorScale(dist, len, dist);
        }

        VectorCopy(org, ent.origin);
        for (j = 0; j < steps; j++) {
            if (b->model == cl_mod_heatbeam) {
                ent.frame = framenum;
                ent.flags = RF_FULLBRIGHT;
                ent.angles[0] = -angles[0];
                ent.angles[1] = angles[1] + 180.0f;
                ent.angles[2] = cl.time % 360;
            } else if (b->model == cl_mod_lightning) {
                ent.flags = RF_FULLBRIGHT;
                ent.angles[0] = -angles[0];
                ent.angles[1] = angles[1] + 180.0f;
                ent.angles[2] = Com_SlowRand() % 360;
            } else {
                ent.angles[0] = angles[0];
                ent.angles[1] = angles[1];
                ent.angles[2] = Com_SlowRand() % 360;
            }

            V_AddEntity(&ent);
            VectorAdd(ent.origin, dist, ent.origin);
        }
    }
}


/*
==============================================================

SUSTAIN MANAGEMENT

==============================================================
*/

#define MAX_SUSTAINS    32

static cl_sustain_t     cl_sustains[MAX_SUSTAINS];

static void CL_ClearSustains(void)
{
    memset(cl_sustains, 0, sizeof(cl_sustains));
}

static cl_sustain_t *CL_AllocSustain(void)
{
    cl_sustain_t    *s;
    int             i;

    for (i = 0, s = cl_sustains; i < MAX_SUSTAINS; i++, s++) {
        if (s->id == 0)
            return s;
    }

    return NULL;
}

static void CL_ProcessSustain(void)
{
    cl_sustain_t    *s;
    int             i;

    for (i = 0, s = cl_sustains; i < MAX_SUSTAINS; i++, s++) {
        if (s->id) {
            if ((s->endtime >= cl.time) && (cl.time >= s->nextthink))
                s->think(s);
            else if (s->endtime < cl.time)
                s->id = 0;
        }
    }
}

static void CL_ParseSteam(void)
{
    cl_sustain_t    *s;

    if (te.entity1 == -1) {
        CL_ParticleSteamEffect(te.pos1, te.dir, te.color & 0xff, te.count, te.entity2);
        return;
    }

    s = CL_AllocSustain();
    if (!s)
        return;

    s->id = te.entity1;
    s->count = te.count;
    VectorCopy(te.pos1, s->org);
    VectorCopy(te.dir, s->dir);
    s->color = te.color & 0xff;
    s->magnitude = te.entity2;
    s->endtime = cl.time + te.time;
    s->think = CL_ParticleSteamEffect2;
    s->nextthink = cl.time;
}

static void CL_ParseWidow(void)
{
    cl_sustain_t    *s;

    s = CL_AllocSustain();
    if (!s)
        return;

    s->id = te.entity1;
    VectorCopy(te.pos1, s->org);
    s->endtime = cl.time + 2100;
    s->think = CL_Widowbeamout;
    s->nextthink = cl.time;
}

static void CL_ParseNuke(void)
{
    cl_sustain_t    *s;

    s = CL_AllocSustain();
    if (!s)
        return;

    s->id = 21000;
    VectorCopy(te.pos1, s->org);
    s->endtime = cl.time + 1000;
    s->think = CL_Nukeblast;
    s->nextthink = cl.time;
}

//==============================================================

static color_t  railcore_color;
static color_t  railspiral_color;

static cvar_t *cl_railtrail_type;
static cvar_t *cl_railtrail_time;
static cvar_t *cl_railcore_color;
static cvar_t *cl_railcore_width;
static cvar_t *cl_railspiral_color;
static cvar_t *cl_railspiral_radius;

static void cl_railcore_color_changed(cvar_t *self)
{
    if (!SCR_ParseColor(self->string, &railcore_color)) {
        Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
        Cvar_Reset(self);
        railcore_color.u32 = U32_RED;
    }
}

static void cl_railspiral_color_changed(cvar_t *self)
{
    if (!SCR_ParseColor(self->string, &railspiral_color)) {
        Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
        Cvar_Reset(self);
        railspiral_color.u32 = U32_BLUE;
    }
}

static void CL_RailCore(void)
{
    laser_t *l;

    l = CL_AllocLaser();
    if (!l)
        return;

    VectorCopy(te.pos1, l->start);
    VectorCopy(te.pos2, l->end);
    l->color = -1;
    l->lifetime = cl_railtrail_time->integer;
    l->width = cl_railcore_width->integer;
    l->rgba = railcore_color;
}

static void CL_RailSpiral(void)
{
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    vec3_t      right, up;
    int         i;
    float       d, c, s;
    vec3_t      dir;

    VectorCopy(te.pos1, move);
    VectorSubtract(te.pos2, te.pos1, vec);
    len = VectorNormalize(vec);

    MakeNormalVectors(vec, right, up);

    for (i = 0; i < len; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        VectorClear(p->accel);

        d = i * 0.1f;
        c = cosf(d);
        s = sinf(d);

        VectorScale(right, c, dir);
        VectorMA(dir, s, up, dir);

        p->alpha = 1.0f;
        p->alphavel = -1.0f / (cl_railtrail_time->value + frand() * 0.2f);
        p->color = -1;
        p->rgba = railspiral_color;
        for (j = 0; j < 3; j++) {
            p->org[j] = move[j] + dir[j] * cl_railspiral_radius->value;
            p->vel[j] = dir[j] * 6;
        }

        VectorAdd(move, vec, move);
    }
}

static void CL_RailTrail(void)
{
    if (!cl_railtrail_type->integer && te.type != TE_RAILTRAIL2) {
        CL_OldRailTrail();
    } else {
        if (cl_railcore_width->integer > 0) {
            CL_RailCore();
        }
        if (cl_railtrail_type->integer > 1) {
            CL_RailSpiral();
        }
    }
}

static void dirtoangles(vec3_t angles)
{
    angles[0] = RAD2DEG(acosf(te.dir[2]));
    if (te.dir[0])
        angles[1] = RAD2DEG(atan2f(te.dir[1], te.dir[0]));
    else if (te.dir[1] > 0)
        angles[1] = 90;
    else if (te.dir[1] < 0)
        angles[1] = 270;
    else
        angles[1] = 0;
}

/*
=================
CL_ParseTEnt
=================
*/
static const byte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

void CL_ParseTEnt(void)
{
    explosion_t *ex;
    int r;

    switch (te.type) {
    case TE_BLOOD:          // bullet hitting flesh
        if (!(cl_disable_particles->integer & NOPART_BLOOD))
            CL_ParticleEffect(te.pos1, te.dir, 0xe8, 60);
        break;

    case TE_GUNSHOT:            // bullet hitting wall
    case TE_SPARKS:
    case TE_BULLET_SPARKS:
        if (te.type == TE_GUNSHOT)
            CL_ParticleEffect(te.pos1, te.dir, 0, 40);
        else
            CL_ParticleEffect(te.pos1, te.dir, 0xe0, 6);

        if (te.type != TE_SPARKS) {
            CL_SmokeAndFlash(te.pos1);

            // impact sound
            r = Q_rand() & 15;
            if (r == 1)
                S_StartSound(te.pos1, 0, 0, cl_sfx_ric1, 1, ATTN_NORM, 0);
            else if (r == 2)
                S_StartSound(te.pos1, 0, 0, cl_sfx_ric2, 1, ATTN_NORM, 0);
            else if (r == 3)
                S_StartSound(te.pos1, 0, 0, cl_sfx_ric3, 1, ATTN_NORM, 0);
        }
        break;

    case TE_SCREEN_SPARKS:
    case TE_SHIELD_SPARKS:
        if (te.type == TE_SCREEN_SPARKS)
            CL_ParticleEffect(te.pos1, te.dir, 0xd0, 40);
        else
            CL_ParticleEffect(te.pos1, te.dir, 0xb0, 40);
        //FIXME : replace or remove this sound
        S_StartSound(te.pos1, 0, 257, cl_sfx_lashit, 1, ATTN_NORM, 0);
        break;

    case TE_SHOTGUN:            // bullet hitting wall
        CL_ParticleEffect(te.pos1, te.dir, 0, 20);
        CL_SmokeAndFlash(te.pos1);
        break;

    case TE_SPLASH:         // bullet hitting water
        if (te.color < 0 || te.color > 6)
            r = 0x00;
        else
            r = splash_color[te.color];
        CL_ParticleEffect(te.pos1, te.dir, r, te.count);

        if (te.color == SPLASH_SPARKS) {
            r = Q_rand() & 3;
            if (r == 0)
                S_StartSound(te.pos1, 0, 0, cl_sfx_spark5, 1, ATTN_STATIC, 0);
            else if (r == 1)
                S_StartSound(te.pos1, 0, 0, cl_sfx_spark6, 1, ATTN_STATIC, 0);
            else
                S_StartSound(te.pos1, 0, 0, cl_sfx_spark7, 1, ATTN_STATIC, 0);
        }
        break;

    case TE_LASER_SPARKS:
        CL_ParticleEffect2(te.pos1, te.dir, te.color, te.count);
        break;

    case TE_BLUEHYPERBLASTER:   // broken version
        CL_BlasterParticles(te.pos1, te.pos2);
        break;

    case TE_BLUEHYPERBLASTER_2: // fixed version
        CL_BlasterParticles(te.pos1, te.dir);
        break;

    case TE_BLASTER:            // blaster hitting wall
    case TE_BLASTER2:           // green blaster hitting wall
    case TE_FLECHETTE:          // flechette
        ex = CL_AllocExplosion();
        VectorCopy(te.pos1, ex->ent.origin);
        dirtoangles(ex->ent.angles);
        ex->type = ex_misc;
        ex->ent.flags = RF_FULLBRIGHT | RF_TRANSLUCENT;
        switch (te.type) {
        case TE_BLASTER:
            CL_BlasterParticles(te.pos1, te.dir);
            ex->lightcolor[0] = 1;
            ex->lightcolor[1] = 1;
            break;
        case TE_BLASTER2:
            CL_BlasterParticles2(te.pos1, te.dir, 0xd0);
            ex->ent.skinnum = 1;
            ex->lightcolor[1] = 1;
            break;
        case TE_FLECHETTE:
            CL_BlasterParticles2(te.pos1, te.dir, 0x6f);  // 75
            ex->ent.skinnum = 2;
            VectorSet(ex->lightcolor, 0.19f, 0.41f, 0.75f);
            break;
        }
        ex->start = cl.servertime - CL_FRAMETIME;
        ex->light = 150;
        ex->ent.model = cl_mod_explode;
        ex->frames = 4;
        S_StartSound(te.pos1,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
        break;

    case TE_RAILTRAIL:          // railgun effect
    case TE_RAILTRAIL2:
        CL_RailTrail();
        S_StartSound(te.pos2, 0, 0, cl_sfx_railg, 1, ATTN_NORM, 0);
        break;

    case TE_GRENADE_EXPLOSION:
    case TE_GRENADE_EXPLOSION_WATER:
        ex = CL_PlainExplosion();
        ex->frames = 19;
        ex->baseframe = 30;
        if (cl_disable_explosions->integer & NOEXP_GRENADE)
            ex->type = ex_light;

        if (!(cl_disable_particles->integer & NOPART_GRENADE_EXPLOSION))
            CL_ExplosionParticles(te.pos1);

        if (cl_dlight_hacks->integer & DLHACK_SMALLER_EXPLOSION)
            ex->light = 200;

        if (te.type == TE_GRENADE_EXPLOSION_WATER)
            S_StartSound(te.pos1, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
        else
            S_StartSound(te.pos1, 0, 0, cl_sfx_grenexp, 1, ATTN_NORM, 0);
        break;

    case TE_EXPLOSION2:
    case TE_EXPLOSION2_NL:
        ex = CL_PlainExplosion();
        ex->frames = 19;
        ex->baseframe = 30;
        if (te.type == TE_EXPLOSION2_NL)
            ex->light = 0;
        CL_ExplosionParticles(te.pos1);
        S_StartSound(te.pos1, 0, 0, cl_sfx_grenexp, 1, ATTN_NORM, 0);
        break;

    case TE_ROCKET_EXPLOSION:
    case TE_ROCKET_EXPLOSION_WATER:
        ex = CL_PlainExplosion();
        if (cl_disable_explosions->integer & NOEXP_ROCKET)
            ex->type = ex_light;

        if (!(cl_disable_particles->integer & NOPART_ROCKET_EXPLOSION))
            CL_ExplosionParticles(te.pos1);

        if (cl_dlight_hacks->integer & DLHACK_SMALLER_EXPLOSION)
            ex->light = 200;

        if (te.type == TE_ROCKET_EXPLOSION_WATER)
            S_StartSound(te.pos1, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
        else
            S_StartSound(te.pos1, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
        break;

    case TE_EXPLOSION1:
    case TE_EXPLOSION1_NL:
    case TE_PLASMA_EXPLOSION:
        ex = CL_PlainExplosion();
        if (te.type == TE_EXPLOSION1_NL)
            ex->light = 0;
        CL_ExplosionParticles(te.pos1);
        S_StartSound(te.pos1, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
        break;

    case TE_EXPLOSION1_NP:
        CL_PlainExplosion();
        S_StartSound(te.pos1, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
        break;

    case TE_EXPLOSION1_BIG:
        ex = CL_PlainExplosion();
        ex->ent.model = cl_mod_explo4;
        ex->ent.scale = 2.0f;
        S_StartSound(te.pos1, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
        break;

    case TE_BFG_EXPLOSION:
        CL_BFGExplosion(te.pos1);
        break;

    case TE_BFG_BIGEXPLOSION:
        CL_BFGExplosionParticles(te.pos1);
        break;

    case TE_BFG_LASER:
        CL_ParseLaser(0xd0d1d2d3);
        break;

    case TE_BFG_ZAP:
        CL_ParseLaser(0xd0d1d2d3);
        CL_BFGExplosion(te.pos2);
        break;

    case TE_BUBBLETRAIL:
        CL_BubbleTrail(te.pos1, te.pos2);
        break;

    case TE_PARASITE_ATTACK:
    case TE_MEDIC_CABLE_ATTACK:
        VectorClear(te.offset);
        te.entity2 = 0;
        CL_ParseBeam(cl_mod_parasite_segment);
        break;

    case TE_BOSSTPORT:          // boss teleporting to station
        CL_BigTeleportParticles(te.pos1);
        S_StartSound(te.pos1, 0, 0, S_RegisterSound("misc/bigtele.wav"), 1, ATTN_NONE, 0);
        break;

    case TE_GRAPPLE_CABLE:
        te.entity2 = 0;
        CL_ParseBeam(cl_mod_grapple_cable);
        break;

    case TE_WELDING_SPARKS:
        CL_ParticleEffect2(te.pos1, te.dir, te.color, te.count);

        ex = CL_AllocExplosion();
        VectorCopy(te.pos1, ex->ent.origin);
        ex->type = ex_flash;
        // note to self
        // we need a better no draw flag
        ex->ent.flags = RF_BEAM;
        ex->start = cl.servertime - CL_FRAMETIME;
        ex->light = 100 + (Q_rand() % 75);
        VectorSet(ex->lightcolor, 1.0f, 1.0f, 0.3f);
        ex->ent.model = cl_mod_flash;
        ex->frames = 2;
        break;

    case TE_GREENBLOOD:
        CL_ParticleEffect2(te.pos1, te.dir, 0xdf, 30);
        break;

    case TE_TUNNEL_SPARKS:
        CL_ParticleEffect3(te.pos1, te.dir, te.color, te.count);
        break;

    case TE_LIGHTNING:
        S_StartSound(NULL, te.entity1, CHAN_WEAPON, cl_sfx_lightning, 1, ATTN_NORM, 0);
        VectorClear(te.offset);
        CL_ParseBeam(cl_mod_lightning);
        break;

    case TE_DEBUGTRAIL:
        CL_DebugTrail(te.pos1, te.pos2);
        break;

    case TE_PLAIN_EXPLOSION:
        CL_PlainExplosion();
        break;

    case TE_FLASHLIGHT:
        CL_Flashlight(te.entity1, te.pos1);
        break;

    case TE_FORCEWALL:
        CL_ForceWall(te.pos1, te.pos2, te.color);
        break;

    case TE_HEATBEAM:
        VectorSet(te.offset, 2, 7, -3);
        CL_ParsePlayerBeam(cl_mod_heatbeam);
        break;

    case TE_MONSTER_HEATBEAM:
        VectorClear(te.offset);
        CL_ParsePlayerBeam(cl_mod_heatbeam);
        break;

    case TE_HEATBEAM_SPARKS:
        CL_ParticleSteamEffect(te.pos1, te.dir, 0x8, 50, 60);
        S_StartSound(te.pos1,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
        break;

    case TE_HEATBEAM_STEAM:
        CL_ParticleSteamEffect(te.pos1, te.dir, 0xE0, 20, 60);
        S_StartSound(te.pos1,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
        break;

    case TE_STEAM:
        CL_ParseSteam();
        break;

    case TE_BUBBLETRAIL2:
        CL_BubbleTrail2(te.pos1, te.pos2, 8);
        S_StartSound(te.pos1,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
        break;

    case TE_MOREBLOOD:
        CL_ParticleEffect(te.pos1, te.dir, 0xe8, 250);
        break;

    case TE_CHAINFIST_SMOKE:
        VectorSet(te.dir, 0, 0, 1);
        CL_ParticleSmokeEffect(te.pos1, te.dir, 0, 20, 20);
        break;

    case TE_ELECTRIC_SPARKS:
        CL_ParticleEffect(te.pos1, te.dir, 0x75, 40);
        //FIXME : replace or remove this sound
        S_StartSound(te.pos1, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
        break;

    case TE_TRACKER_EXPLOSION:
        CL_ColorFlash(te.pos1, 0, 150, -1, -1, -1);
        CL_ColorExplosionParticles(te.pos1, 0, 1);
        S_StartSound(te.pos1, 0, 0, cl_sfx_disrexp, 1, ATTN_NORM, 0);
        break;

    case TE_TELEPORT_EFFECT:
    case TE_DBALL_GOAL:
        CL_TeleportParticles(te.pos1);
        break;

    case TE_WIDOWBEAMOUT:
        CL_ParseWidow();
        break;

    case TE_NUKEBLAST:
        CL_ParseNuke();
        break;

    case TE_WIDOWSPLASH:
        CL_WidowSplash();
        break;

    case TE_BERSERK_SLAM:
        CL_BerserkSlamParticles(te.pos1, te.dir);

        ex = CL_AllocExplosion();
        VectorCopy(te.pos1, ex->ent.origin);
        dirtoangles(ex->ent.angles);
        ex->type = ex_misc;
        ex->ent.model = cl_mod_explode;
        ex->ent.flags = RF_FULLBRIGHT | RF_TRANSLUCENT;
        ex->ent.scale = 3;
        ex->ent.skinnum = 2;
        ex->start = cl.servertime - CL_FRAMETIME;
        ex->light = 550;
        VectorSet(ex->lightcolor, 0.19f, 0.41f, 0.75f);
        ex->frames = 4;
        break;

    case TE_GRAPPLE_CABLE_2:
        VectorSet(te.offset, 9, 12, -3);
        CL_ParsePlayerBeam(cl_mod_grapple_cable);
        break;

    case TE_LIGHTNING_BEAM:
        VectorSet(te.offset, 0, 12, -12);
        CL_ParsePlayerBeam(cl_mod_lightning);
        break;

    case TE_POWER_SPLASH:
        CL_PowerSplash();
        break;

    case TE_DAMAGE_DEALT:
        if (te.count > 0 && cl_hit_markers->integer > 0) {
            cl.hit_marker_time = cls.realtime;
            cl.hit_marker_count = te.count;
            if (cl_hit_markers->integer > 1)
                S_StartSound(NULL, listener_entnum, 257, cl_sfx_hit_marker, 1, ATTN_NONE, 0);
        }
        break;

    default:
        Com_Error(ERR_DROP, "%s: bad type", __func__);
    }
}

/*
=================
CL_AddTEnts
=================
*/
void CL_AddTEnts(void)
{
    CL_AddBeams();
    CL_AddPlayerBeams();
    CL_AddExplosions();
    CL_ProcessSustain();
    CL_AddLasers();
}

/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts(void)
{
    CL_ClearBeams();
    CL_ClearExplosions();
    CL_ClearLasers();
    CL_ClearSustains();
}

void CL_InitTEnts(void)
{
    cl_muzzleflashes = Cvar_Get("cl_muzzleflashes", "1", 0);
    cl_hit_markers = Cvar_Get("cl_hit_markers", "2", 0);
    cl_railtrail_type = Cvar_Get("cl_railtrail_type", "0", 0);
    cl_railtrail_time = Cvar_Get("cl_railtrail_time", "1.0", 0);
    cl_railtrail_time->changed = cl_timeout_changed;
    cl_railtrail_time->changed(cl_railtrail_time);
    cl_railcore_color = Cvar_Get("cl_railcore_color", "red", 0);
    cl_railcore_color->changed = cl_railcore_color_changed;
    cl_railcore_color->generator = Com_Color_g;
    cl_railcore_color_changed(cl_railcore_color);
    cl_railcore_width = Cvar_Get("cl_railcore_width", "2", 0);
    cl_railspiral_color = Cvar_Get("cl_railspiral_color", "blue", 0);
    cl_railspiral_color->changed = cl_railspiral_color_changed;
    cl_railspiral_color->generator = Com_Color_g;
    cl_railspiral_color_changed(cl_railspiral_color);
    cl_railspiral_radius = Cvar_Get("cl_railspiral_radius", "3", 0);
}
