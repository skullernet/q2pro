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

//
// cl_precache.c
//

#include "client.h"

#if 0
// Testing for CL_ParsePlayerSkin()
static void test_parse_player_skin(const char* string, bool parse_dogtag, const char* expect_name, const char* expect_model, const char* expect_skin, const char* expect_dogtag)
{
    char name[MAX_QPATH];
    char model[MAX_QPATH];
    char skin[MAX_QPATH];
    char dogtag[MAX_QPATH];

    CL_ParsePlayerSkin(name, model, skin, dogtag, parse_dogtag, string);
    Q_assert(strcmp(name, expect_name) == 0);
    Q_assert(strcmp(model, expect_model) == 0);
    Q_assert(strcmp(skin, expect_skin) == 0);
    Q_assert(strcmp(dogtag, expect_dogtag) == 0);
}

void test_CL_ParsePlayerSkin(void)
{
    test_parse_player_skin("unnamed\\male/grunt\\default", true, "unnamed", "male", "grunt", "default");
    test_parse_player_skin("unnamed\\male/grunt", false, "unnamed", "male", "grunt", "default");
    test_parse_player_skin("unnamed\\male\\grunt", false, "unnamed", "male", "grunt", "default");

    test_parse_player_skin("unnamed\\/grunt\\default", true, "unnamed", "male", "grunt", "default");
    test_parse_player_skin("unnamed\\/grunt", false, "unnamed", "male", "grunt", "default");

    test_parse_player_skin("Name\\Model/Skin\\Dogtag", true, "Name", "Model", "Skin", "Dogtag");

    test_parse_player_skin("Name\\Model\\Dogtag", true, "Name", "male", "grunt", "Dogtag");
    test_parse_player_skin("Name\\\\Dogtag", true, "Name", "male", "grunt", "Dogtag");
    test_parse_player_skin("Name\\\\", true, "Name", "male", "grunt", "default");

    test_parse_player_skin("Name\\Model/Skin\\", true, "Name", "Model", "Skin", "default");
    test_parse_player_skin("Name\\male\\Dogtag", true, "Name", "male", "grunt", "Dogtag");
    test_parse_player_skin("Name\\female\\Dogtag", true, "Name", "female", "athena", "Dogtag");
}
#endif

/*
================
CL_ParsePlayerSkin

Breaks up playerskin into name (optional), model and skin components.
If model or skin are found to be invalid, replaces them with sane defaults.
================
*/
void CL_ParsePlayerSkin(char *name, char *model, char *skin, char *dogtag, bool parse_dogtag, const char *s)
{
    char buf[MAX_QPATH * 4];
    size_t len;
    char *t;

    len = strlen(s);
    Q_assert(len < sizeof(buf));
    Q_strlcpy(buf, s, sizeof(buf));

    // isolate the player's name
    size_t name_len;
    char *model_str;
    t = strchr(buf, '\\');
    if (t) {
        name_len = t - buf;
        *t = 0;
        model_str = t + 1;
    } else {
        model_str = buf;
        name_len = 0;
    }

    char *skin_str = NULL;
    // isolate the model name
    t = strchr(model_str, '/');
    if (!t && !parse_dogtag) {
        /* Using '\\' as a separator for the skin name is technically incorrect;
         * even early game code always produced "model/skin", yet the backslash
         * was considered as a separator.
         * This means it's probably a compatibility measure for even earlier code,
         * and probably not needed any more...
         * Still, keep it for compatibility's sake when dealing with userinfo
         * from non-rerelease servers. */
        t = strchr(model, '\\');
    }
    if (t) {
        skin_str = t + 1;
        *t = 0;
    }

    // isolate the dogtag name
    const char *dogtag_str = "";
    if (parse_dogtag) {
        char *search_str = skin_str ? skin_str : model_str;
        t = strchr(search_str, '\\');
        if (t) {
            dogtag_str = t + 1;
            *t = 0;
        }
    }

    // copy the player's name
    if (name) {
        Q_strnlcpy(name, buf, name_len, MAX_QPATH);
    }
    // fix empty model to male
    if (!*model_str)
        Q_strlcpy(model, "male", MAX_QPATH);
    else
        Q_strlcpy(model, model_str, MAX_QPATH);
    if (skin_str)
        Q_strlcpy(skin, skin_str, MAX_QPATH);
    else
        skin[0] = 0;
    if (!*dogtag_str)
        Q_strlcpy(dogtag, "default", MAX_QPATH);
    else
        Q_strlcpy(dogtag, dogtag_str, MAX_QPATH);

    // apply restrictions on skins
    if (cl_noskins->integer == 2 || !COM_IsPath(skin))
        goto default_skin;

    if (cl_noskins->integer || !COM_IsPath(model))
        goto default_model;

    return;

default_skin:
    if (!Q_stricmp(model, "female")) {
        Q_strlcpy(model, "female", MAX_QPATH);
        Q_strlcpy(skin, "athena", MAX_QPATH);
    } else {
default_model:
        Q_strlcpy(model, "male", MAX_QPATH);
        Q_strlcpy(skin, "grunt", MAX_QPATH);
    }
}

