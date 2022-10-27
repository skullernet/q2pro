//---------------------------------------------
// bot_combat.c
//
// Copyright (c) 2000 Connor Caple all rights reserved
//
// This file contains the bot combat and aiming code
//---------------------------------------------
/*
 * $Log: /LTK2/src/acesrc/bot_combat.c $
 * 
 * 2     21/02/00 15:16 Riever
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 */

#include "../g_local.h"

//-----------------------------------------------
// BOTCOM_Aim
//
// Sets angles to face ae entity
//-----------------------------------------------

void BOTCOM_Aim (edict_t *bot, edict_t *target, vec3_t angles)
{
    vec3_t vDir, vStart, vEnd;
    
    VectorCopy(target->s.origin, vEnd);
    if (target->client)
    {
        if (target->client->ps.pmove.pm_flags & PMF_DUCKED)
        {
            // Aim lower.....
             vEnd[2] -= 18;
        }
    }
    VectorCopy(bot->s.origin, vStart);
    VectorSubtract(vEnd, vStart, vDir);
    vectoangles(vDir, angles);
}

//-----------------------------------------------
// BOTCOM_AimAt
//
// Sets angles to face a point in space
//-----------------------------------------------

void BOTCOM_AimAt (edict_t *bot, vec3_t target, vec3_t angles)
{
    vec3_t vDir, vStart;

	if( level.framenum > bot->grenadewait )
	{
		bot->grenadewait = 0;
		VectorCopy(bot->s.origin, vStart);
		VectorSubtract(target, vStart, vDir);
		VectorNormalize (vDir);
		vectoangles(vDir, angles);
	}
}

//----------------------------------------------
// BOTCOM_BasicAttack
//
// Simple attack function just to get the bots fighting
//----------------------------------------------

void BOTCOM_BasicAttack (edict_t *bot, usercmd_t *cmd, vec3_t vTarget)
{
	float fRandValue;
	vec3_t	vAttackVector;
	float	vDist;
	qboolean	bHasWeapon;	// Needed to allow knife throwing and kick attacks

	bHasWeapon = BOTWP_ChooseWeapon(bot);

	// Check distance to enemy
	VectorSubtract( bot->s.origin, bot->enemy->s.origin, vAttackVector);
	vDist = VectorLength( vAttackVector);

	// Don't stand around if all you have is a knife or no weapon.
	if( 
		(bot->client->weapon == FindItem(KNIFE_NAME)) 
		|| !bHasWeapon // kick attack
		)
	{
		// Don't walk off the edge
		if( BOTCOL_CanMoveSafely(bot, bot->s.angles))
		{
			bot->bot_speed = SPEED_RUN;
		}
		else
		{
			// Can't get there!
			bot->bot_speed = -SPEED_WALK;
			bot->enemy=NULL;
			bot->nextState = BS_ROAM;
			return;
		}

		if( vDist < 200)
		{
			// See if we want to throw the knife
			if( random() < 0.3 && bHasWeapon)
			{
				// Yes we do.
				bot->client->pers.knife_mode = 1;
			}
			else
			{
				if( vDist < 64 )	// Too close
					bot->bot_speed = -SPEED_WALK;
				// Kick Attack needed!
				cmd->upmove = 400;
			}
		}
		else
		{
			// Outside desired throwing range
			bot->client->pers.knife_mode = 0;
		}
	}
	else if(!(bot->client->weapon == FindItem(SNIPER_NAME))) // Stop moving with sniper rifle
	{
		// Randomly choose a movement direction
		fRandValue = random();	
		if(fRandValue < 0.2)
		{
			bot->bot_strafe = SPEED_WALK;
		}
		else if(fRandValue < 0.4)
		{
			bot->bot_strafe = -SPEED_WALK;
		}
		if( fRandValue < 0.4)
		{
			// Check it's safe and set up the move.
			if( BOTCOL_CanStrafeSafely(bot, bot->s.angles))
			{
				cmd->sidemove = bot->bot_strafe;
			}
		}

		fRandValue = random();
		if(fRandValue < 0.6 && BOTCOL_CanMoveSafely(bot, bot->s.angles))
			bot->bot_speed = 400; 
		else if(fRandValue < 0.8)
			bot->bot_speed = -200;
	}

	if( (vDist < 600) && 
		( !(bot->client->weapon == FindItem(KNIFE_NAME)) && 
		!(bot->client->weapon == FindItem(SNIPER_NAME))) // Stop jumping with sniper rifle
		&&( bHasWeapon )	// Jump already set for kick attack
		)
	{
		// Randomly choose a vertical movement direction
		fRandValue = random();

		if(fRandValue < 0.10)
			cmd->upmove += 200;
		else if( fRandValue> 0.90 )	// Only crouch sometimes
			cmd->upmove -= 200;
	}

//	cmd->buttons = BUTTON_ATTACK;
	// Set the attack 
	//@@ Check this doesn't break grenades!
	if(
		bHasWeapon &&
		((bot->client->weaponstate == WEAPON_READY)||(bot->client->weaponstate == WEAPON_FIRING))
		)
	{
		// Only shoot if the weapon is ready and we can hit the target!
		if( BOTCOL_CheckShot( bot ))
		{
			// Raptor007: If bot skill is negative, don't fire.
			if( ltk_skill->value >= 0 )
				cmd->buttons = BUTTON_ATTACK;
		}
		else
			cmd->upmove = 200;
	}
	
	// Aim
	VectorCopy(bot->enemy->s.origin,vTarget);

	//@@ Add AimLeading here

	// Alter aiming based on skill level
	if( 
		(ltk_skill->value < 10 )
		&& ( bHasWeapon )	// Kick attacks must be accurate
		&& (!(bot->client->weapon == FindItem(KNIFE_NAME))) // Knives accurate
		)
	{
		short int	up, right, iFactor=8;
		up = (random() < 0.5)? -1 :1;
		right = (random() < 0.5)? -1 : 1;

		// Not that complex. We miss by 0 to 80 units based on skill value and random factor
		// Unless we have a sniper rifle!
		if(bot->client->weapon == FindItem(SNIPER_NAME))
			iFactor = 1;

		// Raptor007: Only jiggle for skill >= 0 because negative skill doesn't fire.
		if( ltk_skill->value >= 0 )
		{
			vTarget[0] += ( right * (((iFactor*(10 - ltk_skill->value)) *random())) );
			vTarget[2] += ( up * (((iFactor*(10 - ltk_skill->value)) *random())) );
		}
	}

	// Check the scope use for the snipers
	if( (bot->client->curr_weap == SNIPER_NUM) && vDist > 250 )
	{
		// Use the zoom
		if( BOTWP_GetSniperMode(bot) == 0)
		{
			BOTWP_ChangeSniperMode(bot);
		}
	}
	// Store time we last saw an enemy
	// This value is used to decide if we initiate a long range search or not.
	bot->teamPauseTime = level.framenum;
	
//	if(debug_mode)
//		debug_printf("%s attacking %s\n",bot->client->pers.netname,bot->enemy->client->pers.netname);
}

