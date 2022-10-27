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
 * $Header: /LTK2/src/acesrc/acebot_nodes.c 9     29/02/00 22:59 Riever $
 *
 * $Log: /LTK2/src/acesrc/acebot_nodes.c $
 * 
 * 9     29/02/00 22:59 Riever
 * Updated removelink to work with antpath.
 * 
 * 8     27/02/00 13:08 Riever
 * Enabled sensible jump linking for humans.
 * 
 * 7     24/02/00 20:03 Riever
 * Reverse link code re-enabled.
 * 
 * 6     24/02/00 3:05 Riever
 * Updated LTK nodes version to 4
 * 
 * User: Riever       Date: 23/02/00   Time: 23:19
 * New door node creation (a node either side).
 * Nodes raised to allow better linking.
 * Temp nodes at routing only exist for 60 seconds.
 * User: Riever       Date: 23/02/00   Time: 17:24
 * Added support for 'sv shownodes on/off'
 * Enabled creation of nodes for ALL doors. (Stage 1 of new method)
 * User: Riever       Date: 21/02/00   Time: 15:16
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 * User: Riever       Date: 20/02/00   Time: 20:27
 * Added new members and definitions ready for 2nd generation of bots.
 * 
 */

///////////////////////////////////////////////////////////////////////	
//
//  acebot_nodes.c -   This file contains all of the 
//                     pathing routines for the ACE bot.
// 
///////////////////////////////////////////////////////////////////////

#include "../g_local.h"

#include "acebot.h"

#define LTK_NODEVERSION 4

// flags
qboolean newmap=true;

// Total number of nodes that are items
int numitemnodes; 

// Total number of nodes
int numnodes; 

// For debugging paths
int show_path_from = -1;
int show_path_to = -1;

// array for node data
node_t nodes[MAX_NODES]; 
short int path_table[MAX_NODES][MAX_NODES];

///////////////////////////////////////////////////////////////////////
// NODE INFORMATION FUNCTIONS
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Determin cost of moving from one node to another
///////////////////////////////////////////////////////////////////////
int ACEND_FindCost(int from, int to)
{
	// RiEvEr - Bug Hunting
	int curnode = INVALID;
	int cost=1; // Shortest possible is 1

	// If we can not get there then return invalid
	if( (from == INVALID) || (to == INVALID) || 
		(path_table[from][to] == INVALID)	)
		return INVALID;

	// Otherwise check the path and return the cost
	curnode = path_table[from][to];

	// Find a path (linear time, very fast)
	while(curnode != to)
	{
		curnode = path_table[curnode][to];
		if(curnode == INVALID) // something has corrupted the path abort
			return INVALID;
		if(cost > numnodes) // Sanity check to avoid infinite loop.
		        return INVALID;
		cost++;
	}
	
	return cost;
}

///////////////////////////////////////////////////////////////////////
// Find a close node to the player within dist.
//
// Faster than looking for the closest node, but not very 
// accurate.
///////////////////////////////////////////////////////////////////////
int ACEND_FindCloseReachableNode(edict_t *self, int range, int type)
{
	vec3_t v;
	int i;
	trace_t tr;
	float dist;
	vec3_t maxs,mins;

	VectorCopy(self->mins,mins);
	mins[2] += 16;
	VectorCopy(self->maxs,maxs);
	maxs[2] -= 16;

	range *= range;

	for(i=0;i<numnodes;i++)
	{
		if(type == NODE_ALL || type == nodes[i].type) // check node type
		{
		
			VectorSubtract(nodes[i].origin,self->s.origin,v); // subtract first

			dist = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

			if(dist < range) // square range instead of sqrt
			{
				// make sure it is visible
				//AQ2 ADDED MASK_SOLID
				//trace = gi.trace (self->s.origin, vec3_origin, vec3_origin, nodes[i].origin, self, MASK_SOLID|MASK_OPAQUE);
				tr = gi.trace (self->s.origin, mins, maxs, nodes[i].origin, self, MASK_ALL);

				if(tr.fraction == 1.0)
					return i;
			}
		}
	}

	return -1;
}


int ACEND_DistanceToTargetNode(edict_t *self)
{
	float dist;
	//int node=-1;
	vec3_t v;

	VectorSubtract(nodes[self->goal_node].origin, self->s.origin,v); // subtract first
	dist = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	return dist;
}


///////////////////////////////////////////////////////////////////////
// Find the closest node to the player within a certain range
///////////////////////////////////////////////////////////////////////
int ACEND_FindClosestReachableNode(edict_t *self, int range, int type)
{
	int i;
	float closest = 99999;
	float dist;
	int node=-1;
	vec3_t v;
	trace_t tr;
	float rng;
	vec3_t maxs,mins;

	VectorCopy(self->mins,mins);
	VectorCopy(self->maxs,maxs);
	
	// For Ladders, do not worry so much about reachability
	if(type == NODE_LADDER)
	{
		VectorCopy(vec3_origin,maxs);
		VectorCopy(vec3_origin,mins);
	}
	else
	{
		mins[2] += 18; // Stepsize
		maxs[2] -= 16; // Duck a little.. 
	}

	rng = (float)(range * range); // square range for distance comparison (eliminate sqrt)	
	
	for(i=0;i<numnodes;i++)
	{		
		if(type == NODE_ALL || type == nodes[i].type) // check node type
		{
			VectorSubtract(nodes[i].origin, self->s.origin,v); // subtract first

			dist = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
		
			if(dist < closest && dist < rng) 
			{
				// make sure it is visible
				//AQ2 added MASK_SOLID
				tr = gi.trace (self->s.origin, mins, maxs, nodes[i].origin, self, MASK_SOLID|MASK_OPAQUE);
//				tr = gi.trace (self->s.origin, vec3_origin, vec3_origin, nodes[i].origin, self, MASK_ALL);
				if( (tr.fraction == 1.0) ||
					(	(tr.fraction > 0.9) // may be blocked by the door itself!
						&& (Q_stricmp(tr.ent->classname, "func_door_rotating") == 0)	)	
					)
				{
					node = i;
					closest = dist;
				}
			}
		}
	}
	
	return node;
}