/*
================
CL_LoadClientinfo

================
*/
void CL_LoadClientinfo(clientinfo_t *ci, const char *s)
{
    int         i;
    char        model_name[MAX_QPATH];
    char        skin_name[MAX_QPATH];
    char        dogtag_name[MAX_QPATH];
    char        model_filename[MAX_QPATH];
    char        skin_filename[MAX_QPATH];
    char        weapon_filename[MAX_QPATH];
    char        icon_filename[MAX_QPATH];
    char        dogtag_filename[MAX_QPATH];
    bool        parse_dogtag = cls.serverProtocol == PROTOCOL_VERSION_RERELEASE;

    CL_ParsePlayerSkin(ci->name, model_name, skin_name, dogtag_name, parse_dogtag, s);

    // model file
    Q_concat(model_filename, sizeof(model_filename),
             "players/", model_name, "/tris.md2");
    ci->model = R_RegisterModel(model_filename);
    if (!ci->model && Q_stricmp(model_name, "male")) {
        strcpy(model_name, "male");
        strcpy(model_filename, "players/male/tris.md2");
        ci->model = R_RegisterModel(model_filename);
    }

    // skin file
    Q_concat(skin_filename, sizeof(skin_filename),
             "players/", model_name, "/", skin_name, ".pcx");
    ci->skin = R_RegisterSkin(skin_filename);

    // if we don't have the skin and the model was female,
    // see if athena skin exists
    if (!ci->skin && !Q_stricmp(model_name, "female")) {
        strcpy(skin_name, "athena");
        strcpy(skin_filename, "players/female/athena.pcx");
        ci->skin = R_RegisterSkin(skin_filename);
    }

    // if we don't have the skin and the model wasn't male,
    // see if the male has it (this is for CTF's skins)
    if (!ci->skin && Q_stricmp(model_name, "male")) {
        // change model to male
        strcpy(model_name, "male");
        strcpy(model_filename, "players/male/tris.md2");
        ci->model = R_RegisterModel(model_filename);

        // see if the skin exists for the male model
        Q_concat(skin_filename, sizeof(skin_filename),
                 "players/male/", skin_name, ".pcx");
        ci->skin = R_RegisterSkin(skin_filename);
    }

    // if we still don't have a skin, it means that the male model
    // didn't have it, so default to grunt
    if (!ci->skin) {
        // see if the skin exists for the male model
        strcpy(skin_name, "grunt");
        strcpy(skin_filename, "players/male/grunt.pcx");
        ci->skin = R_RegisterSkin(skin_filename);
    }

    // weapon file
    for (i = 0; i < cl.numWeaponModels; i++) {
        Q_concat(weapon_filename, sizeof(weapon_filename),
                 "players/", model_name, "/", cl.weaponModels[i]);
        ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
        if (!ci->weaponmodel[i] && !Q_stricmp(model_name, "cyborg")) {
            // try male
            Q_concat(weapon_filename, sizeof(weapon_filename),
                     "players/male/", cl.weaponModels[i]);
            ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
        }
    }

    // icon file
    Q_concat(icon_filename, sizeof(icon_filename),
             "/players/", model_name, "/", skin_name, "_i.pcx");
    Q_strlcpy(ci->icon_name, icon_filename, sizeof(ci->icon_name));

    strcpy(ci->model_name, model_name);
    strcpy(ci->skin_name, skin_name);
    Q_concat(dogtag_filename, sizeof(dogtag_filename), dogtag_name, ".pcx");
    Q_strlcpy(ci->dogtag_name, dogtag_filename, sizeof(ci->dogtag_name));

    // base info should be at least partially valid
    if (ci == &cl.baseclientinfo)
        return;

    // must have loaded all data types to be valid
    if (!ci->skin || !ci->model || !ci->weaponmodel[0]) {
        ci->skin = 0;
        ci->icon_name[0] = 0;
        ci->model = 0;
        ci->weaponmodel[0] = 0;
        ci->model_name[0] = 0;
        ci->skin_name[0] = 0;
        ci->dogtag_name[0] = 0;
    }
}

