//-----------------------------------------------------------------------------
// Statistics Related Code
//
// $Id: tng_stats.c,v 1.33 2004/05/18 20:35:45 slicerdw Exp $
//
//-----------------------------------------------------------------------------
// $Log: tng_stats.c,v $
// Revision 1.33  2004/05/18 20:35:45  slicerdw
// Fixed a bug on stats command
//
// Revision 1.32  2002/04/03 15:05:03  freud
// My indenting broke something, rolled the source back.
//
// Revision 1.30  2002/04/01 16:08:59  freud
// Fix in hits/shots counter for each weapon
//
// Revision 1.29  2002/04/01 15:30:38  freud
// Small stat fix
//
// Revision 1.28  2002/04/01 15:16:06  freud
// Stats code redone, tng_stats now much more smarter. Removed a few global
// variables regarding stats code and added kevlar hits to stats.
//
// Revision 1.27  2002/03/28 12:10:12  freud
// Removed unused variables (compiler warnings).
// Added cvar mm_allowlock.
//
// Revision 1.26  2002/03/15 19:28:36  deathwatch
// Updated with stats rifle name fix
//
// Revision 1.25  2002/02/26 23:09:20  freud
// Stats <playerid> not working, fixed.
//
// Revision 1.24  2002/02/21 23:38:39  freud
// Fix to a BAD stats bug. CRASH
//
// Revision 1.23  2002/02/18 23:47:33  freud
// Fixed FPM if time was 0
//
// Revision 1.22  2002/02/18 19:31:40  freud
// FPM fix.
//
// Revision 1.21  2002/02/18 17:21:14  freud
// Changed Knife in stats to Slashing Knife
//
// Revision 1.20  2002/02/17 21:48:56  freud
// Changed/Fixed allignment of Scoreboard
//
// Revision 1.19  2002/02/05 09:27:17  freud
// Weapon name changes and better alignment in "stats list"
//
// Revision 1.18  2002/02/03 01:07:28  freud
// more fixes with stats
//
// Revision 1.14  2002/01/24 11:29:34  ra
// Cleanup's in stats code
//
// Revision 1.13  2002/01/24 02:24:56  deathwatch
// Major update to Stats code (thanks to Freud)
// new cvars:
// stats_afterround - will display the stats after a round ends or map ends
// stats_endmap - if on (1) will display the stats scoreboard when the map ends
//
// Revision 1.12  2001/12/31 13:29:06  deathwatch
// Added revision header
//
//
//-----------------------------------------------------------------------------

#include "g_local.h"
#include <time.h>

/* Stats Command */

void ResetStats(edict_t *ent)
{
	int i;

	if(!ent->client)
		return;

	ent->client->resp.shotsTotal = 0;
	ent->client->resp.hitsTotal = 0;

	for (i = 0; i<LOC_MAX; i++)
		ent->client->resp.hitsLocations[i] = 0;

	memset(ent->client->resp.gunstats, 0, sizeof(ent->client->resp.gunstats));
}

void Stats_AddShot( edict_t *ent, int gun )
{
	if( in_warmup )
		return;

	if ((unsigned)gun >= MAX_GUNSTAT) {
		gi.dprintf( "Stats_AddShot: Bad gun number!\n" );
		return;
	}

	if (!teamplay->value || team_round_going || stats_afterround->value) {
		ent->client->resp.shotsTotal += 1;	// TNG Stats, +1 hit
		ent->client->resp.gunstats[gun].shots += 1;	// TNG Stats, +1 hit
	}
}

void Stats_AddHit( edict_t *ent, int gun, int hitPart )
{
	int headShot = (hitPart == LOC_HDAM || hitPart == LOC_KVLR_HELMET) ? 1 : 0;

	ent->client->last_damaged_part = hitPart;

	if( in_warmup )
		return;

	if ((unsigned)gun >= MAX_GUNSTAT) {
		gi.dprintf( "Stats_AddHit: Bad gun number!\n" );
		return;
	}

	if (!teamplay->value || team_round_going || stats_afterround->value) {
		ent->client->resp.hitsTotal++;
		ent->client->resp.gunstats[gun].hits++;
		ent->client->resp.hitsLocations[hitPart]++;

		if (headShot)
			ent->client->resp.gunstats[gun].headshots++;
	}
	if (!headShot) {
		ent->client->resp.streakHS = 0;
	}
}


