//---------------------------------------------
// BOT_AI.C
//---------------------------------------------

#include "../g_local.h"
#include "../m_player.h"

/*
 * $Log: /LTK2/src/acesrc/bot_ai.c $
 * 
 * 6     2/03/00 17:47 Riever
 * Removed debug prints and tidied up code
 * 
 * 5     2/03/00 14:09 Riever
 * Added NeedToBandage()
 * 
 * 4     21/02/00 23:43 Riever
 * Added goal checking code and fleshed out AI state. Now selects visible
 * enemies but doesn't search for them yet.
 * 
 * 3     21/02/00 15:16 Riever
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 * 
 * 2     20/02/00 20:27 Riever
 * Added new members and definitions ready for 2nd generation of bots.
 */

//----------------------------------------------
// Set up the goal
//----------------------------------------------
void BOTAI_SetGoal(edict_t *self, int goal_node)
{
	self->goal_node = goal_node;
	self->next_node = self->current_node; // make sure we get to the nearest node first
	self->node_timeout = 0;
}

//----------------------------------------------
// BOTAI_PickShortRangeGoal
//
// (Modified from ACE)
// Pick best goal based on importance and range. This function
// overrides the long range goal selection for items that
// are very close to the bot and are reachable.
//----------------------------------------------

void BOTAI_PickShortRangeGoal(edict_t *bot)
{
	edict_t *pTarget = NULL;
	float fWeight = 0.f, fBestWeight = 0.0;
	edict_t *pBest = NULL;
	
	// look for a target (should make more efficient later)
	pTarget = findradius(NULL, bot->s.origin, 200);
	
	while(pTarget)
	{
		if(pTarget->classname == NULL)
			return;
		
		// Missle avoidance code
		// Set our movetarget to be the rocket or grenade fired at us. 
		if(
			strcmp(pTarget->classname,"rocket")==0 || 
			strcmp(pTarget->classname,"grenade")==0 ||
			strcmp(pTarget->classname,"hgrenade")==0
			)
		{
			if(debug_mode) 
				debug_printf("ROCKET ALERT!\n");

			bot->movetarget = pTarget;
			return;
		}
	
		if (BOTCOL_CanReachItem(bot,pTarget->s.origin))
		{
			if (infront(bot, pTarget))
			{
				fWeight = ACEIT_ItemNeed( bot, pTarget );
				
				if(fWeight > fBestWeight)
				{
					fBestWeight = fWeight;
					pBest = pTarget;
				}
			}
		}
		// next target
		pTarget = findradius(pTarget, bot->s.origin, 200);
	}

	if(fBestWeight)
	{
		bot->movetarget = pBest;
		
		if(debug_mode && bot->goalentity != bot->movetarget)
			debug_printf("%s selected a %s for SR goal.\n",bot->client->pers.netname, bot->movetarget->classname);
		
		bot->goalentity = pBest;
	}
}

