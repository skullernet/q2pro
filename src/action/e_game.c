// AQ2: ETE migration to TNG //
// One library to rule them all, and in the darkness, frag them //

#include "g_local.h"

qboolean EspSceneLoadConfig(char *mapname)
{
	char buf[1024];
	char *ptr;
    char def_scnfile[1024];
	FILE *fh;

	memset(&ctfgame, 0, sizeof(ctfgame));

	gi.dprintf("Trying to load Espionage configuration file\n", mapname);

	/* zero is perfectly acceptable respawn time, but we want to know if it came from the config or not */
	ctfgame.spawn_red = -1;
	ctfgame.spawn_blue = -1;

	sprintf (buf, "%s/tng/%s.scn", GAMEVERSION, mapname);
	fh = fopen (buf, "r");
	if (!fh) {
		gi.dprintf ("Warning: Espionage configuration file %s was not found.\n", buf);
		return false;
	}
    else if 

	gi.dprintf("-------------------------------------\n");
	gi.dprintf("CTF configuration loaded from %s\n", buf);
	ptr = INI_Find(fh, "ctf", "author");
	if(ptr) {
		gi.dprintf(" Author    : %s\n", ptr);
		Q_strncpyz(ctfgame.author, ptr, sizeof(ctfgame.author));
	}
	ptr = INI_Find(fh, "ctf", "comment");
	if(ptr) {
		gi.dprintf(" Comment   : %s\n", ptr);
		Q_strncpyz(ctfgame.comment, ptr, sizeof(ctfgame.comment));
	}

	ptr = INI_Find(fh, "ctf", "type");
	if(ptr) {
		gi.dprintf(" Game type : %s\n", ptr);
		if(strcmp(ptr, "balanced") == 0)
			ctfgame.type = 1;
		if(strcmp(ptr, "offdef") == 0)
			ctfgame.type = 2;
	}
	ptr = INI_Find(fh, "ctf", "offence");
	if(ptr) {
		gi.dprintf(" Offence   : %s\n", ptr);
		ctfgame.offence = TEAM1;
		if(strcmp(ptr, "blue") == 0)
			ctfgame.offence = TEAM2;
	}
	ptr = INI_Find(fh, "ctf", "grapple");
	gi.cvar_forceset("use_grapple", "0");
	if(ptr) {
		gi.dprintf(" Grapple   : %s\n", ptr);
		if(strcmp(ptr, "1") == 0)
			gi.cvar_forceset("use_grapple", "1");
		else if(strcmp(ptr, "2") == 0)
			gi.cvar_forceset("use_grapple", "2");
	}

	gi.dprintf(" Spawn times\n");
	ptr = INI_Find(fh, "respawn", "red");
	if(ptr) {
		gi.dprintf("  Red      : %s\n", ptr);
		ctfgame.spawn_red = atoi(ptr);
	}
	ptr = INI_Find(fh, "respawn", "blue");
	if(ptr) {
		gi.dprintf("  Blue     : %s\n", ptr);
		ctfgame.spawn_blue = atoi(ptr);
	}

	gi.dprintf(" Flags\n");
	ptr = INI_Find(fh, "flags", "red");
	if(ptr) {
		gi.dprintf("  Red      : %s\n", ptr);
		CTFSetFlag(TEAM1, ptr);
	}
	ptr = INI_Find(fh, "flags", "blue");
	if(ptr) {
		gi.dprintf("  Blue     : %s\n", ptr);
		CTFSetFlag(TEAM2, ptr);
	}

	gi.dprintf(" Spawns\n");
	ptr = INI_Find(fh, "spawns", "red");
	if(ptr) {
		gi.dprintf("  Red      : %s\n", ptr);
		CTFSetTeamSpawns(TEAM1, ptr);
		ctfgame.custom_spawns = true;
	}
	ptr = INI_Find(fh, "spawns", "blue");
	if(ptr) {
		gi.dprintf("  Blue     : %s\n", ptr);
		CTFSetTeamSpawns(TEAM2, ptr);
		ctfgame.custom_spawns = true;
	}

	// automagically change spawns *only* when we do not have team spawns
	if(!ctfgame.custom_spawns)
		ChangePlayerSpawns();

	gi.dprintf("-------------------------------------\n");

	fclose(fh);

	return true;
}


void G_LoadScenes( void )
{
	//Based on AQ2:TNG New Location Code G_LoadLocations
	char	scnfile[MAX_QPATH], buffer[256];
	FILE	*f, *d;
	int		i, x, y, z, rx, ry, rz;
	char	*locationstr, *param, *line;
	cvar_t	*game_cvar;
	placedata_t *loc;
    char    def_scnfile[MAX_QPATH], buffer[256];
    char    def_scnfilename;

	memset( scn_creator, 0, sizeof( scn_creator ) );
	ml_count = 0;

    def_scnfilename = "atl.scn";

	game_cvar = gi.cvar ("game", "action", 0);

	if (!*game_cvar->string)
		Com_sprintf(scnfile, sizeof(scnfile), "%s/tng/%s.scn", GAMEVERSION, level.mapname);
	else
		Com_sprintf(scnfile, sizeof(scnfile), "%s/tng/%s.scn", game_cvar->string, level.mapname);

	f = fopen( scnfile, "r" );
	if (!f) {
		gi.dprintf( "No scene file for %s\n", level.mapname );
        gi.dprintf( "Attempting to load default scene file %s\n", def_scnfile );
        Com_sprintf(def_scnfile, sizeof(scnfile), "%s/tng/%s.scn", game_cvar->string, def_scnfilename);
        d = fopen( def_scnfile, "r");

        gi.dprintf( "Scene file: %s\n", def_scnfilename );
        if (!d){
            gi.dprintf( "No scene files found, aborting\n" );
            return;
        }
	}
    else {
	    gi.dprintf( "Scene file: %s\n", scnfile );
    }

	do
	{
		line = fgets( buffer, sizeof( buffer ), f );
		if (!line) {
			break;
		}

		if (strlen( line ) < 12)
			continue;

		if (line[0] == '#')
		{
			param = line + 1;
			while (*param == ' ') { param++; }
			if (*param && !Q_strnicmp(param, "creator", 7))
			{
				param += 8;
				while (*param == ' ') { param++; }
				for (i = 0; *param >= ' ' && i < sizeof( scn_creator ) - 1; i++) {
					scn_creator[i] = *param++;
				}
				scn_creator[i] = 0;
				while (i > 0 && scn_creator[i - 1] == ' ') //Remove trailing spaces
					scn_creator[--i] = 0;
			}
			continue;
		}

		param = strtok( line, " :\r\n\0" );
		// TODO: better support for file comments
		if (!param || param[0] == '#')
			continue;

		x = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		y = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		z = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		rx = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		ry = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		rz = atoi( param );

		param = strtok( NULL, "\r\n\0" );
		if (!param)
			continue;
		locationstr = param;

		loc = &locationbase[ml_count++];
		loc->x = x;
		loc->y = y;
		loc->z = z;
		loc->rx = rx;
		loc->ry = ry;
		loc->rz = rz;
		Q_strncpyz( loc->desc, locationstr, sizeof( loc->desc ) );

		if (ml_count >= MAX_LOCATIONS_IN_BASE) {
			gi.dprintf( "Cannot read more than %d locations.\n", MAX_LOCATIONS_IN_BASE );
			break;
		}
	} while (1);

	fclose( f );
	gi.dprintf( "Found %d locations.\n", ml_count );
}