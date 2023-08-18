//---------------------------------------------
// bot_movement.c
//
// Copyright (c) 2000 Connor Caple all rights reserved
//
// This file contains the bot movement routines
//---------------------------------------------
/*
 * $Log: /LTK2/src/acesrc/bot_movement.c $
 * 
 * 3     2/03/00 15:37 Riever
 * SetJumpVelocity, SetRandomDirection added.
 * Roam movement code rewritten to be more useful.
 * 
 * 2     21/02/00 15:16 Riever
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 */

#include "../g_local.h"

void	Cmd_Kill_f (edict_t *bot);

//---------------------------------------------
// BOTMV_Roaming
//
// Aimlessly wanders about the level bumping into things ;)
//---------------------------------------------

void	BOTMV_Roaming( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
	float fRoam, fLen;
	vec3_t	vTvec, vTempDest;

	//deal with completely stuck states first:
	VectorSubtract(bot->lastPosition, bot->s.origin, vTvec);
	// Check if we have been able to move using length between points
	// if we are stuck, deal with it.
	fLen = VectorLength(vTvec);
	if( (fLen <= 2.0) )
	{
		// we are stuck!
		//gi.bprintf(PRINT_HIGH,"BS_WAIT: Stuck!\n");
		fRoam = BOTMV_FindBestDirection (bot, vTempDest, angles);
		if (fRoam > 0)
		{
			// Level up our line of vision
			angles[2] = bot->s.angles[2];
			// Turn to face that direction
			BOTCOM_AimAt (bot, vTempDest, angles);
			bot->bCrawl = false;
			bot->bLastJump = false;
			bot->bot_speed = SPEED_ROAM;
			cmd->upmove = SPEED_WALK; // jump to get out of trouble
		}
		else if(fRoam == -1) // we are really stuck!
		{
			Cmd_Kill_f (bot); // suicide if stuck
		}
	}
	else if( BOTCOL_CanMoveSafely(bot, angles))
	{
		// Set probable walking speed
		bot->bot_speed = SPEED_WALK;

		if ( !BOTCOL_CanMoveForward(bot, angles) )
		{
			// Is it a corner edge?
			if( 
				(BOTCOL_CheckBump(bot, cmd))
				&& ( BOTCOL_CanStrafeSafely(bot, angles ))
				)
			{
				cmd->sidemove = bot->bot_strafe;
				//Just carry on since this will set a strafe up for us
			}
			//Can we crawl forward?
			else if( BOTCOL_CanCrawl(bot, angles))
			{
				cmd->upmove = -200;
				bot->bCrawl = true;
			}
			else if( 
				(BOTCOL_CanJumpForward(bot, angles))
				|| (BOTCOL_CanJumpGap(bot, angles))
				)
			{
				cmd->upmove = 400;

				// Make a point in front of the bot
				BOTUT_MakeTargetVector(bot, angles, vTempDest);
				BOTMV_SetJumpVelocity(bot, angles, vTempDest);
			}
			else
			{
				// Cannot go forward
				BOTMV_SetRandomDirection(bot, angles);
			}
		}
	}
	else
	{
		// No safe move available
		BOTMV_SetRandomDirection(bot, angles);
	}
}

