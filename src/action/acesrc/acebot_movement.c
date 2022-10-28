///////////////////////////////////////////////////////////////////////
//
//  ACE - Quake II Bot Base Code
//
//  Version 1.0
//
//  Original file is Copyright(c), Steve Yeager 1998, All Rights Reserved
//
//
//	All other files are Copyright(c) Id Software, Inc.
////////////////////////////////////////////////////////////////////////
/*
 * $Header: /LTK2/src/acesrc/acebot_movement.c 7     29/02/00 22:58 Riever $
 *
 * $Log: /LTK2/src/acesrc/acebot_movement.c $
 * 
 * 7     29/02/00 22:58 Riever
 * Corner avoidance code added.
 * 
 * 6     29/02/00 11:18 Riever
 * Attack function changes:
 *   Sniper accuracy increased
 *   JumpKick attack added
 *   Knife throw added
 *   Safety check before advancing for knife or kick attack
 * Jump Changed to use a velocity hack.
 * Jump node support fixed and working well.
 * Ledge handling code removed - no longer used.
 * 
 * 5     24/02/00 20:02 Riever
 * New door handling code in place.
 * 
 * User: Riever       Date: 23/02/00   Time: 23:18
 * New door handling code in place - must have route file.
 * User: Riever       Date: 21/02/00   Time: 15:16
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 * User: Riever       Date: 20/02/00   Time: 20:27
 * Added new members and definitions ready for 2nd generation of bots.
 * 
 */
	
///////////////////////////////////////////////////////////////////////
//  acebot_movement.c - This file contains all of the 
//                      movement routines for the ACE bot
//           
///////////////////////////////////////////////////////////////////////

#include "../g_local.h"

#include "acebot.h"
#include "botchat.h"

qboolean ACEAI_CheckShot(edict_t *self);
void ACEMV_ChangeBotAngle (edict_t *ent);
qboolean	ACEND_LadderForward( edict_t *self );
qboolean ACEMV_CanJump(edict_t *self);

/*
=============
M_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes = 0, c_no = 0;

#define STEPSIZE 18

void VectorRotate2( vec3_t v, float degrees )
{
	float radians = DEG2RAD(degrees);
	float x = v[0], y = v[1];
	v[0] = x * cosf(radians) - y * sinf(radians);
	v[1] = y * cosf(radians) + x * sinf(radians);
}

qboolean M_CheckBottom( edict_t *ent )
{
	vec3_t mins = {0,0,0}, maxs = {0,0,0}, start = {0,0,0}, stop = {0,0,0};
	trace_t trace;
	int x = 0, y = 0;
	float mid = 0, bottom = 0;
	
	VectorAdd( ent->s.origin, ent->mins, mins );
	VectorAdd( ent->s.origin, ent->maxs, maxs );
	
	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for( x = 0; x <= 1; x ++ )
		for( y = 0; y <= 1; y ++ )
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if( gi.pointcontents(start) != CONTENTS_SOLID )
				goto realcheck;
		}
	
	c_yes ++;
	return true;  // we got out easy
	
realcheck:
	c_no ++;
	//
	// check it for real...
	//
	start[2] = mins[2];
	
	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5;
	start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5;
	stop[2] = start[2] - 2 * STEPSIZE;
	trace = gi.trace( start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID );
	
	if( trace.fraction == 1.0 )
		return false;
	mid = bottom = trace.endpos[2];
	
	// the corners must be within 16 of the midpoint        
	for( x = 0; x <= 1; x ++ )
		for( y = 0; y <= 1; y ++ )
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];
			
			trace = gi.trace( start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID );
			
			if( trace.fraction != 1.0 && trace.endpos[2] > bottom )
				bottom = trace.endpos[2];
			if( trace.fraction == 1.0 || mid - trace.endpos[2] > STEPSIZE )
				return false;
		}
	
	c_yes ++;
	return true;
}

//=============================================================
// CanMoveSafely (dronebot)
//=============================================================
// Checks for lava and slime
#define	MASK_DEADLY				(CONTENTS_LAVA|CONTENTS_SLIME)
//#define TRACE_DIST_SHORT 48
//#define TRACE_DOWN 96
//#define VEC_ORIGIN tv(0,0,0)
//

qboolean	CanMoveSafely(edict_t	*self, vec3_t angles)
{
	vec3_t	dir, angle, dest1, dest2;
	trace_t	trace;
	//float	this_dist;

//	self->bot_ai.next_safety_time = level.time + EYES_FREQ;

	VectorClear(angle);
	angle[1] = angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	
	// create a position in front of the bot
	VectorMA(self->s.origin, TRACE_DIST_SHORT, dir, dest1);

	// Modified to check  for crawl direction
	//trace = gi.trace(self->s.origin, mins, self->maxs, dest, self, MASK_SOLID);
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
///////////////////////////////////////////////////////////////////////
// Checks if bot can move (really just checking the ground)
// Also, this is not a real accurate check, but does a
// pretty good job and looks for lava/slime. 
///////////////////////////////////////////////////////////////////////
qboolean ACEMV_CanMove(edict_t *self, int direction)
{
	vec3_t forward, right;
	vec3_t offset,start,end;
	vec3_t angles;
	trace_t tr;

	// Now check to see if move will move us off an edge
	VectorCopy(self->s.angles,angles);
	
	if(direction == MOVE_LEFT)
		angles[1] += 90;
	else if(direction == MOVE_RIGHT)
		angles[1] -= 90;
	else if(direction == MOVE_BACK)
		angles[1] -=180;

	// Set up the vectors
	AngleVectors (angles, forward, right, NULL);
	
	VectorSet(offset, 36, 0, 24);
	G_ProjectSource (self->s.origin, offset, forward, right, start);
		
	VectorSet(offset, 36, 0, -110); // RiEvEr reduced drop distance
	G_ProjectSource (self->s.origin, offset, forward, right, end);
	
	//AQ2 ADDED MASK_SOLID
	tr = gi.trace(start, NULL, NULL, end, self, MASK_SOLID|MASK_OPAQUE);
	
	if(tr.fraction == 1.0 || tr.contents & (CONTENTS_LAVA|CONTENTS_SLIME))
	{
		if( self->last_door_time < level.framenum )
		{
			if( debug_mode && (level.framenum % HZ == 0) )
				debug_printf( "%s: move blocked (ACEMV_CanMove)\n", self->client->pers.netname );

			Cmd_OpenDoor_f ( self );	// Open the door
			//self->last_door_time = level.framenum + 3 * HZ; // wait!
		}
		return false;
	}
	
	return true; // yup, can move
}

///////////////////////////////////////////////////////////////////////
// Checks if bot can jump (really just checking the ground)
// Also, this is not a real accurate check, but does a
// pretty good job and looks for lava/slime. 
///////////////////////////////////////////////////////////////////////
qboolean ACEMV_CanJumpInternal(edict_t *self, int direction)
{
	vec3_t forward, right;
	vec3_t offset,start,end;
	vec3_t angles;
	trace_t tr;

	// Now check to see if move will move us off an edge
	VectorCopy(self->s.angles,angles);
	
	if(direction == MOVE_LEFT)
		angles[1] += 90;
	else if(direction == MOVE_RIGHT)
		angles[1] -= 90;
	else if(direction == MOVE_BACK)
		angles[1] -=180;

	// Set up the vectors
	AngleVectors (angles, forward, right, NULL);
	
	VectorSet(offset, 132, 0, 24);
	G_ProjectSource (self->s.origin, offset, forward, right, start);
		
	VectorSet(offset, 132, 0, -110); // RiEvEr reduced drop distance
	G_ProjectSource (self->s.origin, offset, forward, right, end);
	
	//AQ2 ADDED MASK_SOLID
	tr = gi.trace(start, NULL, NULL, end, self, MASK_SOLID|MASK_OPAQUE);
	
	if(tr.fraction == 1.0 || tr.contents & (CONTENTS_LAVA|CONTENTS_SLIME))
	{
		if( self->last_door_time < level.framenum )
		{
			if( debug_mode && (level.framenum % HZ == 0) )
				debug_printf( "%s: move blocked (ACEMV_CanJumpInternal)\n", self->client->pers.netname );

			Cmd_OpenDoor_f ( self );	// Open the door
			//self->last_door_time = level.framenum + 3 * HZ; // wait!
		}
		return false;
	}
	
	return true; // yup, can move
}

qboolean ACEMV_CanJump(edict_t *self)
{
	return( (ACEMV_CanJumpInternal(self, MOVE_LEFT)) &&
			(ACEMV_CanJumpInternal(self, MOVE_RIGHT)) &&
			(ACEMV_CanJumpInternal(self, MOVE_BACK)) &&
			(ACEMV_CanJumpInternal(self, MOVE_FORWARD)) );

}

///////////////////////////////////////////////////////////////////////
// Handle special cases of crouch/jump
//
// If the move is resolved here, this function returns
// true.
///////////////////////////////////////////////////////////////////////
qboolean ACEMV_SpecialMove(edict_t *self, usercmd_t *ucmd)
{
	vec3_t dir,forward,right,start,end,offset;
	vec3_t top;
	trace_t tr; 
	
	// Get current direction
	VectorCopy(self->client->ps.viewangles,dir);
	dir[YAW] = self->s.angles[YAW];
	AngleVectors (dir, forward, right, NULL);

	VectorSet(offset, 0, 0, 0); // changed from 18,0,0
	G_ProjectSource (self->s.origin, offset, forward, right, start);
	offset[0] += 18;
	G_ProjectSource (self->s.origin, offset, forward, right, end);
	
	// trace it
	start[2] += 18; // so they are not jumping all the time
	end[2] += 18;
	tr = gi.trace (start, self->mins, self->maxs, end, self, MASK_MONSTERSOLID);
		
//RiEvEr	if(tr.allsolid)
	if( tr.fraction < 1.0)
	{

		//RiEvEr
		if (Q_stricmp(tr.ent->classname, "func_door_rotating") == 0)
		{
			ucmd->forwardmove = -SPEED_RUN; // back up in case it's trying to open
			return true;
		}
		//R

		// Check for crouching
		start[2] -= 14;
		end[2] -= 14;

		// Set up for crouching check
		VectorCopy(self->maxs,top);
		top[2] = 0.0; // crouching height
		tr = gi.trace (start, self->mins, top, end, self, MASK_PLAYERSOLID);
		
		// Crouch
//RiEvEr		if(!tr.allsolid) 
		if( tr.fraction == 1.0 )
		{
			ucmd->forwardmove = SPEED_RUN;
			ucmd->upmove = -SPEED_RUN;
			return true;
		}
		
		// Check for jump
		start[2] += 32;
		end[2] += 32;
		tr = gi.trace (start, self->mins, self->maxs, end, self, MASK_MONSTERSOLID);

//RiEvEr		if(!tr.allsolid)
		if( tr.fraction == 1.0)
		{	
			ucmd->forwardmove = SPEED_RUN;
			ucmd->upmove = SPEED_RUN;
			return true;
		}
		//RiEvEr
		// My corner management code
		if( BOTCOL_CheckBump(self, ucmd ))
		{
			if( BOTCOL_CanStrafeSafely(self, self->s.angles ) )
			{
				ucmd->sidemove = self->bot_strafe;
				return true;
			}
		}
		//R
	}
	
	return false; // We did not resolve a move here
}


///////////////////////////////////////////////////////////////////////
// Checks for obstructions in front of bot
//
// This is a function I created origianlly for ACE that
// tries to help steer the bot around obstructions.
//
// If the move is resolved here, this function returns true.
///////////////////////////////////////////////////////////////////////
qboolean ACEMV_CheckEyes(edict_t *self, usercmd_t *ucmd)
{
	vec3_t  forward, right;
	vec3_t  leftstart, rightstart,focalpoint;
	vec3_t  /* upstart,*/upend;
	vec3_t  dir,offset;

	trace_t traceRight,traceLeft,/*traceUp,*/ traceFront; // for eyesight

	// Get current angle and set up "eyes"
	VectorCopy(self->s.angles,dir);
	AngleVectors (dir, forward, right, NULL);
	
	// Let them move to targets by walls
	if(!self->movetarget)