//----------------------------------------------
// BOTAI_PickLongRangeGoal
//
// Evaluate the best long range goal of the required type
// GT_POSITION,
// GT_ENEMY,
// GT_ITEM
// Narrow this down later to give many more search options like
// GT_WEAPON
// GT_AMMO, etc.
// 
//----------------------------------------------
void BOTAI_PickLongRangeGoal(edict_t *bot, int	iType)
{

	int i = 0;
	int node = 0;
	float fWeight = 0.f,fBestWeight = 0.0;
	int current_node = 0, goal_node = 0;
	edict_t *goal_ent = NULL;
	float fCost = 0.f;
	
	// look for a target 
	current_node = ACEND_FindClosestReachableNode(bot,NODE_DENSITY,NODE_ALL);

	bot->current_node = current_node;

	// Even in teamplay, we wander if no valid node
	if(current_node == -1)
	{
		bot->nextState = BS_ROAM;
		bot->wander_timeout = level.framenum + 1.0 * HZ;
		bot->goal_node = -1;
		return;
	}

	//======================
	// Looking for a POSITION to move to
	//======================
	if( iType == GT_POSITION )
	{
		int counter = 0;
		fCost = INVALID;
		bot->goal_node = INVALID;

		// Pick a random node to go to
		while( fCost == INVALID && counter < 10) // Don't look for too many
		{
			counter++;
			i = (int)(random() * numnodes -1);	// Any of the current nodes will do
			fCost = ACEND_FindCost(current_node, i);

			if(fCost == INVALID || fCost < 2) // ignore invalid and very short hops
			{
				fCost = INVALID;
				i = INVALID;
				continue;
			}
		}
		// We have a target node - just go there!
		if( i != INVALID )
		{
			bot->tries = 0; // Reset the count of how many times we tried this goal
			ACEND_SetGoal(bot,i);
			bot->wander_timeout = level.framenum + 1.0 * HZ;
			return;
		}
	}

	//=========================
	// Items
	//=========================
	if( iType == GT_ITEM )
	{
		for(i=0;i<num_items;i++)
		{
			if(item_table[i].ent == NULL || item_table[i].ent->solid == SOLID_NOT) // ignore items that are not there.
				continue;
			
			fCost = ACEND_FindCost(current_node,item_table[i].node);
			
			if(fCost == INVALID || fCost < 2) // ignore invalid and very short hops
				continue;
		
			fWeight = ACEIT_ItemNeed( bot, item_table[i].ent );

			if( fWeight <= 0 )  // Ignore items we can't pick up.
				continue;

			fWeight *= ( (rand()%5) +1 ); // Allow random variations
	//		weight /= cost; // Check against cost of getting there
					
			if(fWeight > fBestWeight && item_table[i].node != INVALID)
			{
				fBestWeight = fWeight;
				goal_node = item_table[i].node;
				goal_ent = item_table[i].ent;
			}
		}
	}

	//========================
	// Enemies
	//========================
	if( iType == GT_ENEMY )
	{
		for(i=0;i<num_players;i++)
		{
			if( (players[i] == bot) || (players[i]->solid == SOLID_NOT) )
				continue;

			// If it's dark and he's not already our enemy, ignore him
			if( bot->enemy && players[i] != bot->enemy)
			{

//Disabled by Werewolf
//				if( players[i]->light_level < 30)
//					continue;
			}

			node = ACEND_FindClosestReachableNode(players[i],NODE_DENSITY,NODE_ALL);
			// RiEvEr - bug fixing
			if( node == INVALID)
				fCost = INVALID;
			else
				fCost = ACEND_FindCost(current_node, node);
			if(fCost == INVALID || fCost < 3) // ignore invalid and very short hops
				continue;

			// TeamMates are not enemies!
			if( teamplay->value )
			{
				// If not an enemy, don't choose him
				if( OnSameTeam( bot, players[i]))
					fWeight = 0.0;
				else
					fWeight = 100.0; //Werewolf: was 0.3
			}
			else
			  fWeight = 0.8;	//Werewolf: was 0.3
			
			fWeight *= ( (rand()%5) +1 ); // Allow random variations
	//		weight /= cost; // Check against cost of getting there
			
			if(fWeight > fBestWeight && node != INVALID)
			{		
				fBestWeight = fWeight;
				goal_node = node;
				goal_ent = players[i];
			}	
		}
	} // End GT_ENEMY

	// If do not find a goal, go wandering....
	if(fBestWeight == 0.0 || goal_node == INVALID )
	{
		bot->goal_node = INVALID;
		bot->wander_timeout = level.framenum + 1.0 * HZ;
		if(debug_mode)
			debug_printf("%s did not find a LR goal, wandering.\n",bot->client->pers.netname);
		return; // no path? 
	}
	
	// Reset the count of how many times we tried this goal
	bot->tries = 0; 
	 
	if(goal_ent != NULL && debug_mode)
		debug_printf("%s selected a %s at node %d for LR goal.\n",bot->client->pers.netname, goal_ent->classname, goal_node);

	BOTAI_SetGoal(bot,goal_node);

}

//---------------------------------------------
// BOTAI_VisibleEnemy
//
// Used in roaming state to target only visible enemies
//---------------------------------------------