///////////////////////////////////////////////////////////////////////
// Find the closest node to the player within a certain range that doesn't have LOS to an enemy
///////////////////////////////////////////////////////////////////////
int ACEND_FindClosestReachableSafeNode(edict_t *self, int range, int type)
{
	int i;
	float closest = 99999;
	float dist;
	int node=-1;
	vec3_t v;
	trace_t tr;
	float rng;
	vec3_t maxs,mins;
	int curplayer;
	int is_safe=1;

	VectorCopy(self->mins,mins);
	VectorCopy(self->maxs,maxs);
	
	// For Ladders, do not worry so much about reachability
	if(type == NODE_LADDER)
	{
		VectorCopy(vec3_origin,maxs);
		VectorCopy(vec3_origin,mins);
	}
	else
	{
		mins[2] += 18; // Stepsize
		maxs[2] -= 16; // Duck a little.. 
	}

	rng = (float)(range * range); // square range for distance comparison (eliminate sqrt)	
	
	for(i=0;i<numnodes;i++)
	{		
		if(type == NODE_ALL || type == nodes[i].type) // check node type
		{
			VectorSubtract(nodes[i].origin, self->s.origin,v); // subtract first

			dist = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
		
			if(dist < closest && dist < rng && dist > 200) 
			{
				// make sure it is visible
				//AQ2 added MASK_SOLID
				tr = gi.trace (self->s.origin, mins, maxs, nodes[i].origin, self, MASK_SOLID|MASK_OPAQUE);
//				tr = gi.trace (self->s.origin, vec3_origin, vec3_origin, nodes[i].origin, self, MASK_ALL);
				if( (tr.fraction == 1.0) ||
					(	(tr.fraction > 0.9) // may be blocked by the door itself!
						&& (Q_stricmp(tr.ent->classname, "func_door_rotating") == 0)	)	
					)
				{

					is_safe=1;
					for (curplayer=0; curplayer<num_players; curplayer++)
					if( (players[curplayer]->team != self->team) && (players[curplayer]->solid != SOLID_NOT) )
					{
						tr = gi.trace (self->s.origin, tv(-8,-8,-8), tv(8,8,8), players[curplayer]->s.origin, self, MASK_SOLID|MASK_OPAQUE);
						if (tr.fraction == 1.0)
						{
							is_safe=0;
							break;
						}
					}

					if (is_safe==1)
					{
						node = i;
						closest = dist;
					}

				}
			}
		}
	}
	
	return node;
}


///////////////////////////////////////////////////////////////////////
// BOT NAVIGATION ROUTINES
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Set up the goal
///////////////////////////////////////////////////////////////////////
void ACEND_SetGoal(edict_t *self, int goal_node)
{
	int node;

	self->goal_node = goal_node;
	node = ACEND_FindClosestReachableNode(self, NODE_DENSITY*3, NODE_ALL);
	
	if(node == -1)
	{
		self->node_timeout /= 2;  // If invalid, wait half the time again, then retry.
		return;
	}
	
	if(debug_mode)
		debug_printf("%s new start node selected %d\n",self->client->pers.netname,node);
	
	self->current_node = node;
	self->next_node = self->current_node; // make sure we get to the nearest node first
	self->node_timeout = 0;
}