//		VectorSet(offset,200,0,4); // focalpoint 
		VectorSet(offset,64,0,4); // focalpoint 
	else
		VectorSet(offset,36,0,4); // focalpoint 
	
	G_ProjectSource (self->s.origin, offset, forward, right, focalpoint);

	// Check from self to focalpoint
	// Ladder code
	VectorSet(offset,36,0,0); // set as high as possible
	G_ProjectSource (self->s.origin, offset, forward, right, upend);
	//AQ2 ADDED MASK_SOLID
	traceFront = gi.trace(self->s.origin, self->mins, self->maxs, upend, self, MASK_SOLID|MASK_OPAQUE);
		
	if(traceFront.contents & 0x8000000) // using detail brush here cuz sometimes it does not pick up ladders...??
	{
		ucmd->upmove = SPEED_RUN;
		ucmd->forwardmove = SPEED_RUN;
		return true;
	}
	
	// If this check fails we need to continue on with more detailed checks
	if(traceFront.fraction == 1)
	{	
		ucmd->forwardmove = SPEED_RUN;
		return true;
	}

	VectorSet(offset, 0, 18, 4);
	G_ProjectSource (self->s.origin, offset, forward, right, leftstart);
	
	offset[1] -= 36; // want to make sure this is correct
	//VectorSet(offset, 0, -18, 4);
	G_ProjectSource (self->s.origin, offset, forward, right, rightstart);

	traceRight = gi.trace(rightstart, NULL, NULL, focalpoint, self, MASK_OPAQUE);
	traceLeft = gi.trace(leftstart, NULL, NULL, focalpoint, self, MASK_OPAQUE);

	// Wall checking code, this will degenerate progressivly so the least cost 
	// check will be done first.
		
	// If open space move ok
	if(traceRight.fraction != 1 || traceLeft.fraction != 1 || strcmp(traceLeft.ent->classname,"func_door")!=0)
	{
//@@ weird code...
/*		// Special uppoint logic to check for slopes/stairs/jumping etc.
		VectorSet(offset, 0, 18, 24);
		G_ProjectSource (self->s.origin, offset, forward, right, upstart);

		VectorSet(offset,0,0,200); // scan for height above head
		G_ProjectSource (self->s.origin, offset, forward, right, upend);
		traceUp = gi.trace(upstart, NULL, NULL, upend, self, MASK_OPAQUE);
			
		VectorSet(offset,200,0,200*traceUp.fraction-5); // set as high as possible
		G_ProjectSource (self->s.origin, offset, forward, right, upend);
		traceUp = gi.trace(upstart, NULL, NULL, upend, self, MASK_OPAQUE);

		// If the upper trace is not open, we need to turn.
		if(traceUp.fraction != 1)*/
		{						
			if(traceRight.fraction > traceLeft.fraction)
				self->s.angles[YAW] += (1.0 - traceLeft.fraction) * 45.0;
			else
				self->s.angles[YAW] += -(1.0 - traceRight.fraction) * 45.0;
			
			ucmd->forwardmove = SPEED_RUN;
			ACEMV_ChangeBotAngle(self);
			return true;
		}
	}
				
	return false;
}