//============================================================
// BOTMV_FindBestDirection
//============================================================
//
//
float	BOTMV_FindBestDirection(edict_t	*bot, vec3_t vBestDest, vec3_t angles)
{
	float	fBestDist = -1.0, fThisDist;
	int		i;
	vec3_t	vDir, vAngle, vThisAngle, vDest, vDest2;
	//vec3_t vMins;
	trace_t	tTrace;

//	if (bot->fLastBestDirection > (level.time - EYES_FREQ))
//		return (0); // This means called too soon
//	bot->fLastBestDirection = level.time;

	//VectorAdd(bot->mins, tv(0,0,STRIDESIZE), vMins);

	// check eight compass directions
	VectorClear(vAngle);
	VectorClear(vThisAngle);
	vAngle[1] = angles[1];

	// start at center, then fan out in 45 degree intervals, swapping between + and -
	for (i=1; i<8; i++)
	{
		if (i<7 && random() < 0.2)	// skip random intervals
			i++;

		if( ((i==3) ||(i==4)) && random() < 0.5)
			i=5; // cut out 90 degree turns from time to time

		// Take alternate 45deg steps
		vThisAngle[1] = anglemod(vAngle[1] + ((((i % 2)*2 - 1) * (int) (ceil(i/2))) * 45));
		AngleVectors(vThisAngle, vDir, NULL, NULL);		
		VectorMA(bot->s.origin, TRACE_DIST, vDir, vDest);

		// Modified to check  for crawl direction
		tTrace = gi.trace(bot->s.origin, tv(-16,-16,0), tv(16,16,0), vDest, bot, MASK_PLAYERSOLID);

		// Check if we are looking inside a wall!
		if (tTrace.startsolid)
			continue;

		if (tTrace.fraction > 0)
		{	// check that destination is onground, or not above lava/slime
			vDest[0] = tTrace.endpos[0];
			vDest[1] = tTrace.endpos[1];
			vDest[2] = tTrace.endpos[2] - 24;
			fThisDist = tTrace.fraction * TRACE_DIST;

			if (gi.pointcontents(vDest) & MASK_PLAYERSOLID)
				goto nocheckground;


			VectorCopy(tTrace.endpos, vDest2);
			vDest2[2] -= 256;
			tTrace = gi.trace(tTrace.endpos, VEC_ORIGIN, VEC_ORIGIN, vDest2, bot, MASK_PLAYERSOLID | MASK_WATER);

			if ( ! ((tTrace.fraction == 1) || (tTrace.contents & MASK_DEADLY)))	// avoid ALL forms of liquid for now
			{
				//DEBUG
				//gi.dprintf("Dist %i: %i\n", (int) vThisAngle[1], (int) fThisDist);
				if (tTrace.fraction > 0.4)		// if there is a drop in this direction, try to avoid it if possible
					fThisDist *= 0.5;
			}
			else
			{
				//gi.bprintf(PRINT_HIGH,"BestDir : Ouch! Not going there\n");
				fThisDist = 0; // Do _not_ go there!
			}
nocheckground:
			if (fThisDist > fBestDist)
			{
				fBestDist = fThisDist;
				VectorCopy( vDest, vBestDest); // save the best found so far
				//Head for the wide open spaces :)
				if(fThisDist == TRACE_DIST)
					break;
			}
		}
	}
	// Send back a vec3_t to aim at in vBestDest and return a code to show what happened
	// -1 == nothing found  0 == called too soon fNUM == distance safe to travel
	return (fBestDist);
}

//---------------------------------------------
// BOTMV_SetJumpVelocity
//
// vTempDest is a point the bot wants to jump to
//---------------------------------------------

void	BOTMV_SetJumpVelocity(edict_t *bot, vec3_t angles, vec3_t vTempDest)
{
	vec3_t	vDest;

	VectorSubtract(vTempDest, bot->s.origin, vDest);
	VectorNormalize(vDest);
	// Velocity hack
	VectorScale(vDest,440,bot->velocity);
}

//----------------------------------------------
// BOTMV_SetRandomDirection
//----------------------------------------------

void	BOTMV_SetRandomDirection(edict_t *bot, vec3_t angles)
{
	vec3_t	vTempDest;

	BOTMV_FindBestDirection (bot, vTempDest, angles);
	// Level up our line of vision
	angles[2] = bot->s.angles[2];
	// Turn to face that direction
	BOTCOM_AimAt (bot, vTempDest, angles);
	bot->bCrawl = false;
	bot->bLastJump = false;
	// Stop walking while we turn
	bot->bot_speed = SPEED_ROAM;
}