///////////////////////////////////////////////////////////////////////
// Move closer to goal by pointing the bot to the next node
// that is closer to the goal
///////////////////////////////////////////////////////////////////////
qboolean ACEND_FollowPath(edict_t *self)
{
	vec3_t v;
	
	//////////////////////////////////////////
	// Show the path (uncomment for debugging)
	show_path_from = self->current_node;
	show_path_to = self->goal_node;

	if( ltk_showpath->value )
	{
		ACEND_DrawPath(self);
	}
	//////////////////////////////////////////

	// Try again?
	if(self->node_timeout ++ > 5*BOT_FPS)
	{
		if(self->tries++ >= 3)
			return false;
		else
			ACEND_SetGoal(self,self->goal_node);
	}

	//RiEvEr - new path code & algorithm
	// This part checks if we are off course
	if( (level.framenum % HZ == 0) || (self->node_timeout == 0) )
	{
		if( (self->goal_node == INVALID) || (
			!AntLinkExists( self->current_node, SLLfront(&self->pathList) )
			&& ( self->current_node != SLLfront(&self->pathList) )
			))
		{
			// We are off the path - clear out the lists
			AntInitSearch( self );
		}
	}
	// Boot in our new pathing algorithm
	// This will fill self->pathList with the information we need
	if( 
		SLLempty(&self->pathList)				// We have no path and
		&& (self->current_node != self->goal_node)	// we're not at our destination
		)
	{
		if( !AntStartSearch( self, self->current_node, self->goal_node))	// Set up our pathList
		{
			// Failed to find a path
			if( debug_mode )
				gi.bprintf(PRINT_HIGH,"%s: Target at(%i) - No Path \n",
					self->client->pers.netname, self->goal_node, self->next_node);
			return false;
		}
//		return true;
	}
	//R

	// Are we there yet?
	VectorSubtract(self->s.origin,nodes[self->next_node].origin,v);

	if(VectorLength(v) < 32) 
	{
		// reset timeout
		self->node_timeout = 0;

		if(self->next_node == self->goal_node)
		{
			if(debug_mode)
				debug_printf("%s reached goal node %i!\n", self->client->pers.netname, self->goal_node);

			if (self->state == STATE_FLEE)
				ACEAI_PickSafeGoal(self);
			else
				ACEAI_PickLongRangeGoal(self); // Pick a new goal
		}
		else
		{
			self->current_node = self->next_node;
//			self->next_node = path_table[self->current_node][self->goal_node];
			// Removethe front entry from the list
			if( self->next_node == SLLfront(&self->pathList) )
				SLLpop_front(&self->pathList);
			// Get the next node - if there is one!
			if( !SLLempty(&self->pathList))
			{
				self->next_node = SLLfront( &self->pathList);
				if(debug_mode)
					debug_printf("%s reached node %i, next is %i.\n", self->client->pers.netname, self->current_node, self->next_node);
			}
			else
			{
				// We messed up...
				if( debug_mode)
					gi.bprintf(PRINT_HIGH, "Trying to read an empty SLL nodelist!\n");
				self->next_node = INVALID;
			}
		}
	}
	
	if(self->current_node == -1 || self->next_node ==-1)
		return false;
	
	// Set bot's movement vector
	VectorSubtract (nodes[self->next_node].origin, self->s.origin , self->move_vector);
	
	return true;
}


///////////////////////////////////////////////////////////////////////
// MAPPING CODE
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Capture when the grappling hook has been fired for mapping purposes.
///////////////////////////////////////////////////////////////////////
void ACEND_GrapFired(edict_t *self)
{
/*	int closest_node;
	
	if(!self->owner)
		return; // should not be here
	
	// Check to see if the grapple is in pull mode
	if(self->owner->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL)
	{
		// Look for the closest node of type grapple
		closest_node = ACEND_FindClosestReachableNode(self,NODE_DENSITY,NODE_GRAPPLE);
		if(closest_node == -1 ) // we need to drop a node
		{	
			closest_node = ACEND_AddNode(self,NODE_GRAPPLE);
			 
			// Add an edge
			ACEND_UpdateNodeEdge(self, self->owner->last_node,closest_node);
		
			self->owner->last_node = closest_node;
		}
		else
			self->owner->last_node = closest_node; // zero out so other nodes will not be linked
	}*/
}


///////////////////////////////////////////////////////////////////////
// Check for adding ladder nodes
///////////////////////////////////////////////////////////////////////
qboolean ACEND_CheckForLadder(edict_t *self)
{
	int closest_node;

	// If there is a ladder and we are moving up, see if we should add a ladder node
	if (gi.pointcontents(self->s.origin) & CONTENTS_LADDER && self->velocity[2] > 0)
	{
		//debug_printf("contents: %x\n",tr.contents);

		closest_node = ACEND_FindClosestReachableNode(self,NODE_DENSITY,NODE_LADDER); 
		if(closest_node == -1)
		{
			closest_node = ACEND_AddNode(self,NODE_LADDER);
	
			// Now add link
		    ACEND_UpdateNodeEdge(self, self->last_node,closest_node);	   
			
			// Set current to last
			self->last_node = closest_node;
		}
		else
		{
			ACEND_UpdateNodeEdge(self, self->last_node,closest_node);	   
			self->last_node = closest_node; // set visited to last
		}
		return true;
	}
	return false;
}

//=======================================================
// LadderForward
//=======================================================
//
// The ACE code version of this doesn't work!

qboolean	ACEND_LadderForward( edict_t *self )//, vec3_t angles )
{
	vec3_t	dir, angle, dest, min, max;
	trace_t	trace;
	int closest_node;


	VectorClear(angle);
	angle[1] = self->s.angles[1];

	AngleVectors(angle, dir, NULL, NULL);
	VectorCopy(self->mins,min);
	min[2] += 22;
	VectorCopy(self->maxs,max);
	VectorMA(self->s.origin, TRACE_DIST_LADDER, dir, dest);

	trace = gi.trace(self->s.origin, min, max, dest, self, MASK_ALL);

	//BOTUT_TempLaser(self->s.origin, dest);
	if (trace.fraction == 1.0)
		return (false);

//	gi.bprintf(PRINT_HIGH,"Contents forward are %d\n", trace.contents);
	if (trace.contents & CONTENTS_LADDER || trace.contents &CONTENTS_DETAIL)
	{
		// Debug print
//		gi.bprintf(PRINT_HIGH,"contents: %x\n",trace.contents);

		closest_node = ACEND_FindClosestReachableNode(self,NODE_DENSITY,NODE_LADDER); 
		if(closest_node == -1)
		{
			closest_node = ACEND_AddNode(self,NODE_LADDER);
	
			// Now add link
		    ACEND_UpdateNodeEdge(self, self->last_node,closest_node);	   
			
			// Set current to last
			self->last_node = closest_node;
		}
		else
		{
			ACEND_UpdateNodeEdge(self, self->last_node,closest_node);	   
			self->last_node = closest_node; // set visited to last
		}
		return (true);
	}		
	return (false);
}