void Cmd_Stats_f (edict_t *targetent, char *arg)
{
/* Variables Used:                              *
* stats_shots_t - Total nr of Shots             *
* stats_shots_h - Total nr of Hits              *
* headshots     - Total nr of Headshots         *
*                                               */
	
	double	perc_hit;
	int		total, hits, i, y, len, locHits;
	char		*string, stathead[64];
	edict_t		*ent, *cl_ent;
	gunStats_t	*gun;

	if (!targetent->inuse)
		return;


	if (arg[0] != '\0') {
		if (strcmp (arg, "list") == 0) {
			gi.cprintf (targetent, PRINT_HIGH, "\n\x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F\n");
			gi.cprintf (targetent, PRINT_HIGH, "PlayerID  Name                  Accuracy\n");

			for (i = 0; i < game.maxclients; i++)
			{
				cl_ent = &g_edicts[1 + i];

				if (!cl_ent->inuse || cl_ent->client->pers.mvdspec)
					continue;

				hits = total = 0;
				gun = cl_ent->client->resp.gunstats;
				for (y = 0; y < MAX_GUNSTAT; y++, gun++) {
					hits += gun->hits;
					total += gun->shots;
				}

				if (total > 0)
					perc_hit = (double)hits * 100.0 / (double)total;
				else
					perc_hit = 0.0;

				gi.cprintf (targetent, PRINT_HIGH, "   %-3i    %-16s        %6.2f\n", i, cl_ent->client->pers.netname, perc_hit);
			}
			gi.cprintf (targetent, PRINT_HIGH, "\n  Use \"stats <PlayerID>\" for\n  individual stats\n\x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F\n");
			return;
		}

		ent = LookupPlayer(targetent, gi.args(), true, true);
		if (!ent)
			return;

	} else {
		ent = targetent;
	}

	// Global Stats:
	hits = total = 0;
	gun = ent->client->resp.gunstats;
	for (y = 0; y < MAX_GUNSTAT; y++, gun++) {
		hits += gun->hits;
		total += gun->shots;
	}

	sprintf(stathead, "\n\x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F Statistics for %s \x9D", ent->client->pers.netname);
	len = strlen(stathead);
	for (i = len; i < 55; i++) {
		stathead[i] = '\x9E';
	}
	stathead[i] = 0;
	strcat(stathead, "\x9F\n");

	gi.cprintf (targetent, PRINT_HIGH, "%s", stathead);

	if (!total) {
		gi.cprintf (targetent, PRINT_HIGH, "\n  Player has not fired a shot.\n");
		gi.cprintf (targetent, PRINT_HIGH, "\n\x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F\n\n");
		return;
	}

	gi.cprintf (targetent, PRINT_HIGH, "Weapon            Accuracy Hits/Shots Kills Headshots\n");		

	gun = ent->client->resp.gunstats;
	for (y = 0; y < MAX_GUNSTAT; y++, gun++) {

		if (gun->shots <= 0)
			continue;

		switch (y) {
		case MOD_MK23:
			string = "Pistol";
			break;
		case MOD_DUAL:
			string = "Dual Pistols";
			break;
		case MOD_KNIFE:
			string = "Slashing Knife";
			break;
		case MOD_KNIFE_THROWN:
			string = "Throwing Knife";
			break;
		case MOD_M4:
			string = "M4 Assault Rifle";
			break;
		case MOD_MP5:
			string = "MP5 Submachinegun";
			break;
		case MOD_SNIPER:
			string = "Sniper Rifle";
			break;
		case MOD_HC:
			string = "Handcannon";
			break;
		case MOD_M3:
			string = "M3 Shotgun";
			break;
		default:
			string = "Unknown Weapon";
			break;
		}

		perc_hit = (double)gun->hits * 100.0 / (double)gun->shots; // Percentage of shots that hit
		gi.cprintf( targetent, PRINT_HIGH, "%-17s  %6.2f  %4i/%-4i  %3i   %5i\n",
			string, perc_hit, gun->hits, gun->shots, gun->kills, gun->headshots );
	}

	gi.cprintf (targetent, PRINT_HIGH, "\n\x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F\n");


	// Final Part
	gi.cprintf (targetent, PRINT_HIGH, "Location                          Hits     (%%)\n");		

	for (y = 0; y < LOC_MAX; y++) {
		locHits = ent->client->resp.hitsLocations[y];

		if (locHits <= 0)
			continue;

		switch (y) {
		case LOC_HDAM:
			string = "Head";
			break;
		case LOC_CDAM:
			string = "Chest";
			break;
		case LOC_SDAM:
			string = "Stomach";
			break;
		case LOC_LDAM:
			string = "Legs";
			break;
		case LOC_KVLR_HELMET:
			string = "Kevlar Helmet";
			break;
		case LOC_KVLR_VEST:
			string = "Kevlar Vest";
			break;
		case LOC_NO:
			string = "Spread (Shotgun/Handcannon)";
			break;
		default:
			string = "Unknown";
			break;
		}

		perc_hit = (double)locHits * 100.0 / (double)hits;
		gi.cprintf( targetent, PRINT_HIGH, "%-27s %10i  (%6.2f)\n", string, locHits, perc_hit );
	}
	gi.cprintf (targetent, PRINT_HIGH, "\n");

	if (total > 0)
		perc_hit = (double)hits * 100.0 / (double)total;
	else
		perc_hit = 0.0;

	gi.cprintf (targetent, PRINT_HIGH, "Average Accuracy:                         %.2f\n", perc_hit); // Average
	gi.cprintf (targetent, PRINT_HIGH, "\n\x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F\n\n");
	gi.cprintf(targetent, PRINT_HIGH, "Highest streaks:  kills: %d headshots: %d\n", ent->client->resp.streakKillsHighest, ent->client->resp.streakHSHighest);
}