///////////////////////////////////////////////////////////////////////
// Make the change in angles a little more gradual, not so snappy
// Subtle, but noticeable.
// 
// Modified from the original id ChangeYaw code...
///////////////////////////////////////////////////////////////////////
void ACEMV_ChangeBotAngle (edict_t *ent)
{
	float   ideal_yaw;
	float   ideal_pitch;
	float   current_yaw;
	float   current_pitch;
	float   speed;
	vec3_t  ideal_angle;
	float   yaw_move = 0.f;
	float   pitch_move = 0.f;
	float   move_ratio = 1.f;

	// Normalize the move angle first
	VectorNormalize(ent->move_vector);

	current_yaw = anglemod(ent->s.angles[YAW]);
	current_pitch = anglemod(ent->s.angles[PITCH]);

	vectoangles (ent->move_vector, ideal_angle);

	ideal_yaw = anglemod(ideal_angle[YAW]);
	ideal_pitch = anglemod(ideal_angle[PITCH]);

	// Raptor007: Compensate for M4 climb.
	if( (ent->client->weaponstate == WEAPON_FIRING) && (ent->client->weapon == FindItem(M4_NAME)) )
		ideal_pitch -= ent->client->kick_angles[PITCH];

	// Yaw
	if (current_yaw != ideal_yaw)
	{	
		yaw_move = ideal_yaw - current_yaw;
		speed = ent->yaw_speed / (float) BASE_FRAMERATE;
		if (ideal_yaw > current_yaw)
		{
			if (yaw_move >= 180)
				yaw_move = yaw_move - 360;
		}
		else
		{
			if (yaw_move <= -180)
				yaw_move = yaw_move + 360;
		}
		if (yaw_move > 0)
		{
			if (yaw_move > speed)
				yaw_move = speed;
		}
		else
		{
			if (yaw_move < -speed)
				yaw_move = -speed;
		}
	}

	// Pitch
	if (current_pitch != ideal_pitch)
	{	
		pitch_move = ideal_pitch - current_pitch;
		speed = ent->yaw_speed / (float) BASE_FRAMERATE;
		if (ideal_pitch > current_pitch)
		{
			if (pitch_move >= 180)
				pitch_move = pitch_move - 360;
		}
		else
		{
			if (pitch_move <= -180)
				pitch_move = pitch_move + 360;
		}
		if (pitch_move > 0)
		{
			if (pitch_move > speed)
				pitch_move = speed;
		}
		else
		{
			if (pitch_move < -speed)
				pitch_move = -speed;
		}
	}

	// Raptor007: Interpolate towards desired changes at higher fps.
	if( ! FRAMESYNC )
	{
		int frames_until_sync = FRAMEDIV - (level.framenum - 1) % FRAMEDIV;
		move_ratio = 1.f / (float) frames_until_sync;
	}

	ent->s.angles[YAW] = anglemod( current_yaw + yaw_move * move_ratio );
	ent->s.angles[PITCH] = anglemod( current_pitch + pitch_move * move_ratio );
}

///////////////////////////////////////////////////////////////////////
// Set bot to move to it's movetarget. (following node path)
///////////////////////////////////////////////////////////////////////
void ACEMV_MoveToGoal(edict_t *self, usercmd_t *ucmd)
{
	// If a rocket or grenade is around deal with it
	// Simple, but effective (could be rewritten to be more accurate)
	if(strcmp(self->movetarget->classname,"rocket")==0 ||
	   strcmp(self->movetarget->classname,"grenade")==0)
	{
		VectorSubtract (self->movetarget->s.origin, self->s.origin, self->move_vector);
		ACEMV_ChangeBotAngle(self);
		if(debug_mode)
			debug_printf("%s: Oh crap a rocket!\n",self->client->pers.netname);
		
		// strafe left/right
		if( (self->bot_strafe <= 0) && ACEMV_CanMove(self, MOVE_LEFT) )
			ucmd->sidemove = -SPEED_RUN;
		else if(ACEMV_CanMove(self, MOVE_RIGHT))
			ucmd->sidemove = SPEED_RUN;
		else
			ucmd->sidemove = 0;
		self->bot_strafe = ucmd->sidemove;
		return;

	}
	else
	{
		// Are we there yet?
		vec3_t v;
		trace_t	tTrace;
		vec3_t	vStart, vDest;

		if( nodes[self->current_node].type == NODE_DOOR )
		{
			if( nodes[self->next_node].type == NODE_DOOR )
			{
				VectorCopy( nodes[self->current_node].origin, vStart );
				VectorCopy( nodes[self->next_node].origin, vDest );
				VectorSubtract( self->s.origin, nodes[self->next_node].origin, v );
			}
			else
			{
				VectorCopy( self->s.origin, vStart );
				VectorCopy( nodes[self->current_node].origin, vDest );
				VectorSubtract( self->s.origin, nodes[self->current_node].origin, v );
			}

			if(VectorLength(v) < 32)
			{
				// See if we have a clear shot at it
				PRETRACE();
				tTrace = gi.trace( vStart, tv(-16,-16,-8), tv(16,16,8), vDest, self, MASK_PLAYERSOLID);
				POSTTRACE();
				if( tTrace.fraction <1.0 )
				{
					if( (strcmp( tTrace.ent->classname, "func_door_rotating" ) == 0)
					||  (strcmp( tTrace.ent->classname, "func_door") == 0) )
					{
						// The door is in the way
						// See if it's moving
						if( (VectorLength(tTrace.ent->avelocity) > 0.0)
						||  ((tTrace.ent->moveinfo.state != STATE_BOTTOM) && (tTrace.ent->moveinfo.state != STATE_TOP)) )
						{
							if( self->last_door_time < level.framenum )
								self->last_door_time = level.framenum;
							if( ACEMV_CanMove( self, MOVE_BACK ) )
								ucmd->forwardmove = -SPEED_WALK;
							return;
						}
						else
						{
							// Open it
							if( (self->last_door_time < level.framenum - 2.0 * HZ) &&
								(tTrace.ent->moveinfo.state != STATE_TOP) &&
								(tTrace.ent->moveinfo.state != STATE_UP) )
							{
								Cmd_OpenDoor_f ( self );	// Open the door
								self->last_door_time = level.framenum + random() * 2.5 * HZ; // wait!
								ucmd->forwardmove = 0;
								return;
							}
						}
					}
					else if( tTrace.ent->client )
					{
						// Back up slowly - may have a teammate in the way
						if( self->last_door_time < level.framenum )
							self->last_door_time = level.framenum;
						if( ACEMV_CanMove( self, MOVE_BACK ) )
							ucmd->forwardmove = -SPEED_WALK;
						return;
					}
				}
			}
		}

		// Set bot's movement direction
		VectorSubtract (self->movetarget->s.origin, self->s.origin, self->move_vector);
		ACEMV_ChangeBotAngle(self);
		ucmd->forwardmove = SPEED_RUN;
		return;
	}
}