///////////////////////////////////////////////////////////////////////
// This routine is called to hook in the pathing code and sets
// the current node if valid.
///////////////////////////////////////////////////////////////////////
void ACEND_PathMap(edict_t *self)
{
	int closest_node;
	// Removed last_update checks since this stopped multiple node files being built
	vec3_t v;

	// Special node drawing code for debugging
	if( ltk_showpath->value )
	{
		if(show_path_to != -1)
			ACEND_DrawPath( self );
	}
	
	// Just checking lightlevels - uncomment to use
//	if( debug_mode && !self->is_bot)
//		gi.bprintf(PRINT_HIGH,"LightLevel = %d\n", self->light_level);

	////////////////////////////////////////////////////////
	// Special check for ladder nodes
	///////////////////////////////////////////////////////
	// Replace non-working ACE version with mine.
//	if(ACEND_CheckForLadder(self)) // check for ladder nodes
	if(ACEND_LadderForward(self)) // check for ladder nodes
		return;

	// Not on ground, and not in the water, so bail
    if(!self->groundentity && !self->waterlevel)
		return;

	////////////////////////////////////////////////////////
	// Lava/Slime
	////////////////////////////////////////////////////////
	VectorCopy(self->s.origin,v);
	v[2] -= 18;
	if(gi.pointcontents(v) & (CONTENTS_LAVA|CONTENTS_SLIME))
		return; // no nodes in slime
	
    ////////////////////////////////////////////////////////
	// Jumping
	///////////////////////////////////////////////////////
	if(self->is_jumping)
	{
	   // See if there is a closeby jump landing node (prevent adding too many)
		closest_node = ACEND_FindClosestReachableNode(self, 64, NODE_JUMP);

		if(closest_node == INVALID)
			closest_node = ACEND_AddNode(self,NODE_JUMP);
		
		// Now add link
		if(self->last_node != -1)
			ACEND_UpdateNodeEdge(self, self->last_node, closest_node);	   

		self->is_jumping = false;
		return;
	}

	// Werewolf:
    ////////////////////////////////////////////////////////
	// Switches, etc. - uses the "grapple" nodetype
	///////////////////////////////////////////////////////
	if(self->is_triggering)
	{
	   // See if there is a closeby grapple node (prevent adding too many)
//		closest_node = ACEND_FindClosestReachableNode(self, 64, NODE_GRAPPLE);

//		if(closest_node == INVALID)
			closest_node = ACEND_AddNode(self,NODE_GRAPPLE);
		
		// Now add link
		if(self->last_node != -1)
			ACEND_UpdateNodeEdge(self, self->last_node, closest_node);	   

		self->is_triggering = false;
		return;
	}


/*	////////////////////////////////////////////////////////////
	// Grapple
	// Do not add nodes during grapple, added elsewhere manually
	////////////////////////////////////////////////////////////
	if(ctf->value && self->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL)
		return;*/
	 
	// Iterate through all nodes to make sure far enough apart
	closest_node = ACEND_FindClosestReachableNode(self, NODE_DENSITY, NODE_ALL);

	////////////////////////////////////////////////////////
	// Special Check for Platforms
	////////////////////////////////////////////////////////
	if(self->groundentity && self->groundentity->use == Use_Plat)
	{
		if(closest_node == INVALID)
			return; // Do not want to do anything here.

		// Here we want to add links
		if(closest_node != self->last_node && self->last_node != INVALID)
			ACEND_UpdateNodeEdge(self, self->last_node,closest_node);	   

		self->last_node = closest_node; // set visited to last
		return;
	}
	 
	 ////////////////////////////////////////////////////////
	 // Add Nodes as needed
	 ////////////////////////////////////////////////////////
	 if(closest_node == INVALID)
	 {
		// Add nodes in the water as needed
		if(self->waterlevel)
			closest_node = ACEND_AddNode(self,NODE_WATER);
		else
		    closest_node = ACEND_AddNode(self,NODE_MOVE);
		
		// Now add link
		if(self->last_node != -1)
			ACEND_UpdateNodeEdge(self, self->last_node, closest_node);	   
			
	 }
	 else if(closest_node != self->last_node && self->last_node != INVALID)
	 	ACEND_UpdateNodeEdge(self, self->last_node,closest_node);	   
	
	 self->last_node = closest_node; // set visited to last
	
}

///////////////////////////////////////////////////////////////////////
// Init node array (set all to INVALID)
///////////////////////////////////////////////////////////////////////
void ACEND_InitNodes(void)
{
	numnodes = 1;
	numitemnodes = 1;
	memset(nodes,0,sizeof(node_t) * MAX_NODES);
	memset(path_table,INVALID,sizeof(short int)*MAX_NODES*MAX_NODES);
			
}