void A_ScoreboardEndLevel (edict_t * ent, edict_t * killer)
{
	char string[2048];
	gclient_t *sortedClients[MAX_CLIENTS], *cl;
	int maxsize = 1000, i, line_y;
	int totalClients, secs, shots;
	double accuracy, fpm;
	int totalplayers[TEAM_TOP] = {0};
	int totalscore[TEAM_TOP] = {0};
	int name_pos[TEAM_TOP] = {0};

	totalClients = G_SortedClients(sortedClients);

	ent->client->ps.stats[STAT_TEAM_HEADER] = level.pic_teamtag;

	for (i = 0; i < totalClients; i++) {
		cl = sortedClients[i];

		totalscore[cl->resp.team] += cl->resp.score;
		totalplayers[cl->resp.team]++;
	}

	for (i = TEAM1; i <= teamCount; i++) {
		name_pos[i] = ((20 - strlen(teams[i].name)) / 2) * 8;
		if (name_pos[TEAM1] < 0)
			name_pos[TEAM1] = 0;
	}


	if (teamCount == 3)
	{
		sprintf(string,
			// TEAM1
			"if 24 xv -80 yv 8 pic 24 endif "
			"if 22 xv -48 yv 8 pic 22 endif "
			"xv -48 yv 28 string \"%4d/%-3d\" "
			"xv 10 yv 12 num 2 26 "
			"xv %d yv 0 string \"%s\" ",
			totalscore[TEAM1], totalplayers[TEAM1], name_pos[TEAM1] - 80,
			teams[TEAM1].name);
		sprintf(string + strlen (string),
			// TEAM2
			"if 25 xv 80 yv 8 pic 25 endif "
			"if 22 xv 112 yv 8 pic 22 endif "
			"xv 112 yv 28 string \"%4d/%-3d\" "
			"xv 168 yv 12 num 2 27 "
			"xv %d yv 0 string \"%s\" ",
			totalscore[TEAM2], totalplayers[TEAM2], name_pos[TEAM2] + 80,
			teams[TEAM2].name);
		sprintf(string + strlen (string),
			// TEAM3
			"if 30 xv 240 yv 8 pic 30 endif "
			"if 22 xv 272 yv 8 pic 22 endif "
			"xv 272 yv 28 string \"%4d/%-3d\" "
			"xv 328 yv 12 num 2 31 "
			"xv %d yv 0 string \"%s\" ",
			totalscore[TEAM3], totalplayers[TEAM3], name_pos[TEAM3] + 240,
			teams[TEAM3].name);
	}
	else
	{
		sprintf (string,
			// TEAM1
			"if 24 xv 0 yv 8 pic 24 endif "
			"if 22 xv 32 yv 8 pic 22 endif "
			"xv 32 yv 28 string \"%4d/%-3d\" "
			"xv 90 yv 12 num 2 26 " "xv %d yv 0 string \"%s\" "
			// TEAM2
			"if 25 xv 160 yv 8 pic 25 endif "
			"if 22 xv 192 yv 8 pic 22 endif "
			"xv 192 yv 28 string \"%4d/%-3d\" "
			"xv 248 yv 12 num 2 27 "
			"xv %d yv 0 string \"%s\" ",
			totalscore[TEAM1], totalplayers[TEAM1], name_pos[TEAM1],
			teams[TEAM1].name, totalscore[TEAM2], totalplayers[TEAM2],
			name_pos[TEAM2] + 160, teams[TEAM2].name);
	}

	line_y = 56;
	sprintf(string + strlen (string),
		"xv 0 yv 40 string2 \"Frags Player          Shots   Acc   FPM \" "
		"xv 0 yv 48 string2 \"\x9D\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9F\" ");

//        strcpy (string, "xv 0 yv 32 string2 \"Frags Player          Time Ping Damage Kills\" "
//                "xv 0 yv 40 string2 \"\x9D\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9F \x9D\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9E\x9F \x9D\x9E\x9E\x9E\x9F\" ");
  /*
     {
     strcpy (string, "xv 0 yv 32 string2 \"Player          Time Ping\" "
     "xv 0 yv 40 string2 \"--------------- ---- ----\" ");
     }
     else
     {
     strcpy (string, "xv 0 yv 32 string2 \"Frags Player          Time Ping Damage Kills\" "
     "xv 0 yv 40 string2 \"----- --------------- ---- ---- ------ -----\" ");
     }
   */
  // AQ2:TNG END

	for (i = 0; i < totalClients; i++)
	{
		cl = sortedClients[i];

		if (!cl->resp.team)
			continue;

		shots = min( cl->resp.shotsTotal, 9999 );

		if (shots)
			accuracy = (double)cl->resp.hitsTotal * 100.0 / (double)cl->resp.shotsTotal;
		else
			accuracy = 0;

		secs = (level.framenum - cl->resp.enterframe) / HZ;
		if (secs > 0)
			fpm = (double)cl->resp.score * 60.0 / (double)secs;
		else
			fpm = 0.0;

		sprintf(string + strlen(string),
			"yv %d string \"%5d %-15s  %4d %5.1f  %4.1f\" ",
			line_y,
			cl->resp.score,
			cl->pers.netname, shots, accuracy, fpm );
		
		line_y += 8;

		if (strlen (string) > (maxsize - 100) && i < (totalClients - 2))
		{
			sprintf(string + strlen (string),
				"yv %d string \"..and %d more\" ",
				line_y, (totalClients - i - 1) );
			line_y += 8;
			break;
		}
	}
	
	if (strlen(string) > 1023)	// for debugging...
	{
		gi.dprintf("Warning: scoreboard string neared or exceeded max length\nDump:\n%s\n---\n", string);
		string[1023] = '\0';
	}

	gi.WriteByte(svc_layout);
	gi.WriteString(string);
}

