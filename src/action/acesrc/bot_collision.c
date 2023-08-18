//---------------------------------------------
// bot_collision.c
//
// Copyright (c) 2000 Connor Caple all rights reserved
//
// This file contains the bot collision tests to enable movement
//---------------------------------------------
/*
 * $Log: /LTK2/src/acesrc/bot_collision.c $
 * 
 * 4     2/03/00 13:55 Riever
 * Added CheckShot and removed a debug TempLaser
 * 
 * 3     21/02/00 15:16 Riever
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 * 
 * 2     20/02/00 20:27 Riever
 * Added new members and definitions ready for 2nd generation of bots.
 */

#include "../g_local.h"
#include "../m_player.h"

//----------------------------------------------
// BOTCOL_CanJumpForward
//----------------------------------------------

qboolean	BOTCOL_CanJumpForward(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, temp, dest;
	trace_t	trace;


	VectorCopy (self->s.origin, temp);
	temp[2]+=22; // create a point in line with bot's eyes
	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	VectorMA(temp, TJUMP_DIST, dir, dest);

	trace = gi.trace(temp, tv(-16,-16,0), tv(16,16,56), dest, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);

	if (trace.fraction == 1.0)
		return (true);
	return (false);
}

//----------------------------------------------
// BOTCOL_CanCrawl
//
// Checks a viewport in front of the entity and returns true
// if it can be crawled into
//----------------------------------------------

qboolean	BOTCOL_CanCrawl(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, dest;
	trace_t	trace;


	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	VectorMA(self->s.origin, TCRAWL_DIST, dir, dest);

	// Trying to trace from FeetPos to CrawlHeightPos
	trace = gi.trace(self->s.origin, tv(-16,-16,-22), tv(16,16,0), dest, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);
	// Complete trace possible
	if (trace.fraction == 1.0)
		return (true);
	// Failed
	return (false);
}

//---------------------------------------------
// BOTCOL_CanStand
//
// Returns true if it's possible to stand up.
//---------------------------------------------
qboolean	BOTCOL_CanStand(edict_t	*self)
{
	static vec3_t	maxs = {16, 16, 32};
	trace_t trace;

	trace = gi.trace(self->s.origin, self->mins, maxs, self->s.origin, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);
	else
		return (true);
}

//---------------------------------------------
// BOTCOL_CanJumpUp
//
// Returns true if it's possible to jump up.
//---------------------------------------------
qboolean	BOTCOL_CanJumpUp(edict_t	*self)
{
	static vec3_t	maxs = {16, 16, 64};
	trace_t trace;

	trace = gi.trace(self->s.origin, self->mins, maxs, self->s.origin, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);
	else
		return (true);
}

//---------------------------------------------
// BOTCOL_CanMoveForward
//
// Returns true if it's possible to move forward
//---------------------------------------------

qboolean	BOTCOL_CanMoveForward(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, temp, dest;
	trace_t	trace;


	VectorCopy (self->s.origin, temp);
	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	VectorMA(temp, TMOVE_DIST, dir, dest);
	// Debugging
//	BOTUT_TempLaser (temp, dest);

	// Trying to trace from FeetPos to HeightPos
	// Take a little off the bottom of the box from -22 to allow for slopes
	trace = gi.trace(temp, tv(-16,-16,-8), tv(16,16,32), dest, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);

	if (trace.fraction == 1.0)
		return (true);
	// Failed
	return (false);
}

//-----------------------------------------------
// BOTCOL_WaterMoveForward
//
// Returns true if botcan move forward in water
//-----------------------------------------------

qboolean	BOTCOL_WaterMoveForward(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, dest;
	trace_t	trace;


	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	VectorMA(self->s.origin, TWATER_DIST, dir, dest);

	// Trying to trace from FeetPos to HeightPos
	// Take a little off the bottom of the box from -22 to allow for slopes
	trace = gi.trace(self->s.origin, tv(-16,-16,-10), tv(16,16,32), dest, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);

	if (trace.fraction == 1.0)
		return (true);
	// Failed
	return (false);
}