///////////////////////////////////////////////////////////////////////
// Show the node for debugging (utility function)
///////////////////////////////////////////////////////////////////////
void ACEND_ShowNode(int node)
{
	edict_t *ent;

//	return; // commented out for now. uncommend to show nodes during debugging,
	        // but too many will cause overflows. You have been warned.

	ent = G_Spawn();

	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;

	if(nodes[node].type == NODE_MOVE)
		ent->s.renderfx = RF_SHELL_BLUE;
	else if (nodes[node].type == NODE_WATER)
		ent->s.renderfx = RF_SHELL_RED;
	else			
		ent->s.renderfx = RF_SHELL_GREEN; // action nodes

	ent->s.modelindex = gi.modelindex ("models/items/ammo/grenades/medium/tris.md2");
	ent->owner = ent;
	ent->nextthink = level.framenum + 60 * HZ; // 1 minute is long enough!
	ent->think = G_FreeEdict;                
	ent->dmg = 0;

	VectorCopy(nodes[node].origin,ent->s.origin);
	gi.linkentity (ent);

}

///////////////////////////////////////////////////////////////////////
// Draws the current path (utility function)
///////////////////////////////////////////////////////////////////////
void ACEND_DrawPath(edict_t *self)
{
	int current_node, goal_node, next_node;

	current_node = show_path_from;
	goal_node = show_path_to;

	// RiEvEr - rewritten to use Ant system
	AntStartSearch( self, current_node, goal_node);

	next_node = SLLfront(&self->pathList);

	// Now set up and display the path
	while( current_node != goal_node && current_node != INVALID)
	{
		gi.WriteByte (svc_temp_entity);
		gi.WriteByte (TE_BFG_LASER);
		gi.WritePosition (nodes[current_node].origin);
		gi.WritePosition (nodes[next_node].origin);
		gi.multicast (nodes[current_node].origin, MULTICAST_PVS);
		current_node = next_node;
		SLLpop_front( &self->pathList);
		next_node = SLLfront(&self->pathList);
	}

/*
	next_node = path_table[current_node][goal_node];

	// Now set up and display the path
	while(current_node != goal_node && current_node != -1)
	{
		gi.WriteByte (svc_temp_entity);
		gi.WriteByte (TE_BFG_LASER);
		gi.WritePosition (nodes[current_node].origin);
		gi.WritePosition (nodes[next_node].origin);
		gi.multicast (nodes[current_node].origin, MULTICAST_PVS);
		current_node = next_node;
		next_node = path_table[current_node][goal_node];
	}*/
}

///////////////////////////////////////////////////////////////////////
// Turns on showing of the path, set goal to -1 to 
// shut off. (utility function)
///////////////////////////////////////////////////////////////////////
void ACEND_ShowPath(edict_t *self, int goal_node)
{
	show_path_from = ACEND_FindClosestReachableNode(self, NODE_DENSITY, NODE_ALL);
	show_path_to = goal_node;
}

