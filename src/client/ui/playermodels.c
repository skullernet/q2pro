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

#include "ui.h"
#include "common/files.h"

/*
=============================================================================

PLAYER MODELS

=============================================================================
*/

static bool IconOfSkinExists(char *skin, char **pcxfiles, int npcxfiles)
{
    int i;
    char scratch[MAX_QPATH];

    COM_StripExtension(scratch, skin, sizeof(scratch));
    Q_strlcat(scratch, "_i.pcx", sizeof(scratch));

    for (i = 0; i < npcxfiles; i++) {
        if (Q_stricmp(pcxfiles[i], scratch) == 0)
            return true;
    }

    return false;
}

static int pmicmpfnc(const void *_a, const void *_b)
{
    const playerModelInfo_t *a = (const playerModelInfo_t *)_a;
    const playerModelInfo_t *b = (const playerModelInfo_t *)_b;

    /*
    ** sort by male, female, then alphabetical
    */
    if (Q_stricmp(a->directory, "male") == 0)
        return -1;
    if (Q_stricmp(b->directory, "male") == 0)
        return 1;

    if (Q_stricmp(a->directory, "female") == 0)
        return -1;
    if (Q_stricmp(b->directory, "female") == 0)
        return 1;

    return Q_stricmp(a->directory, b->directory);
}

void PlayerModel_Load(void)
{
    char scratch[MAX_QPATH];
    int i, ndirs;
    char **dirnames;
    playerModelInfo_t *pmi;

    Q_assert(!uis.numPlayerModels);

    // get a list of directories
    if (!(dirnames = (char **)FS_ListFiles("players", NULL, FS_SEARCH_DIRSONLY, &ndirs))) {
        return;
    }

    // go through the subdirectories
    for (i = 0; i < ndirs; i++) {
        int k, s;
        char **pcxnames;
        char **skinnames;
        int npcxfiles;
        int nskins = 0;

        // verify the existence of tris.md2
        Q_concat(scratch, sizeof(scratch), "players/", dirnames[i], "/tris.md2");
        if (!FS_FileExists(scratch)) {
            continue;
        }

        // verify the existence of at least one pcx skin
        Q_concat(scratch, sizeof(scratch), "players/", dirnames[i]);
        pcxnames = (char **)FS_ListFiles(scratch, ".pcx", 0, &npcxfiles);
        if (!pcxnames) {
            continue;
        }

        // count valid skins, which consist of a skin with a matching "_i" icon
        for (k = 0; k < npcxfiles; k++) {
            if (!strstr(pcxnames[k], "_i.pcx")) {
                if (IconOfSkinExists(pcxnames[k], pcxnames, npcxfiles)) {
                    nskins++;
                }
            }
        }

        if (!nskins) {
            FS_FreeList((void **)pcxnames);
            continue;
        }

        skinnames = UI_Malloc(sizeof(char *) * (nskins + 1));
        skinnames[nskins] = NULL;

        // copy the valid skins
        for (s = 0, k = 0; k < npcxfiles; k++) {
            if (!strstr(pcxnames[k], "_i.pcx")) {
                if (IconOfSkinExists(pcxnames[k], pcxnames, npcxfiles)) {
                    COM_StripExtension(scratch, pcxnames[k], sizeof(scratch));
                    skinnames[s++] = UI_CopyString(scratch);
                }
            }
        }

        FS_FreeList((void **)pcxnames);

        // at this point we have a valid player model
        pmi = &uis.pmi[uis.numPlayerModels++];
        pmi->nskins = nskins;
        pmi->skindisplaynames = skinnames;
        pmi->directory = UI_CopyString(dirnames[i]);

        if (uis.numPlayerModels == MAX_PLAYERMODELS)
            break;
    }

    FS_FreeList((void **)dirnames);

    qsort(uis.pmi, uis.numPlayerModels, sizeof(uis.pmi[0]), pmicmpfnc);
}

void PlayerModel_Free(void)
{
    playerModelInfo_t *pmi;
    int i, j;

    for (i = 0, pmi = uis.pmi; i < uis.numPlayerModels; i++, pmi++) {
        if (pmi->skindisplaynames) {
            for (j = 0; j < pmi->nskins; j++) {
                Z_Free(pmi->skindisplaynames[j]);
            }
            Z_Free(pmi->skindisplaynames);
        }
        Z_Free(pmi->directory);
        memset(pmi, 0, sizeof(*pmi));
    }

    uis.numPlayerModels = 0;
}