qboolean BOTAI_VisibleEnemy( edict_t *bot )
{
	vec3_t	v;
	float	fRange, fBestDist = 99999.0;
	edict_t	*goal_ent = NULL;
	int	i;

	for(i=0;i<num_players;i++)
	{
		if( (players[i] == bot) || (players[i]->solid == SOLID_NOT) )
			continue;

		// TeamMates are not enemies!
		if( teamplay->value )
		{
			// If not an enemy, don't choose him
			if( OnSameTeam( bot, players[i]))
				continue;
		}

		// Can't see it - can't shoot it!
		if( !BOTCOL_Visible( bot, players[i] ))
			continue;

		// Get distance to enemy
		VectorSubtract (bot->s.origin, players[i]->s.origin, v);
		fRange = VectorLength(v);

		// If it's dark and he's not already our enemy, ignore him
		if( bot->enemy && players[i] != bot->enemy)
		{
			if( players[i]->light_level < 30)
				continue;
		}

		if(fRange < fBestDist)
		{		
			fBestDist = fRange;
			goal_ent = players[i];
		}	
	}
	// Did we find a visible enemy?
	if( goal_ent != NULL )
	{
		bot->enemy = goal_ent;
		return (true);
	}
	return (false);
}

//---------------------------------------------
// BOTAI_NeedToBandage
//---------------------------------------------

qboolean	BOTAI_NeedToBandage(edict_t *bot)
{
	if ( 
		(bot->client->bandaging == 1)
		|| (bot->client->bleeding == 0 && bot->client->leg_damage == 0)
		)
		return false;
	else
		return true;
}

//---------------------------------------------
//      BOTAI_Think
//
//  The standard entry point for the new bots
//---------------------------------------------