///////////////////////////////////////////////////////////////////////
// Add a node of type ?
///////////////////////////////////////////////////////////////////////
int ACEND_AddNode(edict_t *self, int type)
{
	vec3_t v1,v2;
	int i;

	// Block if we exceed maximum
	if (numnodes + 1 > MAX_NODES)
		return false;
	
	// Set location
	VectorCopy(self->s.origin, nodes[numnodes].origin);
	nodes[numnodes].origin[2] += 8;

	// Set type
	nodes[numnodes].type = type;
	// Set number - RiEvEr
	nodes[numnodes].nodenum = numnodes;

	// Clear out the link information - RiEvEr
	for( i = 0; i< MAXLINKS; i++)
	{
		nodes[numnodes].links[i].targetNode = INVALID;
	}

	/////////////////////////////////////////////////////
	// ITEMS
	// Move the z location up just a bit.
	if(type == NODE_ITEM)
	{
		nodes[numnodes].origin[2] += 16;
		numitemnodes++;
	}

	// Teleporters
	if(type == NODE_TELEPORTER)
	{
		// Up 32
		nodes[numnodes].origin[2] += 32;
	}

	// Doors
	if(type == NODE_DOOR)
	{
		vec3_t	position;
		// Find mid point of door max and min and put the node there
		VectorClear(position);
        // find center of door
		position[0] = self->absmin[0] + ((self->maxs[0] - self->mins[0]) /2);
		position[1] = self->absmin[1] + ((self->maxs[1] - self->mins[1]) /2);
		position[2] = self->absmin[2] + 32;
		// We now have to create TWO nodes, one each side of the door
		// See if the 'door' is wider in the x or y direction
		if( (self->absmax[0] - self->absmin[0]) > (self->absmax[1] - self->absmin[1]) )
		{
			// Create in the 'y' direction

			// First node (Duplication deliberate!)
			// Set type
			nodes[numnodes].type = type;
			// Set number - RiEvEr
			nodes[numnodes].nodenum = numnodes;
			// Set position
			position[1] +=48;
			VectorCopy(position, nodes[numnodes].origin);

			// Second node
			numnodes++;
			// Set type
			nodes[numnodes].type = type;
			// Set number - RiEvEr
			nodes[numnodes].nodenum = numnodes;
			// Set position
			position[1] -=82;
			VectorCopy(position, nodes[numnodes].origin);
		}
		else
		{
			// Create in the 'x' direction

			// First node (Duplication deliberate!)
			// Set type
			nodes[numnodes].type = type;
			// Set number - RiEvEr
			nodes[numnodes].nodenum = numnodes;
			// Set position
			position[0] +=48;
			VectorCopy(position, nodes[numnodes].origin);

			// Second node
			numnodes++; 
			// Set type
			nodes[numnodes].type = type;
			// Set number - RiEvEr
			nodes[numnodes].nodenum = numnodes;
			// Set position
			position[0] -=82;
			VectorCopy(position, nodes[numnodes].origin);
		}
		numnodes++;
		return numnodes-1; // return the second node added

	}

	if(type == NODE_LADDER)
	{
		nodes[numnodes].type = NODE_LADDER;
				
		if(debug_mode)
		{
			debug_printf("Node added %d type: Ladder\n",numnodes);
			ACEND_ShowNode(numnodes);
		}
		
		numnodes++;
		return numnodes-1; // return the node added

	}

	// For platforms drop two nodes one at top, one at bottom
	if(type == NODE_PLATFORM)
	{
		VectorCopy(self->maxs,v1);
		VectorCopy(self->mins,v2);
		
		// To get the center
		nodes[numnodes].origin[0] = (v1[0] - v2[0]) / 2 + v2[0];
		nodes[numnodes].origin[1] = (v1[1] - v2[1]) / 2 + v2[1];
		nodes[numnodes].origin[2] = self->maxs[2];
			
		if(debug_mode)	
			ACEND_ShowNode(numnodes);
		
		numnodes++;

		nodes[numnodes].origin[0] = nodes[numnodes-1].origin[0];
		nodes[numnodes].origin[1] = nodes[numnodes-1].origin[1];
		nodes[numnodes].origin[2] = self->mins[2]+64;
		
		nodes[numnodes].type = NODE_PLATFORM;

		// Add a link 
		//RiEvEr modified to pass in calling entity
		ACEND_UpdateNodeEdge(self, numnodes, numnodes-1);			
		
		if(debug_mode)
		{
			debug_printf("Node added %d type: Platform\n",numnodes);
			ACEND_ShowNode(numnodes);
		}

		numnodes++;

		return numnodes -1;
	}
		
	if(debug_mode)
	{
		if(nodes[numnodes].type == NODE_MOVE)
			debug_printf("Node added %d type: Move\n",numnodes);
		else if(nodes[numnodes].type == NODE_TELEPORTER)
			debug_printf("Node added %d type: Teleporter\n",numnodes);
		else if(nodes[numnodes].type == NODE_ITEM)
			debug_printf("Node added %d type: Item\n",numnodes);
		else if(nodes[numnodes].type == NODE_WATER)
			debug_printf("Node added %d type: Water\n",numnodes);
		else if(nodes[numnodes].type == NODE_GRAPPLE)
			debug_printf("Node added %d type: Grapple\n",numnodes);

		ACEND_ShowNode(numnodes);
	}
		
	numnodes++;
	
	return numnodes-1; // return the node added
}

// RiEvEr
//=======================================
// ReverseLink
//=======================================
// Takes the path BACK to where we came from
// and tries to link the two nodes
// This helps make good path files
//
void ACEND_ReverseLink( edict_t *self, int from, int to )
{
	int	i;
	trace_t	trace;
	vec3_t	min,max;
	
	if(from == INVALID || to == INVALID || from == to)
		return; // safety

	// Need to trace from -> to and check heights
	// if from is much lower than to, forget it
	if( (nodes[from].origin[2]+32.0) < (nodes[to].origin[2]) )
	{
		// May not be able to jump that high so do not allow the return link
		return;
	}
//	VectorCopy(self->mins, min);
//	if( (nodes[from].origin[2]) < (nodes[2].origin[2]) )
//		min[2]  =0;	// Allow for steps etc.
//	VectorCopy(self->maxs, max);
//	if( (nodes[from].origin[2]) > (nodes[2].origin[2]) )
//		max[2] =0;	// Could be a downward sloping feature above our head
	VectorCopy( vec3_origin, min);
	VectorCopy( vec3_origin, max);



	// This should not be necessary, but I've heard that before!
	// Now trace it again
	trace = gi.trace( nodes[from].origin, min, max, nodes[to].origin, self, MASK_SOLID);
//	trace = gi.trace( nodes[from].origin, tv(-8,-8,0), tv(8,8,0), nodes[to].origin, self, MASK_SOLID);
	if( trace.fraction < 1.0)
	{
		// can't get there for some reason
		return;
	}
	// Add the link
	path_table[from][to] = to;

	// Checks if the link exists and then may create a new one - RiEvEr
	for( i=0; i<MAXLINKS; i++)
	{
		if ( nodes[from].links[i].targetNode == to)
			break;
		if ( nodes[from].links[i].targetNode == INVALID)
		{
			// RiEvEr
			// William uses a time factor here, whereas I use distance
			// His is possibly more efficient
			vec3_t	v;
			float thisCost;

			VectorSubtract(nodes[from].origin, nodes[to].origin, v); // subtract first
			thisCost = VectorLength(v);
			nodes[from].links[i].targetNode = to;
			nodes[from].links[i].cost = thisCost;
			if(debug_mode)
				debug_printf("ReverseLink %d -> %d\n", from, to);
			break;
		}
	}

	// Now for the self-referencing part, linear time for each link added
	for(i=0;i<numnodes;i++)
		if(path_table[i][from] != INVALID)
		{
			if(i == to)
				path_table[i][to] = INVALID; // make sure we terminate
			else
				path_table[i][to] = path_table[i][from];
		}
}
//R
	