/*
=================
CL_RegisterSounds
=================
*/
void CL_RegisterSounds(void)
{
    int i;
    char    *s;

    S_BeginRegistration();
    CL_RegisterTEntSounds();
    for (i = 1; i < cl.csr.max_sounds; i++) {
        s = cl.configstrings[cl.csr.sounds + i];
        if (!s[0])
            break;
        cl.sound_precache[i] = S_RegisterSound(s);
    }
    S_EndRegistration();
}

/*
=================
CL_RegisterBspModels

Registers main BSP file and inline models
=================
*/
void CL_RegisterBspModels(void)
{
    char *name = cl.configstrings[cl.csr.models + 1];
    int i, ret;

    if (!name[0]) {
        Com_Error(ERR_DROP, "%s: no map set", __func__);
    }
    ret = BSP_Load(name, &cl.bsp);
    if (cl.bsp == NULL) {
        Com_Error(ERR_DROP, "Couldn't load %s: %s", name, BSP_ErrorString(ret));
    }

    if (cl.bsp->checksum != Q_atoi(cl.configstrings[cl.csr.mapchecksum])) {
        if (cls.demo.playback) {
            Com_WPrintf("Local map version differs from demo: %i != %s\n",
                        cl.bsp->checksum, cl.configstrings[cl.csr.mapchecksum]);
        } else {
            Com_Error(ERR_DROP, "Local map version differs from server: %i != %s",
                      cl.bsp->checksum, cl.configstrings[cl.csr.mapchecksum]);
        }
    }

    for (i = 2; i < cl.csr.max_models; i++) {
        name = cl.configstrings[cl.csr.models + i];
        if (!name[0] && i != MODELINDEX_PLAYER) {
            break;
        }
        if (name[0] == '*')
            cl.model_clip[i] = BSP_InlineModel(cl.bsp, name);
        else
            cl.model_clip[i] = NULL;
    }
}

/*
=================
CL_RegisterVWepModels

Builds a list of visual weapon models
=================
*/
void CL_RegisterVWepModels(void)
{
    int         i;
    char        *name;

    cl.numWeaponModels = 1;
    strcpy(cl.weaponModels[0], "weapon.md2");

    // only default model when vwep is off
    if (!cl_vwep->integer) {
        return;
    }

    for (i = 2; i < cl.csr.max_models; i++) {
        name = cl.configstrings[cl.csr.models + i];
        if (!name[0] && i != MODELINDEX_PLAYER) {
            break;
        }
        if (name[0] != '#') {
            continue;
        }

        // special player weapon model
        Q_strlcpy(cl.weaponModels[cl.numWeaponModels++], name + 1, sizeof(cl.weaponModels[0]));

        if (cl.numWeaponModels == MAX_CLIENTWEAPONMODELS) {
            break;
        }
    }
}

/*
=================
CL_SetSky

=================
*/
void CL_SetSky(void)
{
    float       rotate = 0;
    int         autorotate = 1;
    vec3_t      axis;

    if (cl.csr.extended)
        sscanf(cl.configstrings[CS_SKYROTATE], "%f %d", &rotate, &autorotate);
    else
        rotate = atof(cl.configstrings[CS_SKYROTATE]);

    if (sscanf(cl.configstrings[CS_SKYAXIS], "%f %f %f",
               &axis[0], &axis[1], &axis[2]) != 3) {
        Com_DPrintf("Couldn't parse CS_SKYAXIS\n");
        VectorClear(axis);
    }

    if (!cl.bsp->classic_sky || !R_SetClassicSky(cl.bsp->classic_sky))
        R_SetSky(cl.configstrings[CS_SKY], rotate, autorotate, axis);
}

