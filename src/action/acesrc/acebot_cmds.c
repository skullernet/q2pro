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
 * $Header: /LTK2/src/acesrc/acebot_cmds.c 2     23/02/00 17:24 Riever $
 *
 * $History: acebot_cmds.c $
 * 
 * *****************  Version 2  *****************
 * User: Riever       Date: 23/02/00   Time: 17:24
 * Updated in $/LTK2/src/acesrc
 * Added support for 'sv shownodes on/off'
 * Enabled creation of nodes for ALL doors. (Stage 1 of new method)
 * 
 */
	
///////////////////////////////////////////////////////////////////////
//
//  acebot_cmds.c - Main internal command processor
//
///////////////////////////////////////////////////////////////////////

#include "../g_local.h"


qboolean debug_mode=false;
qboolean shownodes_mode=false;	//RiEvEr - new node showing method

///////////////////////////////////////////////////////////////////////
// Special command processor
///////////////////////////////////////////////////////////////////////
qboolean ACECM_Commands(edict_t *ent)
{
	char	*cmd;
	int node;

	cmd = gi.argv(0);

	if(Q_stricmp (cmd, "addnode") == 0 && debug_mode)
		ent->last_node = ACEND_AddNode(ent,atoi(gi.argv(1))); 
	
	else if(Q_stricmp (cmd, "removelink") == 0 && debug_mode)
		ACEND_RemoveNodeEdge(ent,atoi(gi.argv(1)), atoi(gi.argv(2)));

	else if(Q_stricmp (cmd, "addlink") == 0 && debug_mode)
		ACEND_UpdateNodeEdge(ent, atoi(gi.argv(1)), atoi(gi.argv(2)));
	
	else if(Q_stricmp (cmd, "showpath") == 0 && debug_mode)
		ACEND_ShowPath(ent,atoi(gi.argv(1)));

	else if(Q_stricmp (cmd, "shownode") == 0 && debug_mode)
		ACEND_ShowNode(atoi(gi.argv(1)));

	else if(Q_stricmp (cmd, "findnode") == 0 && debug_mode)
	{
		node = (gi.argc() >= 2) ? atoi(gi.argv(1)) : ACEND_FindClosestReachableNode(ent,NODE_DENSITY, NODE_ALL);
		gi.bprintf(PRINT_MEDIUM,"node: %d type: %d x: %f y: %f z %f\n",node,nodes[node].type,nodes[node].origin[0],nodes[node].origin[1],nodes[node].origin[2]);
	}

	else if(Q_stricmp (cmd, "movenode") == 0 && debug_mode)
	{
		node = (gi.argc() >= 2) ? atoi(gi.argv(1)) : (numnodes - 1);

		if( gi.argc() >= 5 )
		{
			nodes[node].origin[0] = atof(gi.argv(2));
			nodes[node].origin[1] = atof(gi.argv(3));
			nodes[node].origin[2] = atof(gi.argv(4));
		}
		else
		{
			VectorCopy( ent->s.origin, nodes[node].origin );
			nodes[node].origin[2] += 8;
		}

		gi.bprintf(PRINT_MEDIUM,"node: %d moved to x: %f y: %f z %f\n",node, nodes[node].origin[0],nodes[node].origin[1],nodes[node].origin[2]);
	}

	else if(Q_stricmp (cmd, "nodetype") == 0 && debug_mode)
	{
		node = atoi(gi.argv(1));

		if( gi.argc() >= 3 )
			nodes[node].type = atoi(gi.argv(2));

		gi.bprintf(PRINT_MEDIUM,"node %d type %d\n", node, nodes[node].type);
	}

	else if(Q_stricmp (cmd, "showlinks") == 0 && debug_mode)
	{
		int i;
		node = atoi(gi.argv(1));
		for( i = 0; i < MAXLINKS; i ++ )
		{
			int target = nodes[node].links[i].targetNode;
			if( target != INVALID )
				gi.bprintf( PRINT_MEDIUM, "link: %d -> %d\n", node, nodes[node].links[i].targetNode );
		}
	}

	else if(Q_stricmp (cmd, "showallnodes") == 0 && debug_mode)
	{
		int i;
		for( i = 1; i < numnodes; i ++ )
			ACEND_ShowNode(i);
	}

	else if(Q_stricmp (cmd, "botgoal") == 0 && debug_mode)
	{
		int i;
		node = (gi.argc() >= 2) ? atoi(gi.argv(1)) : ACEND_FindClosestReachableNode( ent, NODE_DENSITY, NODE_ALL );
		for( i = 1; i <= game.maxclients; i ++ )
		{
			edict_t *ent = &(g_edicts[ i ]);
			if( ent->inuse && ent->is_bot && IS_ALIVE(ent) )
			{
				AntInitSearch( ent );
				ACEND_SetGoal( ent, node );
				ent->state = STATE_MOVE;
				ent->node_timeout = 0;
			}
		}
	}

	else
		return false;

	return true;
}