void BOTAI_Think(edict_t *bot)
{
    usercmd_t cmd;
    vec3_t angles = {0,0,0};
//	vec3_t vTempDest, vTvec;	// used by random roaming
//	float  fRoam;		// -1 if no direction to roam, 0 if too soon to check again
    int bs, new_bs;
//    trace_t tTrace;
	float	rnd;//, fLen;
//	gitem_t	*new_weap;

	bot->bot_speed = 0; // initialise to nil to stop all unplanned movement
    VectorCopy (bot->client->v_angle, angles);
	VectorCopy(bot->client->ps.viewangles,bot->s.angles);
    VectorSet (bot->client->ps.pmove.delta_angles, 0, 0, 0);
    memset (&cmd, 0, sizeof(usercmd_t));

	// Stop trying to think if the bot can't respawn.
	if( ! IS_ALIVE(bot) && ((gameSettings & GS_ROUNDBASED) || (bot->client->respawn_framenum > level.framenum)) )
		goto LeaveThink;

    bs = bot->botState;
    new_bs = bot->nextState;

    if (bs != new_bs)
    {
/*        if (new_bs == BS_DEAD)
                gi.bprintf (PRINT_HIGH, "botState: BS_DEAD\n");
        if (new_bs == BS_WAIT)
               gi.bprintf (PRINT_HIGH, "botState: BS_WAIT\n");
        if (new_bs == BS_PASSIVE)
                gi.bprintf (PRINT_HIGH, "botState: BS_PASSIVE\n");
        if (new_bs == BS_ACTIVE)
                gi.bprintf (PRINT_HIGH, "botState: BS_ACTIVE\n");
		if (new_bs == BS_SECURE)
			gi.bprintf (PRINT_HIGH, "botState: BS_SECURE\n");
        if (new_bs == BS_RETREAT)
                gi.bprintf (PRINT_HIGH, "botState: BS_RETREAT\n");
        if (new_bs == BS_HOLD)
                gi.bprintf (PRINT_HIGH, "botState: BS_HOLD\n");
        if (new_bs == BS_SUPPORT)
                gi.bprintf (PRINT_HIGH, "botState: BS_SUPPORT\n");
*/
        bs = new_bs;
		bot->botState = bot->nextState;
    }

    if ((!bot->enemy) && (bs == BS_WAIT))
    {
		bot->nextState = BS_ROAM;
		bot->secondaryState = BSS_NONE;
    }
    if (bot->enemy)
    {
        if ((bot->enemy->deadflag == DEAD_DEAD) 
			&& (bot->deadflag == DEAD_NO)) // to stop a crouch anim death bug
        {
            if (!bot->killchat)
            {
				//Taunt animation added
				// can't wave when ducked or in the air :)
				if(		(!(bot->client->ps.pmove.pm_flags & PMF_DUCKED))
					&&	(bot->groundentity)	)
				//FIXME: Add checks for in range and facing enemy
				{
					bot->bot_speed = 0; // stop moving!
					//Turn to face dead enemy
//					Bot_AimAt( bot, bot->lastSeen, angles);
					bot->client->anim_priority = ANIM_WAVE;
					rnd = random() * 4;
					if (rnd < 1)
					{
						//Taunt them
						bot->s.frame = FRAME_taunt01-1;
						bot->client->anim_end = FRAME_taunt17;
//						gi.bprintf (PRINT_HIGH, "bot_anim: Taunt\n");
					}
					else if (rnd < 2)
					{
						//Sarcastic salute time
						bot->s.frame = FRAME_salute01-1;
						bot->client->anim_end = FRAME_salute11;
//						gi.bprintf (PRINT_HIGH, "bot_anim: Salute\n");
					}
					else if (rnd < 3)
					{
						//Point at them
						bot->s.frame = FRAME_point01-1;
						bot->client->anim_end = FRAME_point12;
//						gi.bprintf (PRINT_HIGH, "bot_anim: Point\n");
					}
					else
					{
						// Give them the finger
						bot->s.frame = FRAME_flip01-1;
						bot->client->anim_end = FRAME_flip12;
//						gi.bprintf (PRINT_HIGH, "bot_anim: FlipOff\n");
					}
				}
	                // Laugh at them!
//				Bot_Chat (bot, bot->enemy, DBC_INSULT);
                bot->killchat = true;
            }
            bot->secondaryState = BSS_NONE;
            bot->enemy = NULL;
			bot->movetarget = NULL;
			bot->goal_node = bot->next_node = INVALID;
			//Allow time to taunt and move
            goto LeaveThink;
        }
        else
        {
			bot->killchat = false;
        }
    }

    // If I'm alive, but bot_state is dead, fix it up (when we respawn)
    if ((bot->deadflag == DEAD_NO) && (bs == BS_DEAD))
    {
		bot->nextState = BS_WAIT;
    }

    // If I'm dead, hit fire to respawn
    if (bot->deadflag == DEAD_DEAD)
    {
        bot->nextState = BS_DEAD;

        // Clear enemy status
        bot->enemy = NULL;
        bot->cansee = false;

        // Clear routing status
        bot->current_node = INVALID;
        bot->next_node = INVALID;
        bot->goal_node = INVALID;
        bot->last_node = INVALID;

        // Stop "key flooding"
        if (level.framenum > bot->client->respawn_framenum)
        {
            bot->client->buttons = 0;       // clear buttons
            cmd.buttons = BUTTON_ATTACK;   // hit the attack button
        }
    }

	// Now the state changing part
	switch( bot->botState )
	{
		// State when no path exists
	case BS_ROAM:
		BOTST_Roaming( bot, angles, &cmd);
		break;
		// Normal 'Active' state
	case BS_ACTIVE:
		BOTST_Active(bot, angles, &cmd);
		break;
		// 'Wait' respawn state
	case BS_WAIT:
		// 'Dead' state
	case BS_DEAD:
		// 'Passive' non-combat state
	case BS_PASSIVE:
		// 'Secure the area'or 'Guard' state
	case BS_SECURE:
		// 'Retreat/Take Cover' state
	case BS_RETREAT:
		// Holding or Sniping state
	case BS_HOLD:
		// Following or supporting state
	case BS_SUPPORT:
		break;
	}

LeaveThink:

	// Set movement speed
	cmd.forwardmove = bot->bot_speed;
	// set approximate ping
	cmd.msec = 1000 / BOT_FPS;
	// show random ping values in scoreboard
	bot->client->ping = cmd.msec;

	// Set angles for move
    cmd.angles[PITCH] = ANGLE2SHORT(angles[PITCH]);
    cmd.angles[YAW]= ANGLE2SHORT(angles[YAW]);
    cmd.angles[ROLL]= ANGLE2SHORT(angles[ROLL]);
	// Save our current position
	VectorCopy (bot->s.origin,bot->lastPosition);
	// Does it need this to know where we're going?
	bot->last_node = ACEND_FindClosestReachableNode( bot, NODE_DENSITY, NODE_ALL );
    
	ClientThink (bot, &cmd);
    
    bot->nextthink = level.framenum + (game.framerate / BOT_FPS);
}

