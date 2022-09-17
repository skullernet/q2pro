/*
Copyright (C) 2003-2008 Andrey Nazarov

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

#include "ui.h"
#include "src/baseq2/m_player.h"


/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/

#define ID_MODEL 103
#define ID_SKIN  104
#define ID_ANIM  105

static cvar_t rot_speed;

typedef struct m_player_s {
    menuFrameWork_t     menu;
    menuField_t         name;
    menuSpinControl_t   model;
    menuSpinControl_t   skin;
    menuSpinControl_t   hand;
    menuSpinControl_t   anim;
    menuSlider_t        rotspeed;

    refdef_t    refdef;
    entity_t    entities[2];

    int        time;
    int        oldTime;
    int        frame_start;
    int        num_frames;

    char *pmnames[MAX_PLAYERMODELS];
} m_player_t;

static m_player_t    m_player;

static const char *handedness[] = {
    "right",
    "left",
    "center",
    NULL
};

static const char *anim_names[] = {
    "stand", "run", "attack", "pain 1", "pain 2", "pain 3",
    "jump", "flipoff", "salute", "taunt", "wave", "point",
    "crouch stand", "crouch walk", "crouch attack", "crouch pain", "crouch death",
    "death 1", "death 2", "death 3", NULL
};

static const int anim_num_frames[] = {
    40, 6, 8, 4, 4, 4,
    6, 12, 11, 17, 11, 12,
    19, 6, 9, 4, 5,
    6, 6, 8
};

static const int anim_frame_start[] = {
    FRAME_stand01, FRAME_run1, FRAME_attack1, FRAME_pain101, FRAME_pain201, FRAME_pain301,
    FRAME_jump1, FRAME_flip01, FRAME_salute01, FRAME_taunt01, FRAME_wave01, FRAME_point01,
    FRAME_crstnd01, FRAME_crwalk1, FRAME_crattak1, FRAME_crpain1, FRAME_crdeath1,
    FRAME_death101, FRAME_death201, FRAME_death301
};

static void ReloadMedia(void)
{
    char scratch[MAX_QPATH];
    char *model = uis.pmi[m_player.model.curvalue].directory;
    char *skin = uis.pmi[m_player.model.curvalue].skindisplaynames[m_player.skin.curvalue];

    m_player.refdef.num_entities = 0;

    Q_concat(scratch, sizeof(scratch), "players/", model, "/tris.md2");
    m_player.entities[0].model = R_RegisterModel(scratch);
    if (!m_player.entities[0].model)
        return;

    m_player.refdef.num_entities++;

    Q_concat(scratch, sizeof(scratch), "players/", model, "/", skin, ".pcx");
    m_player.entities[0].skin = R_RegisterSkin(scratch);

    if (!uis.weaponModel[0])
        return;

    Q_concat(scratch, sizeof(scratch), "players/", model, "/", uis.weaponModel);
    m_player.entities[1].model = R_RegisterModel(scratch);
    if (!m_player.entities[1].model)
        return;

    m_player.refdef.num_entities++;
}

static int oldtime;

static void RunFrame(void)
{
    int frame;
    int old_frame;
    int num_frames;
    int anim;
    bool is_death;
    int i;
    float d_angle;
    float d_sec;
    float rotspeed = m_player.rotspeed.curvalue;
    
    if (oldtime == 0) oldtime = uis.realtime;
    d_sec = (uis.realtime - oldtime) / 1000.0f;
    oldtime = uis.realtime;
    
    d_angle = rotspeed * d_sec;
    
    m_player.entities[0].angles[1] += d_angle;
    if (m_player.entities[0].angles[1] > 360.0f)
        m_player.entities[0].angles[1] -= 360.0f;
	    	
    m_player.entities[1].angles[1] = m_player.entities[0].angles[1];

    if (m_player.time < uis.realtime) {
        m_player.oldTime = m_player.time;

        m_player.time += 120;
        if (m_player.time < uis.realtime) {
            m_player.time = uis.realtime;
        }
        
        num_frames = m_player.num_frames;
        anim = m_player.frame_start;
        is_death = anim == FRAME_crdeath1 || anim == FRAME_death101
                || anim == FRAME_death201 || anim == FRAME_death301;

        if (is_death) num_frames += 20; // Stay on death pose for a bit
        frame = (m_player.time / 120) % num_frames;
        if (is_death) clamp(frame, 0, m_player.num_frames-1);
        frame += anim;

        for (i = 0; i < m_player.refdef.num_entities; i++) {
            old_frame = m_player.entities[i].frame;
            if (is_death && frame == anim) old_frame = FRAME_stand01;
            m_player.entities[i].oldframe = old_frame;
            m_player.entities[i].frame = frame;
        }
    }
}

static void Draw(menuFrameWork_t *self)
{
    float backlerp;
    int i;

    m_player.refdef.time = uis.realtime * 0.001f;

    RunFrame();

    if (m_player.time == m_player.oldTime) {
        backlerp = 0;
    } else {
        backlerp = 1 - (float)(uis.realtime - m_player.oldTime) /
                   (float)(m_player.time - m_player.oldTime);
    }

    for (i = 0; i < m_player.refdef.num_entities; i++) {
        m_player.entities[i].backlerp = backlerp;
    }

    Menu_Draw(self);

    R_RenderFrame(&m_player.refdef);

    R_SetScale(uis.scale);
}

static void Size(menuFrameWork_t *self)
{
    int w = uis.width / uis.scale;
    int h = uis.height / uis.scale;
    int x = uis.width / 2;
    int y = uis.height / 2 - MENU_SPACING * 5 / 2;

    m_player.refdef.x = w / 2;
    m_player.refdef.y = h / 10;
    m_player.refdef.width = w / 2;
    m_player.refdef.height = h - h / 5;

    m_player.refdef.fov_x = 90;
    m_player.refdef.fov_y = V_CalcFov(m_player.refdef.fov_x,
                                      m_player.refdef.width, m_player.refdef.height);

    if (uis.width < 800 && uis.width >= 640) {
        x -= CHAR_WIDTH * 10;
    }

    if (m_player.menu.banner) {
        h = GENERIC_SPACING(m_player.menu.banner_rc.height);
        m_player.menu.banner_rc.x = x - m_player.menu.banner_rc.width / 2;
        m_player.menu.banner_rc.y = y - h / 2;
        y += h / 2;
    }

    if (uis.width < 640) {
        x -= CHAR_WIDTH * 10;
        m_player.hand.generic.name = "hand";
    } else {
        m_player.hand.generic.name = "handedness";
    }

    m_player.name.generic.x     = x;
    m_player.name.generic.y     = y;
    y += MENU_SPACING * 2;

    m_player.model.generic.x    = x;
    m_player.model.generic.y    = y;
    y += MENU_SPACING;

    m_player.skin.generic.x     = x;
    m_player.skin.generic.y     = y;
    y += MENU_SPACING;

    m_player.hand.generic.x     = x;
    m_player.hand.generic.y     = y;
    y += MENU_SPACING;
    
    m_player.anim.generic.x			= x;
    m_player.anim.generic.y			= y;
    y += MENU_SPACING;
    
    m_player.rotspeed.generic.x = x;
    m_player.rotspeed.generic.y = y;
}

static menuSound_t Change(menuCommon_t *self)
{
    switch (self->id) {
    case ID_MODEL:
        m_player.skin.itemnames =
            uis.pmi[m_player.model.curvalue].skindisplaynames;
        m_player.skin.curvalue = 0;
        SpinControl_Init(&m_player.skin);
        // fall through
    case ID_SKIN:
        ReloadMedia();
        break;
    case ID_ANIM:
        m_player.frame_start = anim_frame_start[m_player.anim.curvalue];
        m_player.num_frames = anim_num_frames[m_player.anim.curvalue];
        break;
    default:
        break;
    }
    return QMS_MOVE;
}

static void Pop(menuFrameWork_t *self)
{
    char scratch[MAX_OSPATH];

    Cvar_SetEx("name", m_player.name.field.text, FROM_CONSOLE);

    Q_concat(scratch, sizeof(scratch),
             uis.pmi[m_player.model.curvalue].directory, "/",
             uis.pmi[m_player.model.curvalue].skindisplaynames[m_player.skin.curvalue]);

    Cvar_SetEx("skin", scratch, FROM_CONSOLE);

    Cvar_SetEx("hand", va("%d", m_player.hand.curvalue), FROM_CONSOLE);
}

static bool Push(menuFrameWork_t *self)
{
    char currentdirectory[MAX_QPATH];
    char currentskin[MAX_QPATH];
    int i, j;
    int currentdirectoryindex = 0;
    int currentskinindex = 0;
    char *p;

    // find and register all player models
    if (!uis.numPlayerModels) {
        PlayerModel_Load();
        if (!uis.numPlayerModels) {
            Com_Printf("No player models found.\n");
            return false;
        }
    }

    Cvar_VariableStringBuffer("skin", currentdirectory, sizeof(currentdirectory));

    if ((p = strchr(currentdirectory, '/')) || (p = strchr(currentdirectory, '\\'))) {
        *p++ = 0;
        Q_strlcpy(currentskin, p, sizeof(currentskin));
    } else {
        strcpy(currentdirectory, "male");
        strcpy(currentskin, "grunt");
    }

    for (i = 0; i < uis.numPlayerModels; i++) {
        m_player.pmnames[i] = uis.pmi[i].directory;
        if (Q_stricmp(uis.pmi[i].directory, currentdirectory) == 0) {
            currentdirectoryindex = i;

            for (j = 0; j < uis.pmi[i].nskins; j++) {
                if (Q_stricmp(uis.pmi[i].skindisplaynames[j], currentskin) == 0) {
                    currentskinindex = j;
                    break;
                }
            }
        }
    }

    IF_Init(&m_player.name.field, m_player.name.width, m_player.name.width);
    IF_Replace(&m_player.name.field, Cvar_VariableString("name"));

    m_player.model.curvalue = currentdirectoryindex;
    m_player.model.itemnames = m_player.pmnames;

    m_player.skin.curvalue = currentskinindex;
    m_player.skin.itemnames = uis.pmi[currentdirectoryindex].skindisplaynames;

    m_player.hand.curvalue = Cvar_VariableInteger("hand");
    clamp(m_player.hand.curvalue, 0, 2);
    
    m_player.anim.curvalue = 0;
    m_player.frame_start = FRAME_stand01;
    m_player.num_frames = anim_num_frames[0];

    m_player.menu.banner = R_RegisterPic("m_banner_plauer_setup");
    if (m_player.menu.banner) {
        R_GetPicSize(&m_player.menu.banner_rc.width,
                     &m_player.menu.banner_rc.height, m_player.menu.banner);
        m_player.menu.title = NULL;
    } else {
        m_player.menu.title = "Player Setup";
    }

    ReloadMedia();

    // set up oldframe correctly
    m_player.time = uis.realtime - 120;
    m_player.oldTime = m_player.time;
    RunFrame();

    return true;
}

static void Free(menuFrameWork_t *self)
{
    Z_Free(m_player.menu.items);
    memset(&m_player, 0, sizeof(m_player));
}

void M_Menu_PlayerConfig(void)
{
    static const vec3_t origin = { 40.0f, 0.0f, 0.0f };
    static const vec3_t angles = { 0.0f, 260.0f, 0.0f };

    m_player.menu.name = "players";
    m_player.menu.push = Push;
    m_player.menu.pop = Pop;
    m_player.menu.size = Size;
    m_player.menu.draw = Draw;
    m_player.menu.free = Free;
    m_player.menu.image = uis.backgroundHandle;
    m_player.menu.color.u32 = uis.color.background.u32;
    m_player.menu.transparent = uis.transparent;

    m_player.entities[0].flags = RF_FULLBRIGHT;
    VectorCopy(angles, m_player.entities[0].angles);
    VectorCopy(origin, m_player.entities[0].origin);
    VectorCopy(origin, m_player.entities[0].oldorigin);

    m_player.entities[1].flags = RF_FULLBRIGHT;
    VectorCopy(angles, m_player.entities[1].angles);
    VectorCopy(origin, m_player.entities[1].origin);
    VectorCopy(origin, m_player.entities[1].oldorigin);

    m_player.refdef.num_entities = 0;
    m_player.refdef.entities = m_player.entities;
    m_player.refdef.rdflags = RDF_NOWORLDMODEL;

    m_player.name.generic.type = MTYPE_FIELD;
    m_player.name.generic.flags = QMF_HASFOCUS;
    m_player.name.generic.name = "name";
    m_player.name.width = MAX_CLIENT_NAME - 1;

    m_player.model.generic.type = MTYPE_SPINCONTROL;
    m_player.model.generic.id = ID_MODEL;
    m_player.model.generic.name = "model";
    m_player.model.generic.change = Change;

    m_player.skin.generic.type = MTYPE_SPINCONTROL;
    m_player.skin.generic.id = ID_SKIN;
    m_player.skin.generic.name = "skin";
    m_player.skin.generic.change = Change;

    m_player.hand.generic.type = MTYPE_SPINCONTROL;
    m_player.hand.generic.name = "handedness";
    m_player.hand.itemnames = (char **)handedness;
    
    m_player.anim.generic.type = MTYPE_SPINCONTROL;
    m_player.anim.generic.id = ID_ANIM;
    m_player.anim.generic.name = "animation";
    m_player.anim.itemnames = (char **)anim_names;
    m_player.anim.generic.change = Change;
    
    m_player.rotspeed.generic.type = MTYPE_SLIDER;
    m_player.rotspeed.generic.name = "rotation speed";
    m_player.rotspeed.cvar = &rot_speed;
    m_player.rotspeed.minvalue = 0.0f;
    m_player.rotspeed.maxvalue = 90.0f;
    m_player.rotspeed.step = 90.0f / SLIDER_RANGE;

    Menu_AddItem(&m_player.menu, &m_player.name);
    Menu_AddItem(&m_player.menu, &m_player.model);
    Menu_AddItem(&m_player.menu, &m_player.skin);
    Menu_AddItem(&m_player.menu, &m_player.hand);
    Menu_AddItem(&m_player.menu, &m_player.anim);
    Menu_AddItem(&m_player.menu, &m_player.rotspeed);

    List_Append(&ui_menus, &m_player.menu.entry);
}