///////////////////////////////////////////////////////////////////////
// Main movement code. (following node path)
///////////////////////////////////////////////////////////////////////
void ACEMV_Move(edict_t *self, usercmd_t *ucmd)
{
	vec3_t dist;
	int current_node_type=-1;
	int next_node_type=-1;
	int i;
	float	distance;

	// Do not follow path when teammates are still inside us.
	if( OnTransparentList(self) )
	{
		self->state = STATE_WANDER;
		self->wander_timeout = level.framenum + (random() + 0.5) * HZ;
		return;
	}

	// Get current and next node back from nav code.
	if(!ACEND_FollowPath(self))
	{
		if( 
			( !teamplay->value ) ||
			(teamplay->value && level.framenum >= (self->teamPauseTime))
			)
		{
			self->state = STATE_WANDER;
			self->wander_timeout = level.framenum + 1.0 * HZ;
		}
		else
		{
			// Teamplay mode - just fan out and chill
			if (self->state == STATE_FLEE)
			{
				self->state = STATE_POSITION;
				self->wander_timeout = level.framenum + 3.0 * HZ;
			}
			else
			{
				self->state = STATE_POSITION;
				self->wander_timeout = level.framenum + 1.0 * HZ;
			}
		}
		self->goal_node = INVALID;
		return;
	}

	current_node_type = nodes[self->current_node].type;
	next_node_type = nodes[self->next_node].type;
		
	///////////////////////////
	// Move To Goal
	///////////////////////////
	if (self->movetarget)
		ACEMV_MoveToGoal(self,ucmd);

	else if( (self->next_node != self->current_node) && (current_node_type != NODE_LADDER) )
	{
		// Decide if the bot should strafe to stay on course.
		vec3_t v = {0,0,0};
		VectorSubtract( nodes[self->next_node].origin, nodes[self->current_node].origin, v );
		v[2] = 0;
		VectorSubtract( self->s.origin, nodes[self->next_node].origin, dist );
		dist[2] = 0;
		if( (DotProduct( v, dist ) > 0) && (VectorLength(v) > 16) )
		{
			float left = 0;
			VectorNormalize( v );
			VectorRotate2( v, 90 );
			left = DotProduct( v, dist );
			if( (left > 16) && (! self->groundentity || ACEMV_CanMove( self, MOVE_RIGHT )) )
				ucmd->sidemove = SPEED_RUN;
			else if( (left < -16) && (! self->groundentity || ACEMV_CanMove( self, MOVE_LEFT )) )
				ucmd->sidemove = -SPEED_RUN;
		}
	}

	if(current_node_type == NODE_GRAPPLE)
	{
		if( self->last_door_time < level.framenum )
		{
			ucmd->forwardmove = SPEED_WALK; //walk forwards a little
//			Cmd_OpenDoor_f ( self );	// Open the door
			self->last_door_time = level.framenum + random() * 2.0 * HZ; // wait!
		}

	}
		
/*	////////////////////////////////////////////////////////
	// Grapple
	///////////////////////////////////////////////////////
	if(next_node_type == NODE_GRAPPLE)
	{
		ACEMV_ChangeBotAngle(self);
		ACEIT_ChangeWeapon(self,FindItem("grapple"));	
		ucmd->buttons = BUTTON_ATTACK;
		return;
	}
	// Reset the grapple if hangin on a graple node
	if(current_node_type == NODE_GRAPPLE)
	{
		CTFPlayerResetGrapple(self);
		return;
	}*/
	////////////////////////////////////////////////////////
	// Doors - RiEvEr
	// modified by Werewolf
	///////////////////////////////////////////////////////
	if(current_node_type == NODE_DOOR)
	{
		// We are trying to go through a door
		trace_t	tTrace;
		vec3_t	vStart, vDest;

		if( next_node_type == NODE_DOOR )
			VectorCopy( nodes[self->current_node].origin, vStart );
		else
			VectorCopy( self->s.origin, vStart );

		VectorCopy( nodes[self->next_node].origin, vDest );
		VectorSubtract( nodes[self->next_node].origin, self->s.origin, dist );
		dist[2] = 0;
		distance = VectorLength(dist);

		// See if we have a clear shot at it
		PRETRACE();
		tTrace = gi.trace( vStart, tv(-16,-16,-8), tv(16,16,8), vDest, self, MASK_PLAYERSOLID);
		POSTTRACE();

		if( (tTrace.fraction < 1.0) && (distance < 512) )
		{
			if( (strcmp( tTrace.ent->classname, "func_door_rotating" ) == 0)
			||  (strcmp( tTrace.ent->classname, "func_door") == 0) )
			{
				// The door is in the way
				// See if it's moving
				if( (VectorLength(tTrace.ent->avelocity) > 0.0)
				||  ((tTrace.ent->moveinfo.state != STATE_BOTTOM) && (tTrace.ent->moveinfo.state != STATE_TOP)) )
				{
					if( self->last_door_time < level.framenum )
						self->last_door_time = level.framenum;
					if( ACEMV_CanMove( self, MOVE_BACK ) )
						ucmd->forwardmove = -SPEED_WALK; //walk backwards a little
					else if( self->tries && self->groundentity )
						ucmd->upmove = SPEED_RUN;
					return;
				}
				else
				{
					// Open it
					if( (self->last_door_time < level.framenum - 2.0 * HZ) &&
						(tTrace.ent->moveinfo.state != STATE_TOP) &&
						(tTrace.ent->moveinfo.state != STATE_UP) )
					{
						Cmd_OpenDoor_f ( self );	// Open the door
						self->last_door_time = level.framenum + random() * 2.5 * HZ; // wait!
						if( self->tries && self->groundentity )
							ucmd->upmove = SPEED_RUN;
						ucmd->forwardmove = 0;
						return;
					}
				}
			}
			else if( tTrace.ent->client )
			{
				// Back up slowly - may have a teammate in the way
				if( self->last_door_time < level.framenum )
					self->last_door_time = level.framenum;
				if( ACEMV_CanMove( self, MOVE_BACK ) )
					ucmd->forwardmove = -SPEED_WALK;
				else if( self->tries && self->groundentity )
					ucmd->upmove = SPEED_RUN;
				return;
			}
		}
	}
	
	////////////////////////////////////////////////////////
	// Platforms
	///////////////////////////////////////////////////////
	if( (next_node_type == NODE_PLATFORM) && ((current_node_type != NODE_PLATFORM) || (self->current_node == self->next_node)) )
	{
		// Check if the platform is directly above us.
		vec3_t start, above;
		trace_t tr;
		VectorCopy( self->s.origin, start );
		VectorCopy( self->s.origin, above );
		start[2] += 32;
		above[2] += 2048;
		tr = gi.trace( start, tv(-24,-24,-8), tv(24,24,8), above, self, MASK_PLAYERSOLID );
		if( (tr.fraction < 1.0) && tr.ent && (tr.ent->use == Use_Plat) )
		{
			// If it is directly above us, get out of the way.
			ucmd->sidemove = 0;
			if( ACEMV_CanMove( self, MOVE_BACK ) )
				ucmd->forwardmove = -SPEED_WALK;
			else if( (self->bot_strafe <= 0) && ACEMV_CanMove( self, MOVE_LEFT ) )
				ucmd->sidemove = -SPEED_WALK;
			else if( ACEMV_CanMove( self, MOVE_RIGHT ) )
				ucmd->sidemove = SPEED_WALK;
			self->bot_strafe = ucmd->sidemove;
			ACEMV_ChangeBotAngle(self);
			return;
		}
	}
	if(current_node_type != NODE_PLATFORM && next_node_type == NODE_PLATFORM)
	{
		// check to see if lift is down?
		for(i=0;i<num_items;i++)
			if(item_table[i].node == self->next_node)
				if(item_table[i].ent->moveinfo.state != STATE_BOTTOM)
				    return; // Wait for elevator
	}
	if( (current_node_type == NODE_PLATFORM) && self->groundentity
	&&  ((self->groundentity->moveinfo.state == STATE_UP) || (self->groundentity->moveinfo.state == STATE_DOWN)) )
	{
		// Standing on moving elevator.
		ucmd->forwardmove = 0;
		ucmd->sidemove = 0;
		ucmd->upmove = 0;
		if( VectorLength(self->groundentity->velocity) < 8 )
			ucmd->upmove = SPEED_RUN;
		ACEMV_ChangeBotAngle(self);
		if( debug_mode && (level.framenum % HZ == 0) )
			debug_printf( "%s: platform move state %i\n", self->client->pers.netname, self->groundentity->moveinfo.state );
		return;
	}
	if( (current_node_type == NODE_PLATFORM) && (next_node_type == NODE_PLATFORM) && (self->current_node != self->next_node) )
	{
		// Move to the center
		self->move_vector[2] = 0; // kill z movement	
		if(VectorLength(self->move_vector) > 10)
			ucmd->forwardmove = SPEED_WALK; // walk to center
				
		ACEMV_ChangeBotAngle(self);
		
		return; // No move, riding elevator
	}

	////////////////////////////////////////////////////////
	// Jumpto Nodes
	///////////////////////////////////////////////////////
	if(next_node_type == NODE_JUMP)
//		||
//			( current_node_type == NODE_JUMP 
//			&& next_node_type != NODE_ITEM && nodes[self->next_node].origin[2] > self->s.origin[2]+16)
//	  )
	{
		VectorSubtract( nodes[self->next_node].origin, self->s.origin, dist);
		// Lose the vertical component
		dist[2] = 0;
		// Get the absolute length
		distance = VectorLength(dist);
	
		//if (ACEMV_CanJumpInternal(self, MOVE_FORWARD))
		{
			// Jump only when we are moving the correct direction.
			vec3_t velocity;
			VectorCopy( self->velocity, velocity );
			velocity[2] = 0;
			VectorNormalize( velocity );
			VectorNormalize( dist );
			if( DotProduct( velocity, dist ) > 0.8 )
				ucmd->upmove = SPEED_RUN;

			//Kill running movement
//			self->move_vector[0]=0;
//			self->move_vector[1]=0;
//			self->move_vector[2]=0;
			// Set up a jump move
			if( distance < 256 )
				ucmd->forwardmove = SPEED_RUN * sqrtf( distance / 256 );
			else
				ucmd->forwardmove = SPEED_RUN;

			//self->move_vector[2] *= 2;

			ACEMV_ChangeBotAngle(self);

			/*		
			// RiEvEr - re-instate the velocity hack
			if (self->jumphack_timeout < level.framenum)
			{
				VectorCopy(self->move_vector,dist);
				if ((440 * distance / 128) < 440)
					VectorScale(dist,(440 * distance / 128),self->velocity);
				else
					VectorScale(dist,440,self->velocity);
				self->jumphack_timeout = level.framenum + 3.0 * HZ;
			}
			*/
		}
		/*
		else
		{
				self->goal_node = INVALID;
		}
		*/
		return;
	}
	
	////////////////////////////////////////////////////////
	// Ladder Nodes
	///////////////////////////////////////////////////////

	// Find the distance vector to the next node
	VectorSubtract( nodes[self->next_node].origin, self->s.origin, dist);
	// Lose the vertical component
	dist[2] = 0;
	// Get the absolute length
	distance = VectorLength(dist);

	// If on the ladder
	if( (next_node_type == NODE_LADDER) && (distance < 64)
	&&  ((nodes[self->next_node].origin[2] > self->s.origin[2]) || ! self->groundentity) )
	{
		// FIXME: Dirty hack so the bots can actually use ladders.
		VectorSubtract( nodes[self->next_node].origin, self->s.origin, dist );
		VectorNormalize( dist );
		VectorScale( dist, SPEED_RUN, self->velocity );
		if( dist[2] >= 0 )
			self->velocity[2] = min( 200, self->velocity[2] );
		else
			self->velocity[2] = max( -200, self->velocity[2] );
		
		if( self->velocity[2] < 0 )
		{
			// If we are going down the ladder, check for teammates coming up and yield to them.
			trace_t	tr;
			tr = gi.trace( self->s.origin, tv(-16,-16,-8), tv(16,16,8), nodes[self->next_node].origin, self, MASK_PLAYERSOLID );
			if( (tr.fraction < 1.0) && tr.ent && (tr.ent != self) && tr.ent->client && (tr.ent->velocity[2] >= 0) )
			{
				self->velocity[0] = 0;
				self->velocity[1] = 0;
				self->velocity[2] = 200;
			}
		}

		ACEMV_ChangeBotAngle(self);
		return;
	}
	
	// If getting onto the ladder going up
	if( (next_node_type == NODE_LADDER) && (distance < 200)
	&&  (nodes[self->next_node].origin[2] >= self->s.origin[2]) )
	{
		// Jump only when we are moving the correct direction.
		vec3_t velocity;
		VectorCopy( self->velocity, velocity );
		velocity[2] = 0;
		VectorNormalize( velocity );
		VectorNormalize( dist );
		if( DotProduct( velocity, dist ) > 0.8 )
			ucmd->upmove = SPEED_RUN;

		ucmd->forwardmove = SPEED_WALK / 2;
		
		ACEMV_ChangeBotAngle(self);
		return;
	}
	
	// If getting off the ladder
	if( (current_node_type == NODE_LADDER) && (next_node_type != NODE_LADDER) )
	{
		ucmd->forwardmove = SPEED_WALK;

		if( (nodes[self->next_node].origin[2] >= self->s.origin[2])
		&&  (nodes[self->next_node].origin[2] >= nodes[self->current_node].origin[2])
		&&  ( ((self->velocity[2] > 0) && ! self->groundentity) || OnLadder(self) ) )
		{
			ucmd->upmove = SPEED_RUN;
			self->velocity[2] = min( 200, distance * 10 ); // Jump higher for farther node
		}
		
		ACEMV_ChangeBotAngle(self);
		return;
	}
	
	// If getting onto the ladder going down
	if( (current_node_type != NODE_LADDER) && (next_node_type == NODE_LADDER) )
	{
		ucmd->forwardmove = SPEED_WALK / 2;

		if( (distance < 200) && (nodes[self->next_node].origin[2] <= self->s.origin[2] + 16) )
			ucmd->upmove = -SPEED_RUN; //Added by Werewolf to cause crouching
		
		ACEMV_ChangeBotAngle(self);
		return;
	}

/*	//==============================
	// LEDGES etc..
	// If trying to jump up a ledge
	if(
	   (current_node_type == NODE_MOVE &&
	   nodes[self->next_node].origin[2] > self->s.origin[2]+16 && distance < NODE_DENSITY)
	   )
	{
		ucmd->forwardmove = SPEED_RUN;
		ucmd->upmove = SPEED_RUN;
		ACEMV_ChangeBotAngle(self);
		return;
	}*/
	////////////////////////////////////////////////////////
	// Water Nodes
	///////////////////////////////////////////////////////
	if(current_node_type == NODE_WATER)
	{
		// We need to be pointed up/down
		ACEMV_ChangeBotAngle(self);

		// If the next node is not in the water, then move up to get out.
		if(next_node_type != NODE_WATER && !(gi.pointcontents(nodes[self->next_node].origin) & MASK_WATER)) // Exit water
			ucmd->upmove = SPEED_RUN;
		
		ucmd->forwardmove = SPEED_RUN * 3 / 4;
		return;

	}
	
	// Falling off ledge?
/*	if(!self->groundentity)
	{
		ACEMV_ChangeBotAngle(self);

		self->velocity[0] = self->move_vector[0] * 360;
		self->velocity[1] = self->move_vector[1] * 360;
	
		return;
	}*/
		
	// Check to see if stuck, and if so try to free us
	// Also handles crouching
	VectorSubtract( self->s.origin, self->lastPosition, dist );
	if( (VectorLength(self->velocity) < 37) || (VectorLength(dist) < FRAMETIME) )
	{
		// Keep a random factor just in case....
		if(random() > 0.5 && ACEMV_SpecialMove(self, ucmd))
			return;
		
		if( self->groundentity )
		{
			// Check if we are obstructed by an oncoming teammate, and if so strafe right.
			trace_t	tr;
			tr = gi.trace( self->s.origin, tv(-16,-16,-8), tv(16,16,8), nodes[self->next_node].origin, self, MASK_PLAYERSOLID );
			if( (tr.fraction < 1.0) && tr.ent && (tr.ent != self) && tr.ent->client
			&&  (DotProduct( self->velocity, tr.ent->velocity ) <= 0) && ACEMV_CanMove( self, MOVE_RIGHT ) )
			{
				ucmd->sidemove = SPEED_RUN;
				ACEMV_ChangeBotAngle(self);
				return;
			}
		}

		self->s.angles[YAW] += random() * 180 - 90; 

		ACEMV_ChangeBotAngle(self);

		ucmd->forwardmove = SPEED_RUN;

		// Raptor007: Only jump when trying to gain altitude, not down ledges.
		if( nodes[self->next_node].origin[2] > self->s.origin[2] + 16 )
			ucmd->upmove = SPEED_RUN;
		
		return;
	}

	////////////////////////////////////////////////////////
	// Walk Nodes
	///////////////////////////////////////////////////////
	ACEMV_ChangeBotAngle(self);

	VectorSubtract( nodes[self->next_node].origin, self->s.origin, dist );
	dist[2] = 0;
	distance = VectorLength(dist);

	if( ! self->groundentity )
	{
		// When in air, control speed carefully to avoid overshooting the next node.
		if( distance < 256 )
			ucmd->forwardmove = SPEED_RUN * sqrtf( distance / 256 );
		else
			ucmd->forwardmove = SPEED_RUN;
	}
	else if( ! ACEMV_CanMove( self, MOVE_FORWARD ) )
	{
		// If we reached a ledge, slow down.
		if( distance < 512 )
		{
			// Jump unless dropping down to the next node.
			if( (nodes[self->next_node].origin[2] > self->s.origin[2] + 7) && (distance < 500) )
				ucmd->upmove = SPEED_RUN;

			ucmd->forwardmove = SPEED_WALK * sqrtf( distance / 512 );
		}
		else
			ucmd->forwardmove = SPEED_WALK;
	}
	else
		// Otherwise move as fast as we can.
		ucmd->forwardmove = SPEED_RUN;
}