void Cmd_Statmode_f(edict_t* ent)
{
	int i;
	char stuff[32], *arg;


	// Ignore if there is no argument.
	arg = gi.argv(1);
	if (!arg || !arg[0])
		return;

	// Numerical
	i = atoi (arg);

	if (i > 2 || i < 0) {
		gi.dprintf("Warning: stat_mode set to %i by %s\n", i, ent->client->pers.netname);

		// Force the old mode if it is valid else force 0
		if (ent->client->resp.stat_mode > 0 && ent->client->resp.stat_mode < 3)
			sprintf(stuff, "set stat_mode \"%i\"\n", ent->client->resp.stat_mode);
		else
			sprintf(stuff, "set stat_mode \"0\"\n");
	} else {
		sprintf(stuff, "set stat_mode \"%i\"\n", i);
		ent->client->resp.stat_mode = i;
	}
	stuffcmd(ent, stuff);
}

#if USE_AQTION

// Revisit one day...

// #include <curl/curl.h>
// // AQtion stats addon
// // Utilizes AWS API Gateway and AWS SQS
// // Review documentation to understand their use

// size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
// {
//    return size * nmemb;
// }
// void StatSend(const char *payload, ...)
// {	
// 	va_list argptr;
// 	char text[1024];
// 	char apikeyheader[64] = "x-api-key: ";
// 	char apiurl[128] = "\0";
// 	int apikey_check;