///////////////////////////////////////////////////////////////////////
// Called when the level changes, store maps and bots (disconnected)
///////////////////////////////////////////////////////////////////////
void ACECM_Store()
{
	// Stop overwriting good node tables with bad!
	if( numnodes < 100 )
		return;

	ACEND_SaveNodes();
}

///////////////////////////////////////////////////////////////////////
// These routines are bot safe print routines, all id code needs to be 
// changed to these so the bots do not blow up on messages sent to them. 
// Do a find and replace on all code that matches the below criteria. 
//
// (Got the basic idea from Ridah)
//	
//  change: gi.cprintf to safe_cprintf
//  change: gi.centerprintf to safe_centerprintf
// 
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Debug print, could add a "logging" feature to print to a file
///////////////////////////////////////////////////////////////////////
void debug_printf(char *fmt, ...)
{
	int     i;
	char	bigbuffer[0x10000];
	//int	len;
	va_list	argptr;
	edict_t	*cl_ent;
	
	va_start (argptr,fmt);
	//len = vsprintf (bigbuffer,fmt,argptr);
	vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_MEDIUM, "%s", bigbuffer);

	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = g_edicts + 1 + i;
		if (!cl_ent->inuse || cl_ent->is_bot)
			continue;

		gi.cprintf(cl_ent, PRINT_MEDIUM, "%s", bigbuffer);
	}

}

// void (*real_cprintf) (edict_t * ent, int printlevel, char *fmt, ...) = NULL;
// void (*real_centerprintf) (edict_t * ent, char *fmt, ...) = NULL;
void (*real_cprintf) (struct edict_s * ent, int printlevel, const char *fmt, ...) = NULL;
void (*real_centerprintf) (struct edict_s * ent, const char *fmt, ...) = NULL;

///////////////////////////////////////////////////////////////////////
// botsafe cprintf
///////////////////////////////////////////////////////////////////////
void safe_cprintf (edict_t *ent, int printlevel, char *fmt, ...)
{
	char	bigbuffer[0x10000];
	va_list		argptr;
	//int len;

	if (ent && (!ent->inuse || ent->is_bot))
		return;

	va_start (argptr,fmt);
	//len = vsprintf (bigbuffer,fmt,argptr);
	vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);

	real_cprintf(ent, printlevel, "%s", bigbuffer);
	
}

///////////////////////////////////////////////////////////////////////
// botsafe centerprintf
///////////////////////////////////////////////////////////////////////
void safe_centerprintf (edict_t *ent, char *fmt, ...)
{
	char	bigbuffer[0x10000];
	va_list		argptr;
	//int len;

	if (!ent->inuse || ent->is_bot)
		return;
	
	va_start (argptr,fmt);
	//len = vsprintf (bigbuffer,fmt,argptr);
	vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);
	
	real_centerprintf(ent, "%s", bigbuffer);
	
}
