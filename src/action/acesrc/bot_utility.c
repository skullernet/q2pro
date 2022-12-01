//---------------------------------------------
// bot_utility.c
//
// Contains utility code used by other functions for debugging etc.
//
// Copyright (c) 2000 Connor Caple all rights reserved
//---------------------------------------------
/*
 * $Log: /LTK2/src/acesrc/bot_utility.c $
 * 
 * 7     2/03/00 11:34 Riever
 * Added MakeTargetVector to set up fake jump targets.
 * 
 * 6     24/02/00 20:06 Riever
 * Shownodes distance added.
 */

#include "../g_local.h"


#define SHOWNODES_DIST		384	// Max range for shownodes
//---------------------------------------------
//	BOTUT_TempLaser()
//	start - where the laser starts from
//	end - where the laser will end
//
// (Got this one from XoXus)
//---------------------------------------------
void BOTUT_TempLaser (vec3_t start, vec3_t end)
{
        gi.WriteByte (svc_temp_entity);
        gi.WriteByte (TE_BFG_LASER);
        gi.WritePosition (start);
        gi.WritePosition (end);
        gi.multicast (start, MULTICAST_ALL);
}


//---------------------------------------------
// BOTUT_NodeMarker
//---------------------------------------------

void BOTUT_NodeMarker (int iNode)
{
	edict_t *ent;

	ent = G_Spawn();
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;

	if(nodes[iNode].type == NODE_MOVE)
		ent->s.renderfx = RF_SHELL_BLUE;
	else if (nodes[iNode].type == NODE_WATER)
		ent->s.renderfx = RF_SHELL_RED;
	else			
		ent->s.renderfx = RF_SHELL_GREEN; // action nodes

	ent->s.modelindex = gi.modelindex ("models/items/ammo/grenades/medium/tris.md2");
	ent->owner = ent;
	ent->nextthink = level.framenum + 0.2 * HZ;
	ent->think = G_FreeEdict;
	ent->dmg = 0;

	VectorCopy(nodes[iNode].origin,ent->s.origin);
	gi.linkentity (ent);

}

//----------------------------------------------
// BOTUT_ShowNodes
//
// This routine creates a temp node entity for the ent's viewport
// It only shows nodes that are currently visible.
//----------------------------------------------

void BOTUT_ShowNodes (edict_t *ent)
{
    int i;
    vec3_t vEyes, vNodePos, vForward, vLos, vOffset, vDiff;    // Los = Line Of Sight
    node_t *pThisNode;
    trace_t tTrace;
    float fFudge;

    if ((!ent) || (!ent->client))
		return;

    VectorCopy (ent->s.origin, vEyes);
    vEyes[2] += ent->viewheight;

    for (i=0; i < numnodes; i++)
    {
        pThisNode = &nodes[i];
        if (pThisNode->nodenum < 0)
                continue;
        VectorCopy (pThisNode->origin, vNodePos);

		// Distance check for visibility
		VectorSubtract(vEyes, vNodePos, vDiff);
		if( VectorLength(vDiff) > SHOWNODES_DIST )
			continue;

        tTrace = gi.trace (vEyes, vec3_origin, vec3_origin,
                       vNodePos, ent, MASK_OPAQUE);
        if (tTrace.fraction < 0.9)
                continue;
        // Ok, can see the node
        // But is it in the viewport?
        AngleVectors (ent->s.angles, vForward, NULL, NULL);
        //VectorNormalize (forward);
        VectorAdd (vForward, vEyes, vForward);
        VectorSubtract (vNodePos, vEyes, vLos);
        VectorNormalize (vLos);
        VectorAdd (vLos, vEyes, vLos);
        VectorSubtract (vLos, vForward, vOffset);
        fFudge = VectorLength (vOffset);
        if ((1 - (fFudge * fFudge / 2)) > COS90)
        {
			BOTUT_NodeMarker(i);
        }
    }
}

//----------------------------------------------
// BOTUT_Cmd_Say_f
//----------------------------------------------

void BOTUT_Cmd_Say_f (edict_t *ent, char *pMsg)
{
	int     j, /*i,*/ offset_of_text;
	edict_t *other;
	char    text[2048];
	//gclient_t *cl;


	if (!teamplay->value)
		return;

	if (ent->client->resp.team == NOTEAM)
		return;

	Q_snprintf (text, sizeof(text), "%s%s: ", 
		(teamplay->value && (ent->solid == SOLID_NOT || ent->deadflag == DEAD_DEAD)) ? "[DEAD] " : "",
		ent->client->pers.netname);

	offset_of_text = strlen(text);  //FB 5/31/99

	strcat (text, pMsg);

	// don't let text be too long for malicious reasons
	// ...doubled this limit for Axshun -FB
	if (strlen(text) > 300)
		text[300] = 0;

	if (ent->solid != SOLID_NOT && ent->deadflag != DEAD_DEAD)
		ParseSayText(ent, text + offset_of_text, strlen(text));  //FB 5/31/99 - offset change
                            // this will parse the % variables, 
                            // and again check 300 limit afterwards -FB
                            // (although it checks it without the name in front, oh well)

	strcat(text, "\n");

/*
	if (flood_msgs->value)
	{
		cl = ent->client;

        if (level.time < cl->flood_locktill) 
			return;

		i = cl->flood_whenhead - flood_msgs->value + 1;
		if (i < 0)
			i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && level.time - cl->flood_when[i] < flood_persecond->value) 
		{
			cl->flood_locktill = level.time + flood_waitdelay->value;
			return;
		}
		cl->flood_whenhead = (cl->flood_whenhead + 1) % (sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}
*/

	if (dedicated->value)
		safe_cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		if (!OnSameTeam(ent, other))
			continue;
		if (teamplay->value && team_round_going)
		{
			if ((ent->solid == SOLID_NOT || ent->deadflag == DEAD_DEAD)
			&&  (other->solid != SOLID_NOT && other->deadflag != DEAD_DEAD))
				continue;
		} 

		safe_cprintf(other, PRINT_CHAT, "%s", text);
	}
}

//----------------------------------------------
// BOTUT_MakeTargetVector
//
// Creates a fake jump vector for the bot to aim at
//----------------------------------------------
void	BOTUT_MakeTargetVector(edict_t *bot, vec3_t angles, vec3_t vDest)
{
	vec3_t	vDir, vAngle, vTemp;

	VectorCopy (bot->s.origin, vTemp);
	vTemp[2]+=22; // create a point in line with bot's eyes
	VectorClear(vAngle);
	vAngle[1] = angles[1];

	AngleVectors(vAngle, vDir, NULL, NULL);// Forward vector
	
	VectorMA(vTemp, 64, vDir, vDest);
}