//=======================================================
// BOTCOL_CanLeaveWater
//=======================================================
//
qboolean	BOTCOL_CanLeaveWater(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, temp, dest;
	trace_t	trace;


	VectorCopy (self->s.origin, temp);
	temp[2]+=22; // create a point in line with bot's eyes
	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	VectorMA(temp, TWEDGE_DIST, dir, dest);

	trace = gi.trace(temp, tv(-16,-16,0), tv(16,16,56), dest, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false);

	if (trace.fraction == 1.0)
		return (true);

	return (false);
}


//----------------------------------------------
// BOTCOL_CanMoveSafely
//
// Checks for lava, slime and long drops
//----------------------------------------------
//#define	MASK_DEADLY				(CONTENTS_LAVA|CONTENTS_SLIME)

qboolean	BOTCOL_CanMoveSafely(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, dest1, dest2;
	trace_t	trace;
	//float	this_dist;

	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	// create a position in front of the bot
	VectorMA(self->s.origin, TRACE_DIST_SHORT, dir, dest1);

	// Modified to check  for crawl direction
	//BOTUT_TempLaser (self->s.origin, dest1);
	trace = gi.trace(self->s.origin, tv(-16,-16,0), tv(16,16,0), dest1, self, MASK_PLAYERSOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (true);

	if (trace.fraction > 0)
	{	// check that destination is onground, or not above lava/slime
		dest1[0] = trace.endpos[0];
		dest1[1] = trace.endpos[1];
		dest1[2] = trace.endpos[2] - 28;
		//this_dist = trace.fraction * TRACE_DIST_SHORT;

		if (gi.pointcontents(dest1) & MASK_PLAYERSOLID)
			return (true);
		// create a position a distance below it
		VectorCopy( trace.endpos, dest2);
		dest2[2] -= TRACE_DOWN;
		//BOTUT_TempLaser (trace.endpos, dest2);
		trace = gi.trace(trace.endpos, VEC_ORIGIN, VEC_ORIGIN, dest2, self, MASK_PLAYERSOLID | MASK_DEADLY);

		if( (trace.fraction == 1.0) // long drop!
			|| (trace.contents & MASK_DEADLY) )	// avoid SLIME or LAVA
		{
			return (false);
		}
		else
		{
			return (true);
		}
	}
	//gi.bprintf(PRINT_HIGH,"Default failure from LAVACHECK\n");
	return (false);
}

//----------------------------------------------
// BOTCOL_CanStrafeSafely
//
// Checks for lava, slime and long drops
//----------------------------------------------
//#define	MASK_DEADLY				(CONTENTS_LAVA|CONTENTS_SLIME)

qboolean	BOTCOL_CanStrafeSafely(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, dest1, dest2;
	trace_t	trace;
	//float	this_dist;
	int	sign;

	sign = (self->bot_strafe > 0) ? (-1) : (1);
	VectorClear(angle);
	angle[1] = anglemod(angles[1] + (sign * 90));//[forward][right][up]

	AngleVectors(angle, dir, NULL, NULL);
	
	// create a position to the side of the bot
	VectorMA(self->s.origin, TRACE_DIST_STRAFE, dir, dest1);

	// Modified to check  for crawl direction
	//BOTUT_TempLaser (self->s.origin, dest1);
	trace = gi.trace(self->s.origin, tv(-16,-16,-22), tv(16,16,0), dest1, self, MASK_SOLID);

	if (trace.fraction > 0)
	{	// check that destination is onground, or not above lava/slime
		dest1[0] = trace.endpos[0];
		dest1[1] = trace.endpos[1];
		dest1[2] = trace.endpos[2] - 24;
		//this_dist = trace.fraction * TRACE_DIST_STRAFE;

		if (gi.pointcontents(dest1) & MASK_PLAYERSOLID)
			return (true);

		// create a position a distance below it
		VectorCopy( trace.endpos, dest2);
		dest2[2] -= TRACE_DOWN_STRAFE;
		trace = gi.trace(trace.endpos, VEC_ORIGIN, VEC_ORIGIN, dest2, self, MASK_SOLID | MASK_DEADLY);

		if( (trace.fraction == 1.0) // don't drop!
			||(trace.contents & MASK_DEADLY) )	// avoid SLIME or LAVA
		{
			return (false);
		}
		else
		{
			return (true);
		}
	}
	//gi.bprintf(PRINT_HIGH,"Default failure from LAVACHECK\n");
	return (false);
}

//----------------------------------------------
// BOTCOL_CanJumpGap
//
//
// Tests for a gap in the floor and if bot can cross it safely
//-----------------------------------------------
qboolean	BOTCOL_CanJumpGap(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, dest1, dest2;
	trace_t	trace;

	VectorClear(angle);
	angle[1] = angles[1];
	AngleVectors(angle, dir, NULL, NULL);
	
	// create a position in front of the bot
	VectorMA(self->s.origin, TJUMP_DIST, dir, dest1);

	// Modified to check  for crawl direction
	//BOTUT_TempLaser (self->s.origin, dest1);
	trace = gi.trace(self->s.origin, tv(-16,-16,-12), tv(16,16,32), dest1, self, MASK_SOLID);

	// Check if we are looking inside a wall!
	if (trace.startsolid)
		return (false); // we need a gap!

	if (trace.fraction !=1.0)
		return (false); // we still need a gap!
	else
	{	// check that destination is not onground
		// create a position a distance below it
		VectorCopy( trace.endpos, dest2);
		dest2[2] -= TRACE_DOWN_STRAFE;
		//BOTUT_TempLaser (trace.endpos, dest2);
		trace = gi.trace(trace.endpos, VEC_ORIGIN, VEC_ORIGIN, dest2, self, MASK_SOLID);// | MASK_DEADLY);

		if(trace.fraction != 1.0) // There is no gap!
		{
			return (false);
		}
		else
		{
			//gi.bprintf(PRINT_HIGH,"CanJumpGap: Checking the gap distance\n");
			//Now look for a longer gap to jump over
			VectorMA(self->s.origin, TRACE_DIST_JUMP, dir, dest1);
			trace = gi.trace(self->s.origin, tv(-16,-16,-12), tv(16,16,32), dest1, self, MASK_SOLID);
			if (trace.fraction < 0.5)
			{
				return (false); // we still need a longer gap!
			}
			dest1[0] = trace.endpos[0];
			dest1[1] = trace.endpos[1];
			dest1[2] = trace.endpos[2] - 28;

			if (gi.pointcontents(dest1) & MASK_PLAYERSOLID)
				// We have a safe landing area
				return (true);
			else // check if we get hurt by jumping here
			{
				VectorCopy( trace.endpos, dest2);
				dest2[2] -= TRACE_DOWN;
				trace = gi.trace(trace.endpos, VEC_ORIGIN, VEC_ORIGIN, dest2, self, (MASK_SOLID | MASK_DEADLY));

				if( (trace.fraction == 1.0) // don't drop too far!
					||(trace.contents & MASK_DEADLY) )	// avoid SLIME or LAVA
				{
					return (false);
				}
				else
				{
					// there is no obvious danger
					return (true);
				}
			}
		}
	}
	//gi.bprintf(PRINT_HIGH,"Default failure from JumpGap\n");
	return (false);
}


//---------------------------------------------
// BOTCOL_CheckBump
//
// Helps steer the bot round protrusions, crate edges, etc., ...
//---------------------------------------------

qboolean BOTCOL_CheckBump (edict_t *bot, usercmd_t *ucmd)
{
    vec3_t angles, forward, right, aim, leftarm, rightarm;
    trace_t ltr, rtr;

    VectorCopy (bot->s.angles, angles);
    angles[0] = 0;
    AngleVectors (angles, forward, right, NULL);

    VectorScale (forward, 80, aim);
    VectorAdd (bot->s.origin, aim, aim);

    VectorScale (right, 16, right);
    VectorAdd (bot->s.origin, right, rightarm);
    VectorInverse (right);
    VectorAdd (bot->s.origin, right, leftarm);

    ltr = gi.trace (leftarm, vec3_origin, vec3_origin, aim, bot, MASK_SOLID);
    rtr = gi.trace (rightarm, vec3_origin, vec3_origin, aim, bot, MASK_SOLID);

    if ((ltr.fraction < 1.00) && (rtr.fraction == 1.00))
    {
          //gi.centerprintf (bot, "Strafe RIGHT, goddamit!\n");
		bot->bot_strafe = SPEED_WALK;
		return true;
    }
    if ((ltr.fraction == 1.00) && (rtr.fraction < 1.00))
    {
          //gi.centerprintf (bot, "Strafe LEFT, goddamit!\n");
		bot->bot_strafe = -SPEED_WALK;
		return true;
    }
    if ((ltr.fraction < 1.00) && (rtr.fraction < 1.00))
    {
          //gi.centerprintf (bot, "That's a WALL!\n");
    }

    // Debugging
	// TempNode (aim, 0.2);
	// BOTUT_TempLaser (leftarm, aim);
	// BOTUT_TempLaser (rightarm, aim);
	return false;
}

//---------------------------------------------
// BOTCOL_Visible
//
// returns 1 if any part of the entity is visible to bot
// (Yes - accurate bot vision is based on collision detection!)
//---------------------------------------------

qboolean BOTCOL_Visible (edict_t *bot, edict_t *other)
{
    vec3_t  vEyes, vForward, actvec, offset3;
	vec3_t	origin, sawhim, vTemp;
    vec_t	offset;
    trace_t tr1, tr2, tr3;  // 1 = bottom right
                            // 2 = top left
                            // 3 = origin

	// We don't use the eye height since the test is for shooting.
    VectorCopy (bot->s.origin, vEyes);
    VectorCopy (other->s.origin, origin);

    if (other->client)
    {
        if (other->client->ps.pmove.pm_flags & PMF_DUCKED)
        {
                origin[2] -= 18;
        }
    }

	// May change this later to allow for glass / breakable etc..
    tr1 = gi.trace (vEyes, vec3_origin, vec3_origin, other->absmin, bot, MASK_PLAYERSOLID);
    tr2 = gi.trace (vEyes, vec3_origin, vec3_origin, other->absmax, bot, MASK_PLAYERSOLID);
    tr3 = gi.trace (vEyes, vec3_origin, vec3_origin, origin, bot, MASK_PLAYERSOLID);

    VectorCopy (vec3_origin, sawhim);
    if (tr1.fraction == 1.0)
            VectorCopy (other->absmin, sawhim);
    if (tr2.fraction == 1.0)
            VectorCopy (other->absmax, sawhim);
    if (tr3.fraction == 1.0)
            VectorCopy (origin, sawhim);

    if (VectorCompare(sawhim, vec3_origin) == 0)
    {
        AngleVectors (bot->s.angles, vForward, NULL, NULL);
        VectorSubtract (sawhim, bot->s.origin, actvec);
        VectorNormalize (actvec);
        VectorSubtract (actvec, vForward, offset3);
        offset = VectorLength (offset3);

        // Within Field Of View?
        if (offset < ROOT2)
			return true;

		// Have we already seen this enemy
		if( other == bot->enemy )
		{
			VectorSubtract(other->s.origin, bot->s.origin, vTemp);
			// Is it too close for us to ignore?
			if( VectorLength(vTemp) < 100.0)
				return true;
		}
    }
	return false;
}

//----------------------------------------------
// BOTCOL_CheckShot
//
// See if we can hit the target without hitting a teammate
//----------------------------------------------

qboolean BOTCOL_CheckShot(edict_t *bot)
{
	trace_t tTrace;

	tTrace = gi.trace (bot->s.origin, vec3_origin, vec3_origin, bot->enemy->s.origin, bot, MASK_SOLID|MASK_OPAQUE);

	// If we are in teamplay, check enemy team.
	if(teamplay->value && tTrace.ent && tTrace.ent->client)
	{
		if( tTrace.ent->client->resp.team == bot->client->resp.team )
			return (false);
	}
	// Clear shot.
	return true;
}

//----------------------------------------------
// BOTCOL_CanReachItem
//
// This _really_ needs more work!
//----------------------------------------------

qboolean BOTCOL_CanReachItem(edict_t *bot, vec3_t goal)
{
	trace_t tTrace;
	vec3_t v;

	VectorCopy(bot->mins,v);
	v[2] += 18; // Stepsize

	tTrace = gi.trace (bot->s.origin, v, bot->maxs, goal, bot, MASK_SOLID|MASK_OPAQUE);
	
	// Yes we can see it
	if (tTrace.fraction == 1.0)
		return true;
	else
		return false;
}