/*
=================
CL_RegisterImage

Hack to handle RF_CUSTOMSKIN for remaster
=================
*/
static qhandle_t CL_RegisterImage(const char *s)
{
    // if it's in a subdir and has an extension, it's either a sprite or a skin
    // allow /some/pic.pcx escape syntax
    if (cl.csr.extended && *s != '/' && *s != '\\' && *COM_FileExtension(s)) {
        if (!FS_pathcmpn(s, CONST_STR_LEN("sprites/")))
            return R_RegisterSprite(s);
        if (strchr(s, '/'))
            return R_RegisterSkin(s);
    }

    return R_RegisterTempPic(s);
}

#define MAX_WHEEL_VALUES 8

/*
=================
CL_LoadWheelIcons
=================
*/
static cl_wheel_icon_t CL_LoadWheelIcons(int icon_index)
{
    cl_wheel_icon_t icons = { .main = cl.image_precache[icon_index] };

    char path[MAX_QPATH];
    Q_snprintf(path, sizeof(path), "wheel/%s", cl.configstrings[cl.csr.images + icon_index]);

    icons.wheel = R_RegisterTempPic(path);

    if (!icons.wheel) {
        icons.wheel = icons.selected = icons.main;
    } else {
        Q_snprintf(path, sizeof(path), "wheel/%s_selected", cl.configstrings[cl.csr.images + icon_index]);

        icons.selected = R_RegisterTempPic(path);

        if (!icons.selected) {
            icons.selected = icons.wheel;
        }
    }

    return icons;
}

/*
=================
CL_LoadWheelEntry
=================
*/
static void CL_LoadWheelEntry(int index, const char *s)
{
    configstring_t entry;
    Q_strlcpy(entry, s, sizeof(entry));
    int values[MAX_WHEEL_VALUES];
    size_t num_values = 0;

    for (char *start = entry; num_values < MAX_WHEEL_VALUES && start && *start; ) {
        char *end = strchr(start, '|');

        if (end) {
            *end = '\0';
        }

        char *endptr;
        values[num_values++] = strtol(start, &endptr, 10);

        // sanity
        if (endptr == start) {
            return;
        }

        start = end ? (end + 1) : NULL;
    }

    // parse & sanity check
    if (index >= cl.csr.wheelammo + MAX_WHEEL_ITEMS) {
        if (num_values != 6) {
            return;
        }

        index = index - cl.csr.wheelpowerups;
        
        cl.wheel_data.powerups[index].item_index = values[0];
        cl.wheel_data.powerups[index].icons = CL_LoadWheelIcons(values[1]);
        cl.wheel_data.powerups[index].is_toggle = values[2];
        cl.wheel_data.powerups[index].sort_id = values[3];
        cl.wheel_data.powerups[index].can_drop = values[4];
        cl.wheel_data.powerups[index].ammo_index = values[5];
        cl.wheel_data.num_powerups = max(index + 1, cl.wheel_data.num_powerups);
    } else if (index >= cl.csr.wheelweapons + MAX_WHEEL_ITEMS) {
        if (num_values != 2) {
            return;
        }

        index = index - cl.csr.wheelammo;
        
        cl.wheel_data.ammo[index].item_index = values[0];
        cl.wheel_data.ammo[index].icons = CL_LoadWheelIcons(values[1]);
        cl.wheel_data.num_ammo = max(index + 1, cl.wheel_data.num_ammo);
    } else {
        if (num_values != 8) {
            return;
        }

        index = index - cl.csr.wheelweapons;
        
        cl.wheel_data.weapons[index].item_index = values[0];
        cl.wheel_data.weapons[index].icons = CL_LoadWheelIcons(values[1]);
        cl.wheel_data.weapons[index].ammo_index = values[2];
        cl.wheel_data.weapons[index].min_ammo = values[3];
        cl.wheel_data.weapons[index].is_powerup = values[4];
        cl.wheel_data.weapons[index].sort_id = values[5];
        cl.wheel_data.weapons[index].quantity_warn = values[6];
        cl.wheel_data.weapons[index].can_drop = values[7];
        cl.wheel_data.num_weapons = max(index + 1, cl.wheel_data.num_weapons);
    }
}