// 	// If stat logs are disabled or stat-apikey is default, just return
// 	apikey_check = Q_stricmp(stat_apikey->string, "none");
// 	if (!stat_logs->value || apikey_check == 0) {
// 		return;
// 	}
	
// 	Q_strncatz(apikeyheader, stat_apikey->string, sizeof(apikeyheader));
// 	Q_strncpyz(apiurl, stat_url->string, sizeof(apiurl));
	
// 	va_start (argptr, payload);
// 	vsnprintf (text, sizeof(text), payload, argptr);
// 	va_end (argptr);

// 	CURL *curl = curl_easy_init();
// 	struct curl_slist *headers = NULL;
// 	headers = curl_slist_append(headers, "Accept: application/json");
// 	headers = curl_slist_append(headers, "Content-Type: application/json");
// 	headers = curl_slist_append(headers, apikeyheader);

// 	curl_easy_setopt(curl, CURLOPT_URL, apiurl);
// 	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
// 	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
// 	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, text);

// 	// Do not print responses from curl request
// 	// Comment below if you are debugging responses
// 	// Hint: Forbidden would mean your stat_url is malformed,
// 	// and a key error indicates your api key is bad or expired
// 	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

// 	// Run it!
// 	curl_easy_perform(curl);
// 	curl_easy_cleanup(curl);
// 	curl_global_cleanup();
// }

int Gamemode(void) // These are distinct game modes; you cannot have a teamdm tourney mode, for example
{
	int gamemode = 0;
	if (teamdm->value) {
		gamemode = GM_TEAMDM;
	} else if (ctf->value) {
		gamemode = GM_CTF;
	} else if (use_tourney->value) {
		gamemode = GM_TOURNEY;
	} else if (teamplay->value) {
		gamemode = GM_TEAMPLAY;  
	} else if (dom->value) {
		gamemode = GM_DOMINATION;
	} else if (deathmatch->value) {
		gamemode = GM_DEATHMATCH;
	}
	return gamemode;
}

int Gamemodeflag(void) 
// These are gamemode flags that change the rules of gamemodes.
// For example, you can have a darkmatch matchmode 3team teamplay server
{
	int gamemodeflag = 0;
	char gmfstr[16];

	if (use_3teams->value) {
		gamemodeflag += GMF_3TEAMS;
	}
	if (darkmatch->value) {
		gamemodeflag += GMF_DARKMATCH;
	}
	if (matchmode->value) {
		gamemodeflag += GMF_MATCHMODE;
	}
	sprintf(gmfstr, "%d", gamemodeflag);
	gi.cvar_forceset("gmf", gmfstr);
	return gamemodeflag;
}

#ifndef NO_BOTS
/*
=================
Bot Check
=================
*/
void StatBotCheck(void)
{
    for (int i = 0; i < num_players; i++)
    {
        if (players[i]->is_bot)
        {
            game.ai_ent_found = true;
            if (stat_logs->value) {
                gi.dprintf("Bot detected, forcing stat_logs off\n");
                gi.cvar_forceset(stat_logs->name, "0");    // Turn off stat collection
            }
            return;
        }
    }
    game.ai_ent_found = false;
}
#endif

cvar_t* logfile_name;
void Write_Stats(const char* msg, ...)
{
	va_list	argptr;
	char	stat_string[1024];
	char	stat_cpy[1024];
	char	logpath[MAX_QPATH];
	FILE* 	f;

	va_start(argptr, msg);
	vsprintf(stat_cpy, msg, argptr);
	va_end(argptr);

	logfile_name = gi.cvar("logfile_name", "", CVAR_NOSET);
	sprintf(logpath, "action/logs/%s.stats", logfile_name->string);

	if ((f = fopen(logpath, "a")) != NULL)
	{
		fprintf(f, "%s", stat_cpy);
		fclose(f);
	}
	else
		gi.dprintf("Error writing to %s.stats\n", logfile_name->string);

}