///////////////////////////////////////////////////////////////////////
// Wandering code (based on old ACE movement code) 
///////////////////////////////////////////////////////////////////////
//
// RiEvEr - this routine is of a very poor standard and has a LOT of problems
// Maybe replace this with the DroneBot or ReDeMpTiOn wander code?
//
void ACEMV_Wander(edict_t *self, usercmd_t *ucmd)
{
	vec3_t  temp;
	int content_head = 0, content_feet = 0;
	float moved = 0;
	
	// Do not move
	if(self->next_move_time > level.framenum)
		return;

	// Special check for elevators, stand still until the ride comes to a complete stop.
	if( self->groundentity && (VectorLength(self->groundentity->velocity) >= 8) )
		if(self->groundentity->moveinfo.state == STATE_UP ||
		   self->groundentity->moveinfo.state == STATE_DOWN) // only move when platform not
		{
			self->velocity[0] = 0;
			self->velocity[1] = 0;
			self->velocity[2] = 0;
			self->next_move_time = level.framenum + 0.5 * HZ;
			return;
		}
	
	
	// Is there a target to move to
	if (self->movetarget)
		ACEMV_MoveToGoal(self,ucmd);
		
	VectorSubtract( self->s.origin, self->lastPosition, temp );
	moved = VectorLength(temp);

	////////////////////////////////
	// Ladder?
	////////////////////////////////
	// To avoid unnecessary falls, don't use ladders when on solid ground.
	if( (! self->groundentity) && OnLadder(self) )
	{
		ucmd->upmove = SPEED_RUN;
		ucmd->forwardmove = 0;
		ucmd->sidemove = 0;
		self->s.angles[PITCH] = -15;

		// FIXME: Dirty hack so the bots can actually use ladders.
		self->velocity[0] = 0;
		self->velocity[1] = 0;
		self->velocity[2] = 200;

		if( moved >= FRAMETIME )
			return;
	}

	////////////////////////////////
	// Swimming?
	////////////////////////////////
	VectorCopy(self->s.origin,temp);
	temp[2] += 22;
	content_head = gi.pointcontents(temp);
	temp[2] = self->s.origin[2] - 8;
	content_feet = gi.pointcontents(temp);

	// Just try to keep our head above water.
	if( content_head & MASK_WATER )
	{
		// If drowning and no node, move up
		if(self->client->next_drown_framenum > 0)
		{
			ucmd->upmove = SPEED_RUN;
			self->s.angles[PITCH] = -45;
		}
		else
			ucmd->upmove = SPEED_WALK;
	}

	// Don't wade in lava, try to get out!
	if( content_feet & (CONTENTS_LAVA|CONTENTS_SLIME) )
		ucmd->upmove = SPEED_RUN;

	// See if we're jumping out of the water.
	if( ! self->groundentity
	&& (content_feet & MASK_WATER)
	&& ACEMV_SpecialMove( self, ucmd ) )
	{
		if( ucmd->upmove > 0 )
			self->velocity[2] = 270;  // FIXME: Is there a cleaner way?
		if( moved >= FRAMETIME )
			return;
	}

//@@	if(ACEMV_CheckEyes(self,ucmd))
//		return;
	
	// Check for special movement if we have a normal move (have to test)
	if( (VectorLength(self->velocity) < 37) || (moved < FRAMETIME) )
	{
		if(random() > 0.1 && ACEMV_SpecialMove(self,ucmd))
			return;

		self->s.angles[YAW] += random() * 180 - 90; 
		self->s.angles[PITCH] = 0;

		if( content_feet & MASK_WATER )  // Just keep swimming.
			ucmd->forwardmove = SPEED_RUN;
		else if(!M_CheckBottom(self) && !self->groundentity) // if there is ground continue otherwise wait for next move
			ucmd->forwardmove = 0;
		else if( ACEMV_CanMove( self, MOVE_FORWARD))
			ucmd->forwardmove = SPEED_WALK;
		
		return;
	}
	
	// Otherwise move as fast as we can
	// If it's safe to move forward (I can't believe ACE didn't check this!
	if( ACEMV_CanMove( self, MOVE_FORWARD )
	|| (content_feet & MASK_WATER) )
	{
		ucmd->forwardmove = SPEED_RUN;
	}
	else
	{
		// Need a "findbestdirection" routine in here
		// Forget about this route!
		ucmd->forwardmove = -SPEED_WALK / 2;
		self->movetarget = NULL;
	}	

}

