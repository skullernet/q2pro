//---------------------------------------------
// bot_states.c
//
// All bot states and reactions currently defined in here
//---------------------------------------------

#include "../g_local.h"

//**********************************************
// BS_ACTIVE
//**********************************************

// Positional sub-state
void	ActivePosition( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
}

// Collecting sub-state
void	ActiveCollect( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
}

// Attacking sub-state
void	ActiveAttack( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
	if( bot->enemy )
	{
		// Attack it in a nice way
	}
	else
	{
		// No enemy - get out of here
		bot->secondaryState = BSS_NONE;
	}
}

// Chasing Enemy sub-state
void	ActiveSeekEnemy( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
}

//-----------------------------------------------
// Main entry to the Active State
//-----------------------------------------------
void BOTST_Active( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
	// bot_speed and bot_strafe are set in here if needed

	switch( bot->secondaryState)
	{
	case BSS_POSITION:
		ActivePosition( bot, angles, cmd );
		break;
	case BSS_COLLECT:
		ActiveCollect( bot, angles, cmd );
		break;
	case BSS_SEEKENEMY:
		ActiveSeekEnemy( bot, angles, cmd );
		break;
	case BSS_ATTACK:
		ActiveAttack( bot, angles, cmd );
		break;
	case BSS_NONE:
	default:
		// Check for routing information
		// Choose what to do
		// Then do it
		break;
	}

	// Are any enemies visible?
	// Have we taken any damage that needs us to react?
	// Do we have a good enough gun?
	// Do we need more ammo?
	// Are there any interesting items lying about?
}

//**********************************************
// BS_ROAM
//**********************************************

// Attacking sub-state
void	RoamingAttack( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
	vec3_t	vTarget;

	if( bot->enemy && 
		(bot->enemy->solid != SOLID_NOT) &&
		BOTCOL_Visible( bot, bot->enemy)
		)
	{
		BOTCOM_BasicAttack (bot, cmd, vTarget);
		// Aim at returned vector
		BOTCOM_AimAt( bot, vTarget, angles );
		if(debug_mode)
			BOTUT_TempLaser( bot->s.origin, vTarget);
	}
	else
	{
		// No visible enemy - get out of here
		bot->enemy = NULL;
		bot->secondaryState = BSS_NONE;
	}
}

// 'Normal' sub-state
void	RoamingNone( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
/*	// Check if we have routes available
	bot->current_node = ACEND_FindClosestReachableNode(bot,NODE_DENSITY, NODE_ALL);
	if( bot->current_node != INVALID)
		bot->nextState = BS_ACTIVE;*/

	// Get us out of sniper zoom mode if we were in it
	BOTWP_RemoveSniperZoomMode(bot);

	// Remember to move
	BOTMV_Roaming( bot, angles, cmd );

	// Have we taken any damage that needs us to react?
	if( BOTAI_NeedToBandage(bot))
		Cmd_Bandage_f(bot);

	// Are any enemies visible? (This sets bot->enemy too)
	if( BOTAI_VisibleEnemy(bot))
	{
		bot->secondaryState = BSS_ATTACK;
	}
	else
	{
		// Do we have a good enough gun?
		// Do we need more ammo?
		if(!bot->movetarget)
		{
			// Are there any interesting items lying about?
			BOTAI_PickShortRangeGoal(bot);
		}
		// If it's gone
		if(bot->movetarget && bot->movetarget->solid == SOLID_NOT)
			bot->movetarget = NULL;
		// Otherwise
		if(bot->movetarget)
		{
			BOTCOM_AimAt(bot, bot->movetarget->s.origin, angles);
		}
	}
}

//-----------------------------------------------
// Main entry to the Roaming State
//-----------------------------------------------
void BOTST_Roaming( edict_t *bot, vec3_t angles, usercmd_t *cmd)
{
	// bot_speed and bot_strafe are set in here if needed

	switch( bot->secondaryState)
	{
	case BSS_ATTACK:
		RoamingAttack(bot, angles, cmd);
		break;
	case BSS_NONE:
	default:
		RoamingNone(bot, angles, cmd);
		break;
	}

}