/*
==================
LogKill
=================
*/
void LogKill(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	int mod;
	int loc;
	int gametime = 0;
	int roundNum;
	int eventtime;
	int vt = 0; //Default victim team is 0 (no team)
	int kt = 0; //Default killer team is 0 (no team)
	int ttk = 0; //Default TTK (time to kill) is 0
	int vl = 0; //Placeholder victimleader until Espionage gets ported
	int kl = 0; //Placeholder killerleader until Espionage gets ported
	char msg[1024]; // Whole stat line in JSON format
	char v[24]; // Victim's Steam ID
	char vn[128]; // Victim's name
	char vip[24]; // Victim's IP:port
	char vd[24]; // Victim's Discord ID
	char *vi; // Victim's IP (without port)
	char k[24]; // Killer's Steam ID
	char kn[128]; // Killer's name
	char kip[24]; // Killer's IP:port
	char kd[24]; // Killer's Discord ID
	char *ki; // Killer's IP (without port)

	// Check if there's an AI bot in the game, if so, do nothing
	if (game.ai_ent_found) {
		return;
	}

	// Only record stats if there's more than one opponent
    if (gameSettings & GS_DEATHMATCH) // Only check if in DM
    {
        int oc = 0; // Opponent count
        for (int i = 0; i < game.maxclients; i++)
        {
            // If player is connected and not spectating, add them as an opponent
            if (game.clients[i].pers.connected && game.clients[i].pers.spectator == false)
            {
                if (++oc > 1) // Two or more opponents are active, so log kills
                    break;
            }
        }
        if (oc == 1) // Only one opponent active, so don't log kills
            return;
    }

	if ((team_round_going && !in_warmup) || (gameSettings & GS_DEATHMATCH)) // If round is active OR if deathmatch
	{
		mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
		loc = locOfDeath;

		if (gameSettings & GS_TEAMPLAY) // Define these if the game is teamplay
		{
			vt = self->client->resp.team;
			kt = attacker->client->resp.team;
			ttk = current_round_length / 10;
		}
		
		Q_strncpyz(v, Info_ValueForKey(self->client->pers.userinfo, "steamid"), sizeof(v));
		Q_strncpyz(vn, Info_ValueForKey(self->client->pers.userinfo, "name"), sizeof(vn));
		Q_strncpyz(vip, Info_ValueForKey(self->client->pers.userinfo, "ip"), sizeof(vip));
		Q_strncpyz(vd, Info_ValueForKey(self->client->pers.userinfo, "cl_discord_id"), sizeof(vd));
		Q_strncpyz(k, Info_ValueForKey(attacker->client->pers.userinfo, "steamid"), sizeof(k));
		Q_strncpyz(kn, Info_ValueForKey(attacker->client->pers.userinfo, "name"), sizeof(kn));
		Q_strncpyz(kip, Info_ValueForKey(attacker->client->pers.userinfo, "ip"), sizeof(kip));
		Q_strncpyz(kd, Info_ValueForKey(attacker->client->pers.userinfo, "cl_discord_id"), sizeof(kd));

		// Separate IP from Port
		char* portSeperator = ":";
		vi = strtok(vip, portSeperator);
		ki = strtok(kip, portSeperator);

		gametime = level.matchTime;
		eventtime = (int)time(NULL);
		roundNum = game.roundNum + 1;

		Com_sprintf(
			msg, sizeof(msg),
			"{\"frag\":{\"sid\":\"%s\",\"mid\":\"%s\",\"v\":\"%s\",\"vn\":\"%s\",\"vi\":\"%s\",\"vt\":%i,\"vl\":%i,\"k\":\"%s\",\"kn\":\"%s\",\"ki\":\"%s\",\"kt\":%i,\"kl\":%i,\"w\":%i,\"i\":%i,\"l\":%i,\"ks\":%i,\"gm\":%i,\"gmf\":%i,\"ttk\":%d,\"t\":%d,\"gt\":%d,\"m\":\"%s\",\"r\":%i,\"vd\":\"%s\",\"kd\":\"%s\"}}\n",
			server_id->string,
			game.matchid,
			v,
			vn,
			vi,
			vt,
			vl,
			k,
			kn,
			ki,
			kt,
			kl,
			mod,
			attacker->client->pers.chosenItem->typeNum,
			loc,
			attacker->client->resp.streakKills + 1,
			Gamemode(),
			Gamemodeflag(),
			ttk,
			eventtime,
			gametime,
			level.mapname,
			roundNum,
			vd,
			kd
		);
		Write_Stats(msg);
	}
}