///////////////////////////////////////////////////////////////////////
// Add/Update node connections (paths)
///////////////////////////////////////////////////////////////////////
void ACEND_UpdateNodeEdge(edict_t *self, int from, int to)
{
	int i;
	trace_t	trace;
	vec3_t	min,max;
	
	if(from == INVALID || to == INVALID || from == to)
		return; // safety

	// Try to stop bots creating impossible links!
	// If it looks higher than a jump...
	if(
		( nodes[to].origin[2] > nodes[from].origin[2]+36)
		&& self->is_bot
		)
	{
		// If we are coming from a move or jump node
		if( (nodes[from].type == NODE_MOVE) ||
			(nodes[from].type == NODE_JUMP)	)
		{
			// No if the to node is the same, it's illegal
			if( (nodes[to].type == NODE_MOVE) ||
				(nodes[to].type == NODE_JUMP)	)
			{
				// Too high - not possible!!
				return;
			}
		}
	}
	// Do not allow creation of nodes where the falling distance would kill you!
	if( (nodes[from].origin[2]) > (nodes[to].origin[2] + 180) )
		return;

/*	VectorCopy(self->mins, min);
	// If going up
//	if( (nodes[from].origin[2]) < (nodes[to].origin[2]) )
		min[2] = 0;	// Allow for steps up etc.
	VectorCopy(self->maxs, max);
	// If going down
//	if( (nodes[from].origin[2]) > (nodes[to].origin[2]) )
		max[2] = 0;	// door node linking*/
	VectorCopy( vec3_origin, min);
	VectorCopy( vec3_origin, max);

	// Now trace it - more safety stuff!
	trace = gi.trace( nodes[from].origin, min, max, nodes[to].origin, self, MASK_SOLID);

	if( trace.fraction < 1.0)
	{
		// can't do it
		if(debug_mode)
			debug_printf("Warning: (NoTrace) Failed to Link %d -> %d\n", from, to);
		return;
	}
	// Add the link
	path_table[from][to] = to;

	// Checks if the link exists and then may create a new one - RiEvEr
	for( i=0; i<MAXLINKS; i++)
	{
		if ( nodes[from].links[i].targetNode == to)
			break;
		if ( nodes[from].links[i].targetNode == INVALID)
		{
			// RiEvEr
			// William uses a time factor here, whereas I use distance
			// His is possibly more efficient
			vec3_t	v;
			float thisCost;

			VectorSubtract(nodes[from].origin, nodes[to].origin, v); // subtract first
			thisCost = VectorLength(v);
			nodes[from].links[i].targetNode = to;
			nodes[from].links[i].cost = thisCost;
			if(debug_mode)
				debug_printf("Link %d -> %d\n", from, to);
			break;
		}
	}

	// Now for the self-referencing part, linear time for each link added
	for(i=0;i<numnodes;i++)
		if(path_table[i][from] != INVALID)
		{
			if(i == to)
				path_table[i][to] = INVALID; // make sure we terminate
			else
				path_table[i][to] = path_table[i][from];
		}
        
	// RiEvEr - check for the link going back the other way
	// Reverse the input data so it works properly!
	ACEND_ReverseLink( self, to, from );
	// R
}

///////////////////////////////////////////////////////////////////////
// Remove a node edge
///////////////////////////////////////////////////////////////////////
void ACEND_RemoveNodeEdge(edict_t *self, int from, int to)
{
	int i;

	if(debug_mode) 
		debug_printf("%s: Removing Edge %d -> %d\n", self->client->pers.netname, from, to);
		
	path_table[from][to] = INVALID; // set to invalid

	// RiEvEr
	// Now we must remove the link from the antpath system
	// Get the link information
	for( i=0; i<MAXLINKS; i++)
	{
		// If it matches our link remove it
		if ( nodes[from].links[i].targetNode == to)
			break;
	}
	for( ; i<MAXLINKS; i++)
	{
		if( i == MAXLINKS-1)
		{
			nodes[from].links[i].targetNode = INVALID;
			nodes[from].links[i].cost = INVALID;
		}
		// Move all the other info down to fill the gap.
		if(nodes[from].links[i].targetNode != INVALID)
		{
			nodes[from].links[i].targetNode = nodes[from].links[i+1].targetNode;
			nodes[from].links[i].cost = nodes[from].links[i+1].cost;
		}
	}

	//R

	// Make sure this gets updated in our path array
	for(i=0;i<numnodes;i++)
		if(path_table[from][i] == to)
			path_table[from][i] = INVALID;
}

///////////////////////////////////////////////////////////////////////
// This function will resolve all paths that are incomplete
// usually called before saving to disk
///////////////////////////////////////////////////////////////////////
void ACEND_ResolveAllPaths()
{
	int i, from, to;
	int num=0;

//	return;	// RiEvEr - disabled since it will interfere with the optimiser

	gi.bprintf(PRINT_HIGH,"Resolving all paths...");

	for(from=0;from<numnodes;from++)
	{
		for(to=0;to<numnodes;to++)
		{
			// update unresolved paths
			// Not equal to itself, not equal to -1 and equal to the last link
			if(from != to && path_table[from][to] == to)
			{
				num++;

				// Now for the self-referencing part linear time for each link added
				for(i=0;i<numnodes;i++)
					if(path_table[i][from] != -1)
					{
						if(i == to)
							path_table[i][to] = -1; // make sure we terminate
						else
							path_table[i][to] = path_table[i][from];
					}
			}
		}
	}

	gi.bprintf(PRINT_MEDIUM,"done (%d updated)\n",num);
}