/*
        if (bot->deadflag == DEAD_NO)
        {
			// Show the path (uncomment for debugging)
//			show_path_from = bot->client->bot_route_ai->last_node;
//			show_path_to = bot->bot_ai.targetnode;
//			DrawPath( bot );

                if( (level.time >= bot->bot_ai.next_look_time))
					// Currently allow new enemy checks every frame
                {
					// Now resets movetarget and nodepath
//					Bot_CheckEyes (bot, &bs, &new_bs, angles);

					if (!bot->enemy && !bot->movetarget )
					{
						// Look for something else to collect
						// sets BS_COLLECT, bot->bot_ai.targetnode & bot->movetarget 
//						Bot_FindItems(bot, &bs, &new_bs, angles);
					}
                }

					// Check for Taunt anims before roaming
                if( (bs == BS_WAIT) || (bs == BS_COLLECT) || (bs == BS_HUNT)
					|| (bs == BS_FIND)
					&&	!((bot->s.frame <= FRAME_point12) && (bot->s.frame >= FRAME_flip01)) )
                {

				if (bot->enemy)
                {
                    bot->bot_ai.enemy_range = range (bot, bot->enemy);
                    Bot_ChooseWeapon (bot);
                    if (Bot_Visible (bot, bot->enemy))
                    {
						if( CarryingFlag(bot->enemy) && (bot->bot_ai.flagSayTime < level.time) )
						{
							// Tell the team we saw their flag carrier
							CTFSay_Team(bot, "Enemy flag carrier is %l\n");
							bot->bot_ai.flagSayTime = level.time + 7.0;
						}
						new_bs = BS_HUNT;
                        Bot_Attack (bot, &cmd, angles);
						goto LeaveThink;
                    }
                    else if(!bot->movetarget)
                    {
						new_bs = BS_FIND;
						trace = gi.trace(bot->s.origin, VEC_ORIGIN, VEC_ORIGIN,
							bot->bot_ai.last_seen, bot, MASK_PLAYERSOLID);
						if (trace.fraction == 1.0) // direct route available
						{
							Bot_AimAt(bot, bot->bot_ai.last_seen, angles);
							//drop through to roam code
						}
						// Don't chase if we have the flag
						else if (!CarryingFlag(bot))
						{
							// set our enemy up as an object we want to collect!
							// only uses his last seen position - no cheating here!
							bot->movetarget = bot->enemy;
							bot->bot_ai.targetnode = ens_NodeProximity (bot->movetarget->s.origin);
							//drop through to collect code
						}
                    }
					if (bot->bot_ai.last_seen_time < level.time - 5.0)
					{
						// We ain't seen him for quite a while
						bot->enemy = bot->movetarget = NULL;
						bot->bot_ai.targetnode = bot->bot_ai.nextnode = NOPATH;
//						AntInitSearch( bot );
						new_bs = BS_WAIT;
						// drop through to roam code
					}
					// Don't chase if we have the flag in CTF
					else if (!CarryingFlag(bot))
					{
						new_bs = BS_FIND;
						// set our enemy up as an object we want to collect!
						// only uses his last seen position - no cheating here!
						bot->movetarget = bot->enemy;
						bot->bot_ai.targetnode = ens_NodeProximity (bot->movetarget->s.origin);
						//drop through to collect code
					}
                }


//==========================================================
// New BS_COLLECT code start
//==========================================================
//NodePaths:
					if( (bot->movetarget) && (bot->bot_ai.targetnode != NOPATH) )
					{
						//gi.bprintf(PRINT_HIGH,"BS_COLLECT: Target node is %i\n",(int)bot->bot_ai.targetnode);
						if (AntPathMove(bot))
						{
//							gi.bprintf(PRINT_HIGH,"BS_COLLECT: Routing to node %i from %i\n",
//								(int)bot->bot_ai.nextnode, 
//								(int)bot->client->bot_route_ai->last_node);

							if( bot->bot_ai.nextnode != NOPATH)
							{
								if(bot->bot_ai.nextnode != bot->bot_ai.aimnode) 
								// not aiming for the same node again
								{
									// LADDER ====================
									else if( (node_array[bot->bot_ai.nextnode].method == LADDER)
										&& (bot->bot_ai.nextnode != bot->client->bot_route_ai->last_node) )
									{
										Bot_AimAt(bot, node_array[bot->bot_ai.nextnode].position,angles);
										bot->bot_ai.bot_speed = SPEED_RUN;
										cmd.upmove = SPEED_WALK;
										bot->bot_ai.aimnode = bot->bot_ai.nextnode;
										// If no enemies, get out of here!
										if( !bot->enemy )
											goto LeaveThink;
									}
									// End Ladder ===============
									else
									{
										// move to nextnode
										// currently just walk between them - no context information yet
										Bot_AimAt(bot, node_array[bot->bot_ai.nextnode].position, angles);
										bot->bot_ai.bot_speed = SPEED_WALK;
										bot->bot_ai.aimnode = bot->bot_ai.nextnode;
										// DEBUGGING!! Take this out later:
										// goto LeaveThink;
										// On next time through, roam code will take over
									}
								}
								// else drop through to roam code
							}
							else if ( bot->movetarget->solid != SOLID_NOT)
							{
								//turn to collect it
								Bot_Aim( bot, bot->movetarget, angles);
								bot->bot_ai.bot_speed = SPEED_WALK;
								// we're there so delete the path info
								bot->bot_ai.targetnode = NOPATH;
								bot->bot_ai.nextnode = NOPATH;
								//DEBUG goto LeaveThink;
								//variables are reset by collection in g_items.c
								// continue into roam code
							}
							else if (bot->movetarget != bot->enemy)
							{
								// it's gone, forget it!
								bot->movetarget = NULL;
								bot->bot_ai.targetnode = NOPATH;
								bot->bot_ai.nextnode = NOPATH;
								new_bs = BS_WAIT;
								// Continue into roam code
							}
						}
						else if (bot->movetarget)
						{
							bot->bot_ai.targetnode = NOPATH; // no path, clear it
							bot->bot_ai.nextnode = NOPATH;
							if( Bot_Visible( bot, bot->movetarget) )
							{
								Bot_Aim( bot, bot->movetarget, angles);
								// FIXME: Check if we need to grapple to item
								// let the roam code move us
							}
							else if (bot->movetarget != bot->enemy)
							{
								// can't see it, forget it!
								bot->movetarget = NULL;
								bot->bot_ai.nextnode = NOPATH;
								new_bs = BS_WAIT;
								// Continue into roam code
							}
						}
					}
					else if (bot->movetarget)
					{
						if( 
							(Bot_Visible( bot, bot->movetarget) )
							&& (range(bot, bot->movetarget)<= RANGE_NEAR)
							)
						{
							if ( CanGrapple(bot, bot->movetarget->s.origin, angles))
							{
								//gi.bprintf(PRINT_HIGH,"Found grapple route\n");
								// tempvector will be set
								Bot_AimAt(bot,bot->bot_ai.tempvector, angles);
								if (!(strcmp(bot->client->pers.weapon->classname, "weapon_grapple") == 0))
								{
									// Change to grapple
									//gi.bprintf(PRINT_HIGH,"Bot changing weapon to grapple...\n");
									new_weap = FindItem("Grapple");
									bot->client->newweapon = bot->client->pers.weapon = new_weap;
									ChangeWeapon(bot);
									goto LeaveThink;
								}
								else
								{
									// aim and shoot
									Bot_AimAt(bot,bot->bot_ai.tempvector, angles);
									cmd.buttons = BUTTON_ATTACK;
									if(
										!bot->enemy
										&& (bot->client->ctf_grapplestate < CTF_GRAPPLE_STATE_HANG)
										)
										goto LeaveThink;
									// If problems, goto leavethink after checking for
									// grapplestate HANG
								}
							}
							Bot_Aim( bot, bot->movetarget, angles);
							// continue into roam code
						}
						else if (bot->movetarget != bot->enemy)
						{
							// can't see it, forget it!
							bot->movetarget = NULL;
							bot->bot_ai.targetnode = NOPATH;
							bot->bot_ai.nextnode = NOPATH;
							new_bs = BS_WAIT;
							// Continue into roam code
						}
					}

//=====================================================
// New Collect code end
//=====================================================
//----------------------------------------------------------------
// NEW BS_WAIT CODE START
//----------------------------------------------------------------
//RoamingAI:
					//deal with completely stuck states first:
					VectorSubtract(bot->bot_ai.last_pos, bot->s.origin, vTvec);
					// Check if we have been able to move using length between points
					// if we are stuck, deal with it.
					len = VectorLength(vTvec);
					if( (len <= 2.0) )
					{
						// we are stuck!
						// Stop grappling just in case
						cmd.buttons = 0;
						//gi.bprintf(PRINT_HIGH,"BS_WAIT: Stuck!\n");
						roam = Bot_FindBestDirection (bot, vTempDest, angles);
						if (roam > 0)
						{
							// Turn to face that direction
							Bot_AimAt (bot, vTempDest, angles);
							bot->bot_ai.crawl = false;
							bot->bot_ai.last_jump = false;
							bot->bot_ai.bot_speed = SPEED_ROAM;
							cmd.upmove = SPEED_WALK; // jump to get out of trouble
							//cmd->upmove = SPEED_WALK; // jump to get out of trouble
						}
						else if(roam == -1) // we are really stuck!
						{
							// TODO: climb, grapple or kill myself!
							Cmd_Kill_f (bot); // suicide if stuck
						}
						else // Roam == 0
						{
							// do nothing - just wait till next second :)
						}
						cmd.forwardmove = bot->bot_ai.bot_speed;
						goto LeaveThink;
					}
					if( (bot->waterlevel < 2) 
						&& (bot->client->old_waterlevel <2)
						&& bot->groundentity )
					{
						if (bot->groundentity)
						{
							// Reset water timer
							bot->bot_ai.watertimer = level.time;
						}
						if ( LadderForward (bot, angles)
							&& CanJumpUp (bot))
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: Ladder\n");
							bot->bot_ai.bot_speed = 0;
							cmd.upmove = SPEED_WALK;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						else if( CanJumpGap(bot, angles) ) // checks for safety
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: CanJumpGap\n");
							cmd.upmove = SPEED_WALK;
							bot->bot_ai.bot_speed = SPEED_WALK;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						else if( CanMoveSafely (bot, angles)
							&&	CanMoveForward (bot, angles) )
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: Moving forward\n");
							bot->bot_ai.bot_speed = SPEED_WALK;
							bot->bot_ai.last_jump = false;
							if (CanStand (bot))
							{
								//Stand up
								bot->bot_ai.crawl = false;
							}
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						else if( !CanMoveForward (bot, angles)
							&&	CanCrawlForward (bot, angles)	)
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: Crawling\n");
							cmd.upmove = -SPEED_WALK;
							bot->client->ps.pmove.pm_flags |= PMF_DUCKED;
							bot->bot_ai.bot_speed = SPEED_WALK;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						// Possibly need a safety check here..
						else if( (CanMoveSafely(bot,angles))
							&& (CanJumpForward (bot, angles)) 
							&& !bot->bot_ai.last_jump)//&& (HasMoved (bot) )
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: Jumping\n");
							cmd.upmove = SPEED_WALK;
							bot->bot_ai.last_jump = true;
							bot->bot_ai.bot_speed = SPEED_WALK;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						else if (Bot_CheckBump( bot, &cmd))
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: Checkbump\n");
							cmd.sidemove = bot->bot_ai.bot_strafe;
						}
						else
						{
							//gi.bprintf(PRINT_HIGH,"BS_WAIT: NewDirection\n");
							if (!CanMoveSafely (bot, angles)) // Lava, slime, etc.
							{
								roam = Bot_FindShortBestDirection( bot, vTempDest, angles);
							}
							else
							{
								roam = Bot_FindBestDirection (bot, vTempDest, angles);
							}
							if (roam > 0)
							{
								// Turn to face that direction
								Bot_AimAt (bot, vTempDest, angles);
								bot->bot_ai.crawl = false;
								bot->bot_ai.last_jump = false;
								bot->bot_ai.bot_speed = SPEED_ROAM;
							}
							else if(roam == -1) // we are really stuck!
							{
								// TODO: climb, grapple or kill myself!
							}
							else // Roam == 0
							{
								// do nothing - just wait till next second :)
							}
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
					}
					else if (bot->waterlevel)
					{
						if (bot->movetarget && bot->bot_ai.nextnode)
						{
							if( node_array[bot->bot_ai.nextnode].contents && IN_WATER)
							{
								//special handling for water nodes
								if ( node_array[bot->bot_ai.nextnode].position[2] > bot->s.origin[2])
								{
									cmd.upmove = SPEED_WALK;
								}
								bot->bot_ai.bot_speed = SPEED_WALK;
							}
							else
							{
								// getting out of the water!
								cmd.upmove = SPEED_RUN;
								bot->bot_ai.bot_speed = SPEED_RUN;
							}
						}
						else if ( WaterMoveForward (bot, angles))
						{
							bot->bot_ai.bot_speed = SPEED_WALK;
							cmd.forwardmove = bot->bot_ai.bot_speed;
						}
//						else if (bot->waterlevel < 3) // at the surface
						else if (CanLeaveWater(bot,angles)) // at the surface and edge??
						{
							cmd.upmove = SPEED_RUN;
							bot->bot_ai.bot_speed = SPEED_RUN;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
							// Removed all this - trying for simplicity!
						}
						else //we hit a wall!
						{
							float	temprand;

							temprand = random();
							if (temprand < 0.3)
								cmd.upmove = SPEED_WALK;
							else if (temprand < 0.6)
								cmd.sidemove = SPEED_WALK;
							else
								bot->bot_ai.bot_speed = 0;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						if (bot->air_finished < (level.time +5)) // if drowning..
						{
							cmd.upmove = SPEED_WALK;
							bot->bot_ai.bot_speed = SPEED_WALK;
							cmd.forwardmove = bot->bot_ai.bot_speed;
							goto LeaveThink;
						}
						goto LeaveThink;
					}
					else if ( !bot->groundentity) // We're in the air
					{
						//@@ Check for steps!
						//gi.bprintf(PRINT_HIGH,"BS_WAIT: Woohoo - we're flying!\n");
						cmd.upmove = SPEED_RUN;
						bot->bot_ai.bot_speed = SPEED_RUN;
						cmd.forwardmove = bot->bot_ai.bot_speed;
						goto LeaveThink;
					}
							

// NEW CODE END
//======================================================

                }
				else if ((bot->s.frame <= FRAME_point12) && (bot->s.frame >= FRAME_flip01))
				{
					//Reset stuck time because we're taunting someone!
					bot->bot_ai.last_jump = false;
				}
LeaveThink:
            if (VectorCompare (bot->bot_ai.last_hurt, vec3_origin) == 0)
            {
                if ((bs == BS_WAIT)  || (bs == BS_COLLECT))
                {
                        // Turn to my hurt direction
                        // FIXME: Does this work?!?
                     Bot_AimAt (bot, bot->bot_ai.last_hurt, angles);
                     VectorClear (bot->bot_ai.last_hurt);
                }
				//RiEvEr
				else if( ((bs == BS_HUNT) || (bs == BS_FIND)) && (bot->bot_ai.change_enemy) )
				{
					// We've been shot by someone else
					// and they are a better target or more of a threat
					bot->bot_ai.change_enemy = false; // set in Bot_Pain
                    Bot_AimAt (bot, bot->bot_ai.last_hurt, angles);
                    VectorClear (bot->bot_ai.last_hurt);
					// clear any previous information just in case!
					bot->movetarget = NULL;
					bot->bot_ai.targetnode = bot->bot_ai.nextnode = NOPATH;
				}
				//R
            }
        }


		cmd.forwardmove = bot->bot_ai.bot_speed;
        cmd.msec = 100;
 //       if (level.framenum%20 == (int)(random() * 19))
 //       {
             // cmd.msec = PING_AVG + (PING_STD_DEV * (random()-0.5) * 2);
             // bot->client->ping = cmd.msec;
//        }

        cmd.angles[PITCH] = ANGLE2SHORT(angles[PITCH]);
        cmd.angles[YAW]= ANGLE2SHORT(angles[YAW]);
        cmd.angles[ROLL]= ANGLE2SHORT(angles[ROLL]);
        bot->client->bs = bs;
        bot->client->new_bs = new_bs;
		// Save our current position
		VectorCopy (bot->s.origin,bot->bot_ai.last_pos);
		// Does it need this to know where we're going?
		bot->client->bot_route_ai->last_node = ens_NodeProximity (bot->s.origin);
        
		ClientThink (bot, &cmd);
        
        bot->nextthink = level.framenum + (game.framerate / BOT_FPS);
}*/