/*
==================
LogWorldKill
=================
*/
void LogWorldKill(edict_t *self)
{
	int mod;
	int loc = 16;
	int gametime = 0;
	int roundNum;
	int eventtime;
	int vt = 0; //Default victim team is 0 (no team)
	int ttk = 0; //Default TTK (time to kill) is 0
	int vl = 0; //Placeholder victimleader until Espionage gets ported
	char msg[1024];
	char v[24];
	char vn[128];
	char vip[24];
	char vd[24];
	char *vi;

	// Check if there's an AI bot in the game, if so, do nothing
	if (game.ai_ent_found) {
		return;
	}

	// Only record stats if there's more than one opponent
    if (gameSettings & GS_DEATHMATCH) // Only check if in DM
    {
        int oc = 0; // Opponent count
        for (int i = 0; i < game.maxclients; i++)
        {
            // If player is connected and not spectating, add them as an opponent
            if (game.clients[i].pers.connected && game.clients[i].pers.spectator == false)
            {
                if (++oc > 1) // Two or more opponents are active, so log kills
                    break;
            }
        }
        if (oc == 1) // Only one opponent active, so don't log kills
            return;
    }

	if ((team_round_going && !in_warmup) || (gameSettings & GS_DEATHMATCH)) // If round is active OR if deathmatch
	{
		mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
		loc = locOfDeath;

		if (gameSettings & GS_TEAMPLAY) // Define these if the game is teamplay, else default to 0
		{
			vt = self->client->resp.team;
			ttk = current_round_length / 10;
		}
		
		Q_strncpyz(v, Info_ValueForKey(self->client->pers.userinfo, "steamid"), sizeof(v));
		Q_strncpyz(vn, Info_ValueForKey(self->client->pers.userinfo, "name"), sizeof(vn));
		Q_strncpyz(vip, Info_ValueForKey(self->client->pers.userinfo, "ip"), sizeof(vip));
		Q_strncpyz(vd, Info_ValueForKey(self->client->pers.userinfo, "cl_discord_id"), sizeof(vd));

		// Separate IP from Port
		char* portSeperator = ":";
		vi = strtok(vip, portSeperator);

		gametime = level.matchTime;
		eventtime = (int)time(NULL);
		roundNum = game.roundNum + 1;

		Com_sprintf(
			msg, sizeof(msg),
			"{\"frag\":{\"sid\":\"%s\",\"mid\":\"%s\",\"v\":\"%s\",\"vn\":\"%s\",\"vi\":\"%s\",\"vt\":%i,\"vl\":%i,\"k\":\"%s\",\"kn\":\"%s\",\"ki\":\"%s\",\"kt\":%i,\"kl\":%i,\"w\":%i,\"i\":%i,\"l\":%i,\"ks\":%i,\"gm\":%i,\"gmf\":%i,\"ttk\":%d,\"t\":%d,\"gt\":%d,\"m\":\"%s\",\"r\":%i,\"vd\":\"%s\",\"kd\":\"%s\"}}\n",
			server_id->string,
			game.matchid,
			v,
			vn,
			vi,
			vt,
			vl,
			v,
			vn,
			vi,
			vt,
			vl,
			mod,
			16, // No attacker for world death, setting to 16 as a 'dummy' value
			loc,
			0, // No killstreak for world death, setting to 0 permanently as we aren't tracking world death kill streaks
			Gamemode(),
			Gamemodeflag(),
			ttk,
			eventtime,
			gametime,
			level.mapname,
			roundNum,
			vd,
			vd
		);
		Write_Stats(msg);
	}
}