///////////////////////////////////////////////////////////////////////
// Attack movement routine
//
// NOTE: Very simple for now, just a basic move about avoidance.
//       Change this routine for more advanced attack movement.
///////////////////////////////////////////////////////////////////////
void ACEMV_Attack (edict_t *self, usercmd_t *ucmd)
{
	float c;
	vec3_t  target;
	vec3_t	attackvector;
	float	dist;
	qboolean	bHasWeapon;	// Needed to allow knife throwing and kick attacks
	qboolean	enemy_front;

	bHasWeapon = self->grenadewait || ACEAI_ChooseWeapon(self);

	// Check distance to enemy
	VectorSubtract( self->s.origin, self->enemy->s.origin, attackvector);
	dist = VectorLength( attackvector);

	enemy_front = infront( self, self->enemy );

  //If we're fleeing, don't bother moving randomly around the enemy and stuff...
//  if (self->state != STATE_FLEE)
  {

//AQ2 CHANGE
	// Don't stand around if all you have is a knife or no weapon.
	if( 
		(self->client->weapon == FindItem(KNIFE_NAME)) 
		|| !bHasWeapon // kick attack
		)
	{
		// Don't walk off the edge
		if( ACEMV_CanMove( self, MOVE_FORWARD))
		{
			ucmd->forwardmove += SPEED_RUN;
		}
		else
		{
			// Can't get there!
			ucmd->forwardmove = -SPEED_WALK;
			self->enemy=NULL;
			self->state = STATE_WANDER;
			return;
		}

		// Raptor007: Attempt longer knife throws when carrying many knives.
		self->client->pers.knife_mode = 0;
		if( (dist < 2000) && (dist < 400 + 300 * INV_AMMO(self,KNIFE_NUM)) )
		{
			float kick_dist = 200;

			if( bHasWeapon )
			{
				self->client->pers.knife_mode = 1;
				kick_dist = 100;
			}

			if( dist < kick_dist )
			{
				if( dist < 64 )	// Too close
					ucmd->forwardmove = -SPEED_WALK;
				// Kick Attack needed!
				ucmd->upmove = SPEED_RUN;
			}
		}
	}
	else //if(!(self->client->weapon == FindItem(SNIPER_NAME))) // Stop moving with sniper rifle
	{
		// Randomly choose a movement direction
		c = random();		
		if(c < 0.2 && ACEMV_CanMove(self,MOVE_LEFT))
			ucmd->sidemove -= SPEED_RUN; 
		else if(c < 0.4 && ACEMV_CanMove(self,MOVE_RIGHT))
			ucmd->sidemove += SPEED_RUN;

		c = random();
		if(c < 0.6 && ACEMV_CanMove(self,MOVE_FORWARD))
			ucmd->forwardmove += SPEED_RUN;
		else if(c < 0.8 && ACEMV_CanMove(self,MOVE_FORWARD))
			ucmd->forwardmove -= SPEED_RUN;
	}
//AQ2 END
	if( (dist < 600) && 
		( !(self->client->weapon == FindItem(KNIFE_NAME)) 
)//		&& !(self->client->weapon == FindItem(SNIPER_NAME))) // Stop jumping with sniper rifle
		&&( bHasWeapon )	// Jump already set for kick attack
		)
	{
		// Randomly choose a vertical movement direction
		c = random();

		if(c < 0.10) //Werewolf: was 0.15
		{
			if (ACEMV_CanJump(self))
				ucmd->upmove += SPEED_RUN;
		}
		else if( c> 0.85 )	// Only crouch sometimes
			ucmd->upmove -= SPEED_WALK;
	}

  }	//The rest applies even for fleeing bots

	// Werewolf: Crouch if no laser light
	if( (ltk_skill->value >= 3)
	&& self->groundentity
	&& ! INV_AMMO( self, LASER_NUM )
	&& (  (self->client->m4_rds   && (self->client->weapon == FindItem(M4_NAME)))
	   || (self->client->mp5_rds  && (self->client->weapon == FindItem(MP5_NAME)))
	   || (self->client->mk23_rds && (self->client->weapon == FindItem(MK23_NAME)))
	   || (self->client->dual_rds && (self->client->weapon == FindItem(DUAL_NAME))) ) )
	{
		// Raptor007: Don't crouch if it blocks the shot.
		float old_z = self->s.origin[2];
		self->s.origin[2] -= 14;
		if( ACEAI_CheckShot(self) )
			ucmd->upmove = -SPEED_RUN;
		self->s.origin[2] = old_z;
	}

	// Set the attack 
	//@@ Check this doesn't break grenades!
	//Werewolf: I've checked that. Now it doesn't anymore. Behold my power! Muhahahahaha!!!
	if((self->client->weaponstate == WEAPON_READY)||(self->client->weaponstate == WEAPON_FIRING))
	{
		// Only shoot if the weapon is ready and we can hit the target!
		//@@ Removed to try to help RobbieBoy!
		//Reenabled by Werewolf
		if( ACEAI_CheckShot( self ))
		{
			if( (ltk_skill->value >= 0)
			&&  (self->react >= 1.f / (ltk_skill->value + 2.f))             // Reaction time.
			&&  enemy_front
			&&  (FRAMESYNC || (self->client->weaponstate != WEAPON_READY))  // Sync firing with random offsets.
			&&  (self->teamPauseTime == level.framenum - 1) )               // Saw them last frame too.
			{
				ucmd->buttons = BUTTON_ATTACK;

				if(self->client->weapon == FindItem(GRENADE_NAME))
				{
					self->grenadewait = level.framenum + 1.5 * HZ;
					ucmd->forwardmove = -SPEED_RUN; //Stalk back, behold of the holy Grenade!
				}
				else
					self->grenadewait = 0;
			}
		}
		else if (self->state != STATE_FLEE)
		{
			if (ucmd->upmove == -SPEED_RUN)
				ucmd->upmove = 0;
			if (ltk_jumpy->value)
			{
				c = random();
				if(c < 0.50)	//Only jump at 50% probability
				{
					if (ACEMV_CanJump(self))
					ucmd->upmove = SPEED_RUN;
				}
				else if (c < 0.75 && ACEMV_CanMove(self,MOVE_LEFT))
					ucmd->sidemove -= SPEED_WALK;
				else if (ACEMV_CanMove(self,MOVE_RIGHT))
					ucmd->sidemove += SPEED_WALK;
			}
		}
	}

	// Raptor007: Don't immediately shoot friendlies after round.
	if( (team_round_countdown > 40) && ! self->grenadewait )
	{
		if( self->client->weapon == FindItem(KNIFE_NAME) )
			self->client->pers.knife_mode = 1;  // Throwing knives are honorable.
		else if( self->client->weapon != FindItem(HC_NAME) )  // Handcannon is allowed.
			ucmd->buttons &= ~BUTTON_ATTACK;

		if( dist < 128 )
		{
			if( self->groundentity && (self->s.origin <= self->enemy->s.origin) )
				ucmd->upmove = SPEED_RUN;  // Kicking is the most honorable form of combat.
		}
		else
		{
			ucmd->upmove = 0;
			if( ACEMV_CanMove( self, MOVE_FORWARD ) )
				ucmd->forwardmove = SPEED_RUN;  // We need to get closer to kick.
		}
	}

	// Aim
	VectorCopy(self->enemy->s.origin,target);

	// Werewolf: Aim higher if using grenades
	if(self->client->weapon == FindItem(GRENADE_NAME))
		target[2] += 35;

	//AQ2 ADD - RiEvEr
	// Alter aiming based on skill level
	// Modified by Werewolf and Raptor007
	if( 
		( (ltk_skill->value >= 0) && (ltk_skill->value < 10) )
		&& ( bHasWeapon )	// Kick attacks must be accurate
		&& (!(self->client->weapon == FindItem(KNIFE_NAME))) // Knives accurate
		&& (!(self->client->weapon == FindItem(GRENADE_NAME))) // Grenades accurate
		)
	{
		short int sign[3], iFactor = 7;
		sign[0] = (random() < 0.5) ? -1 : 1;
		sign[1] = (random() < 0.5) ? -1 : 1;
		sign[2] = (random() < 0.5) ? -1 : 1;

		// Not that complex. We miss by 0 to 80 units based on skill value and random factor
		// Unless we have a sniper rifle!
		if(self->client->weapon == FindItem(SNIPER_NAME))
			iFactor = 5;

		// Shoot less accurately if we just got hit.
		if( self->client->push_timeout > 45 )
			iFactor += self->client->push_timeout - 45;

		// Shoot more accurately after holding aim, but less accurately when first aiming.
		if( self->react >= 4.f )
			iFactor --;
		else if( (self->react < 0.5f) && (dist > 300.f) )
			iFactor ++;

		target[0] += sign[0] * (10 - ltk_skill->value + ( (  iFactor*(10 - ltk_skill->value)  ) * random() )) * 0.7f;
		target[1] += sign[1] * (10 - ltk_skill->value + ( (  iFactor*(10 - ltk_skill->value)  ) * random() )) * 0.7f;
		target[2] += sign[2] * (10 - ltk_skill->value + ( (  iFactor*(10 - ltk_skill->value)  ) * random() ));
	}
	//Werewolf: Snipers of skill 10 are complete lethal, so I don't use that code down there
/*	else if (ltk_skill->value == 11)
	if(self->client->weapon == FindItem(SNIPER_NAME))
	{
		short int	up, right, iFactor=1;
		up = (random() < 0.5)? -1 :1;
		right = (random() < 0.5)? -1 : 1;
		target[0] += ( right * (10 - 5 +((iFactor*(10 - 5)) *random())) );
		target[2] += ( up * (10 - 5 +((iFactor*(10 - 5)) *random())) );
	}
*/
//AQ2 END

	// Werewolf: release trigger after 1 second for grenades
	if( (self->grenadewait == level.framenum + 1 * HZ) && (self->solid != SOLID_NOT) )
	{
		ucmd->buttons = 0;
	}

	//Werewolf: Wait for grenade to launch before facing elsewhere
	if( level.framenum >= self->grenadewait )
	{
		self->grenadewait = 0;

		// Raptor007: Interpolate angle changes, and set new desired direction at 10fps.
		// Make sure imperfect angles are calculated when first spotting an enemy, too.
		ACEMV_ChangeBotAngle( self );
		if( FRAMESYNC || ! enemy_front || (level.framenum - self->teamPauseTime >= FRAMEDIV) )
			VectorSubtract( target, self->s.origin, self->move_vector );
	}

	// Store time we last saw an enemy
	// This value is used to decide if we initiate a long range search or not.
	self->teamPauseTime = level.framenum;
	
//	if(debug_mode)
//		debug_printf("%s attacking %s\n",self->client->pers.netname,self->enemy->client->pers.netname);
}