/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh(void)
{
    int         i;
    char        *name;

    if (!cls.ref_initialized)
        return;
    if (!cl.mapname[0])
        return;     // no map loaded

    // register models, pics, and skins
    R_BeginRegistration(cl.mapname);

    CL_LoadState(LOAD_MODELS);

    CL_RegisterTEntModels();

    for (i = 2; i < cl.csr.max_models; i++) {
        name = cl.configstrings[cl.csr.models + i];
        if (!name[0] && i != MODELINDEX_PLAYER) {
            break;
        }
        if (name[0] == '#') {
            continue;
        }
        cl.model_draw[i] = R_RegisterModel(name);
    }

    CL_LoadState(LOAD_IMAGES);
    for (i = 1; i < cl.csr.max_images; i++) {
        name = cl.configstrings[cl.csr.images + i];
        if (!name[0]) {
            break;
        }
        cl.image_precache[i] = CL_RegisterImage(name);
    }
    
    cgame->TouchPics();

    CL_Wheel_Precache();

    CL_LoadState(LOAD_CLIENTS);
    for (i = 0; i < MAX_CLIENTS; i++) {
        name = cl.configstrings[cl.csr.playerskins + i];
        if (!name[0]) {
            continue;
        }
        CL_LoadClientinfo(&cl.clientinfo[i], name);
    }

    CL_LoadClientinfo(&cl.baseclientinfo, "unnamed\\male/grunt\\default");

    // set sky textures and speed
    CL_SetSky();

    // load wheel data
    int n;
    for (n = cl.csr.wheelweapons, i = 0; i < MAX_WHEEL_ITEMS * 3; i++, n++) {
        if (*cl.configstrings[n]) {
            CL_LoadWheelEntry(n, cl.configstrings[n]);
        }
    }

    // the renderer can now free unneeded stuff
    R_EndRegistration();

    // clear any lines of console text
    Con_ClearNotify_f();

    SCR_UpdateScreen();

    // start the cd track
    OGG_Play();
}

// parse configstring, internal method
static void update_configstring(int index)
{
    const char *s = cl.configstrings[index];

    if (index == cl.csr.maxclients) {
        cl.maxclients = Q_atoi(s);
        return;
    }

    if (index == cl.csr.airaccel) {
        cl.pmp.airaccelerate = cl.pmp.qwmode || Q_atoi(s);
        return;
    }

    if (index == cl.csr.models + 1) {
        if (!Com_ParseMapName(cl.mapname, s, sizeof(cl.mapname)))
            Com_Error(ERR_DROP, "%s: bad world model: %s", __func__, s);
        return;
    }

    if (index >= cl.csr.lights && index < cl.csr.lights + MAX_LIGHTSTYLES) {
        CL_SetLightStyle(index - cl.csr.lights, s);
        return;
    }

    if (cls.state < ca_precached) {
        return;
    }

    if (index >= cl.csr.models + 2 && index < cl.csr.models + cl.csr.max_models) {
        int i = index - cl.csr.models;

        cl.model_draw[i] = R_RegisterModel(s);
        if (*s == '*')
            cl.model_clip[i] = BSP_InlineModel(cl.bsp, s);
        else
            cl.model_clip[i] = NULL;
        return;
    }

    if (index >= cl.csr.sounds && index < cl.csr.sounds + cl.csr.max_sounds) {
        cl.sound_precache[index - cl.csr.sounds] = S_RegisterSound(s);
        return;
    }

    if (index >= cl.csr.images && index < cl.csr.images + cl.csr.max_images) {
        cl.image_precache[index - cl.csr.images] = CL_RegisterImage(s);
        return;
    }

    if (index >= cl.csr.playerskins && index < cl.csr.playerskins + MAX_CLIENTS) {
        CL_LoadClientinfo(&cl.clientinfo[index - cl.csr.playerskins], s);
        return;
    }

    if (index == CS_CDTRACK) {
        OGG_Play();
        return;
    }

    if (index == CS_SKYROTATE || index == CS_SKYAXIS) {
        CL_SetSky();
        return;
    }

    if (index >= cl.csr.wheelweapons && index <= cl.csr.wheelpowerups + MAX_WHEEL_ITEMS) {
        CL_LoadWheelEntry(index, s);
        return;
    }
}

/*
=================
CL_UpdateConfigstring

A configstring update has been parsed.
=================
*/
void CL_UpdateConfigstring(int index)
{
    update_configstring(index);

    cgame->ParseConfigString(index, cl.configstrings[index]);
}