/*
==================
LogMatch
=================
*/
void LogMatch()
{
	int eventtime;
	char msg[1024];
	int t1 = teams[TEAM1].score;
	int t2 = teams[TEAM2].score;
	int t3 = teams[TEAM3].score;
	eventtime = (int)time(NULL);

	// Check if there's an AI bot in the game, if so, do nothing
	if (game.ai_ent_found) {
		return;
	}

	// Don't log a scoreless match, nothing happened
	if (t1 == 0 && t2 == 0 && t3 == 0) {
		return;
	}

	Com_sprintf(
		msg, sizeof(msg),
		"{\"gamematch\":{\"mid\":\"%s\",\"sid\":\"%s\",\"t\":\"%d\",\"m\":\"%s\",\"gm\":%i,\"gmf\":%i,\"t1\":%i,\"t2\":%i,\"t3\":%i}}\n",
		game.matchid,
		server_id->string,
		eventtime,
		level.mapname,
		Gamemode(),
		Gamemodeflag(),
		t1,
		t2,
		t3
	);
	Write_Stats(msg);
}

/*
==================
LogAward
=================
*/
void LogAward(char* steamid, char* discordid, int award)
{
	int gametime = 0;
	int eventtime;
	char msg[1024];
	int mod;
	mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;

	gametime = level.matchTime;
	eventtime = (int)time(NULL);

	// Check if there's an AI bot in the game, if so, do nothing
	if (game.ai_ent_found) {
		return;
	}

	Com_sprintf(
		msg, sizeof(msg),
		"{\"award\":{\"sid\":\"%s\",\"mid\":\"%s\",\"t\":\"%d\",\"gt\":\"%d\",\"a\":%i,\"k\":\"%s\",\"w\":%i,\"d\":\"%s\"}}\n",
		server_id->string,
		game.matchid,
		eventtime,
		gametime,
		award,
		steamid,
		mod,
		discordid
	);
	Write_Stats(msg);
}

/*
==================
LogEndMatchStats
=================
*/
void LogEndMatchStats()
{
	int i;
	char msg[1024];
	gclient_t *sortedClients[MAX_CLIENTS], *cl;
	int totalClients, secs, shots;
	double accuracy, fpm;
	char steamid[24];
	char discordid[24];
	totalClients = G_SortedClients(sortedClients);

	// Check if there's an AI bot in the game, if so, do nothing
	if (game.ai_ent_found) {
		return;
	}

	for (i = 0; i < totalClients; i++){
		cl = sortedClients[i];
		shots = min( cl->resp.shotsTotal, 9999 );
		secs = (level.framenum - cl->resp.enterframe) / HZ;

		if (shots)
				accuracy = (double)cl->resp.hitsTotal * 100.0 / (double)cl->resp.shotsTotal;
			else
				accuracy = 0;
			if (secs > 0)
				fpm = (double)cl->resp.score * 60.0 / (double)secs;
			else
				fpm = 0.0;
				
		Q_strncpyz(steamid, Info_ValueForKey(cl->pers.userinfo, "steamid"), sizeof(steamid));
		Q_strncpyz(discordid, Info_ValueForKey(cl->pers.userinfo, "cl_discord_id"), sizeof(discordid));

		Com_sprintf(
			msg, sizeof(msg),
			"{\"matchstats\":{\"sid\":\"%s\",\"mid\":\"%s\",\"s\":\"%s\",\"sc\":%i,\"sh\":%i,\"a\":%f,\"f\":%f,\"dd\":%i,\"d\":%i,\"k\":%i,\"ctfc\":%i,\"ctfcs\":%i,\"ht\":%i,\"tk\":%i,\"t\":%i,\"hks\":%i,\"hhs\":%i,\"dis\":\"%s\",\"pt\":%i}}\n",
			server_id->string,
			game.matchid,
			steamid,
			cl->resp.score,
			shots,
			accuracy,
			fpm,
			cl->resp.damage_dealt,
			cl->resp.deaths,
			cl->resp.kills,
			cl->resp.ctf_caps,
			cl->resp.ctf_capstreak,
			cl->resp.hitsTotal,
			cl->resp.team_kills,
			cl->resp.team,
			cl->resp.streakKillsHighest,
			cl->resp.streakHSHighest,
			discordid,
			secs
		);
		Write_Stats(msg);
	}
}
#endif