//==========================
// AntPathMove
//==========================
//
qboolean	AntPathMove( edict_t *self )
{
	//node_t *temp = &nodes[self->current_node];	// For checking our position

	//if( level.time == (float)((int)level.time) )
	if( level.framenum % HZ == 0 )
	{
		if( 
			!AntLinkExists( self->current_node, SLLfront(&self->pathList) )
			&& ( self->current_node != SLLfront(&self->pathList) )
			)
		{
			// We are off the path - clear out the lists
			AntInitSearch( self );
		}
	}

	// Boot in our new pathing algorithm
	// This will fill ai.pathList with the information we need
	if( 
		SLLempty(&self->pathList)				// We have no path and
		&& (self->current_node != self->goal_node)	// we're not at our destination
		)
	{
		if( !AntStartSearch( self, self->current_node, self->goal_node))	// Set up our pathList
		{
			// Failed to find a path
//			gi.bprintf(PRINT_HIGH,"%s: Target at(%i) - No Path \n",
//				self->client->pers.netname, self->goal_node, self->next_node);
			return false;
		}
		return true;
	}

	// And change our pathfinding to use it
	if ( !SLLempty(&self->pathList) )								// We have a path 
	{
//		if( AtNextNode( self->s.origin, SLLfront(&self->pathList))	// we're at the nextnode
		if( ACEND_FindClosestReachableNode(self,NODE_DENSITY*0.66,NODE_ALL) == SLLfront(&self->pathList)	// we're at the nextnode
			|| self->current_node == SLLfront(&self->pathList) 
		)
		{
			self->current_node = SLLfront(&self->pathList);		// Show we're there
			SLLpop_front(&self->pathList);				// Remove the top node
		}
		if( !SLLempty(&self->pathList) )
			self->next_node = SLLfront(&self->pathList);	// Set our next destination
		else
			self->next_node = INVALID;				// We're at the target location
		return true;
	}
	return true;	// Pathlist is emptyand we are at our destination
}