///////////////////////////////////////////////////////////////////////
// Save to disk file
//
// Since my compression routines are one thing I did not want to
// release, I took out the compressed format option. Most levels will
// save out to a node file around 50-200k, so compression is not really
// a big deal.
///////////////////////////////////////////////////////////////////////
void ACEND_SaveNodes()
{
	FILE *pOut;
	char filename[60];
	int i,j;
	int version;
	cvar_t	*game_dir;

	version = LTK_NODEVERSION;

	game_dir = gi.cvar ("game", "action", 0);

	//@@ change 'nav' to 'terrain' to line up with William
#ifdef	_WIN32
	i =  sprintf(filename, ".\\");
	i += sprintf(filename + i, game_dir->string);
	i += sprintf(filename + i, "\\terrain\\");
	i += sprintf(filename + i, level.mapname);
	i += sprintf(filename + i, ".ltk");
#else
	strcpy(filename, "./");
	strcat(filename, game_dir->string);
	strcat(filename, "/terrain/");
	strcat(filename,level.mapname);
	strcat(filename,".ltk");
#endif
	
	// Resolve paths
	ACEND_ResolveAllPaths();

	gi.bprintf(PRINT_MEDIUM,"Saving node table...");

/*	strcpy(filename,"action\\nav\\");
	strcat(filename,level.mapname);
	strcat(filename,".nod");*/

	if((pOut = fopen(filename, "wb" )) == NULL)
		return; // bail
	
	fwrite(&version,sizeof(int),1,pOut); // write version
	fwrite(&numnodes,sizeof(int),1,pOut); // write count
	fwrite(&num_items,sizeof(int),1,pOut); // write facts count
	
	fwrite(nodes,sizeof(node_t),numnodes,pOut); // write nodes
	
	for(i=0;i<numnodes;i++)
		for(j=0;j<numnodes;j++)
			fwrite(&path_table[i][j],sizeof(short int),1,pOut); // write count
		
	//fwrite(item_table,sizeof(item_table_t),num_items,pOut); 		// write out the fact table

	// Write out the fact table with sanitized pointers.
	for( i = 0; i < num_items; i ++ )
	{
		item_table_t item;
		memcpy( &item, &(item_table[i]), sizeof(item) );
		item.ent = NULL;
		fwrite( &item, sizeof(item), 1, pOut );
	}

	fclose(pOut);
	
	gi.bprintf(PRINT_MEDIUM,"done.\n");
}

///////////////////////////////////////////////////////////////////////
// Read from disk file
///////////////////////////////////////////////////////////////////////
void ACEND_LoadNodes(void)
{
	FILE *pIn;
	int i,j;
	char filename[60];
	int version;
	cvar_t	*game_dir;

	game_dir = gi.cvar ("game", "action", 0);

#ifdef	_WIN32
	i =  sprintf(filename, ".\\");
	i += sprintf(filename + i, game_dir->string);
	i += sprintf(filename + i, "\\terrain\\");
	i += sprintf(filename + i, level.mapname);
	i += sprintf(filename + i, ".ltk");
#else
	strcpy(filename, "./");
	strcat(filename, game_dir->string);
	strcat(filename, "/terrain/");
	strcat(filename,level.mapname);
	strcat(filename,".ltk");
#endif
/*
	strcpy(filename,"action\\nav\\");
	strcat(filename,level.mapname);
	strcat(filename,".nod");*/

	if((pIn = fopen(filename, "rb" )) == NULL)
    {
		// Create item table
		gi.bprintf(PRINT_MEDIUM, "ACE: No node file found, creating new one...");
		ACEIT_BuildItemNodeTable(false);
		gi.bprintf(PRINT_MEDIUM, "done.\n");
		return; 
	}

	// determin version
	fread(&version,sizeof(int),1,pIn); // read version
	
	if(version == LTK_NODEVERSION) 
	{
		gi.bprintf(PRINT_MEDIUM,"ACE: Loading node table...");

		fread(&numnodes,sizeof(int),1,pIn); // read count
		fread(&num_items,sizeof(int),1,pIn); // read facts count
		
		fread(nodes,sizeof(node_t),numnodes,pIn);

		for(i=0;i<numnodes;i++)
			for(j=0;j<numnodes;j++)
				fread(&path_table[i][j],sizeof(short int),1,pIn); // write count
	
		fread(item_table,sizeof(item_table_t),num_items,pIn);
		fclose(pIn);

		// Raptor007: Do not trust saved pointers!
		for(i=0;i<MAX_EDICTS;i++)
			item_table[i].ent = NULL;
	}
	else
	{
		// Create item table
		gi.bprintf(PRINT_MEDIUM, "ACE: No node file found, creating new one...");
		ACEIT_BuildItemNodeTable(false);
		gi.bprintf(PRINT_MEDIUM, "done.\n");
		return; // bail
	}
	
	gi.bprintf(PRINT_MEDIUM, "done.\n");
	
	ACEIT_BuildItemNodeTable(true);

}

