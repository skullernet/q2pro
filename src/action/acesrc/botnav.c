//-----------------------------------------------------------------------------
//
//  $Header:$
//
// Copyright (C) 2000 by Connor "RiEvEr" Caple
// All rights reserved.
//
// This file contains the searchpath algorithm files for use by the bots
// The code in this file, or variants of it, may be used in any non-commercial 
// FPS mod as long as you credit me as the author.
//
// Commercial permission can be obtained from me via my current e-mail
// address. (connor@botgod.org.uk as of February 2000)
//
//-----------------------------------------------------------------------------

/*
 * $History:$
 * 
 */

#include "../g_local.h"

#include "botnav.h"

//== GLOBAL SEMAPHORE ==
int	antSearch;

// The nodes array
extern	node_t		nodes[MAX_NODES];
extern short	path_table[MAX_NODES][MAX_NODES]; // Quick pathsearch array [from][to]

qboolean		nodeused[MAX_NODES]; // This is used for a FAST check if the node has been used
short int	nodefrom[MAX_NODES]; // Stores how we got here once the node is closed

/* =========================================================
The basic system works by using a single linked list and accessing information from the node array

1) The current node is found
2) All links from it are propogated - if not already done by another node
3) If we haven't found the target node then we get the next open node and go to 1
4) If we have the target node we return the path to it
5) If we run out of nodes then we return INVALID

This idea is based on "Path Finding Via 'Ant Races'  (a floodfill algorithm)" 
by Richard Wesson. It is easy to optimise this code to make it even more efficient.

============================================================= */


//=========================
// Init the search path variables
//=========================
void	AntInitSearch(edict_t *ent)
{
	//Make sure the lists and arrays used are all set to the correct values and EMPTY!
	memset(nodeused, 0, sizeof(nodeused) );
	memset(nodefrom, INVALID, sizeof(nodefrom) );
	while( !SLLempty(&ent->pathList) )
	{
		SLLpop_front(&ent->pathList);
	}
}

//=========================
// StartSearch
//=========================
//
// returns true if a path is found
// false otherwise
//
qboolean	AntStartSearch(edict_t *ent, int from, int to )
{
	// Safety first!
	if( from==INVALID || to==INVALID)
		return false;

	//@@ TESTING ONLY!! antSearch always available
	antSearch = 1;
	// Check we're allowed to search - if so, do it
	if(1)// (antSearch > 0) && (ent->antLastCallTime < level.framenum - ANT_FREQ) )
	{
		// Decrement the semaphore to limit calls to this function
		//@@ If we ever get multithreading then we can increment later
		antSearch--;
		// make a note of when this bot last made a path call
		ent->antLastCallTime = level.framenum;
		// Set up the lists
		AntInitSearch(ent);
		// If we found a path
		if( AntFindPath( ent, from, to) )
		{
			// pathList now contains the links in reverse order
			return true;
		}
	}
	// We can use the quick node search method here to get our path and put it in the pathList
	// the same way we do with the AntSearch mode. This will have the side effect of finding
	// bad paths and removing them.
	if( AntQuickPath(ent, from, to) )
	{
		return true;
	}
	// If not allowed to search and no path
	AntInitSearch(ent);	// Clear out the path storage
	return false;
}

//=================================
// QuickPath
//=================================
//
// Uses the old path array to get a quick answer and removes bad paths
//

qboolean	AntQuickPath(edict_t *ent, int from, int to)
{
	int	newNode = from;
	int	oldNode = 0;

	// Clean out the arrays, etc.
	AntInitSearch(ent);
	nodeused[from] = true;
	// Check we can get from->to and that the path is complete
	while( newNode != INVALID )
	{
		oldNode = newNode;
		// get next node
		newNode = path_table[newNode][to];
		if( newNode == to )
		{
			// We're there - store it then build the path
			nodeused[newNode] = true;
			nodefrom[newNode] = oldNode;
			break;
		}
		else if( newNode == INVALID )
		{
			// We have a bad path
			break;
		}
		else if( !nodeused[newNode] )
		{
			// Not been here yet - store it!
			nodeused[newNode] = true;
			nodefrom[newNode] = oldNode;
		}
		else
			break; // LOOP encountered
	}

	// If successful, build the pathList
	if( newNode == to )
	{
		SLLpush_front(&ent->pathList, to );
		while( newNode != from)
		{
			// Push the 
			SLLpush_front(&ent->pathList, nodefrom[newNode]);
			newNode = nodefrom[newNode];
		}
		return true;
	}
	// else wipe out the bad path!
	else
	{
		newNode = oldNode;
		while( newNode != from)
		{
			path_table[ nodefrom[newNode] ][ to ] = INVALID;
		}
		path_table[ from ][ to ] = INVALID;
	}
	return false;
}

//=======================
// FindPath
//=======================
//
// Uses OPEN and CLOSED lists to conduct a search
// Many refinements planned
//
qboolean	AntFindPath( edict_t *ent, int from, int to)
{
	int counter = 0;
	int	newNode = INVALID; // Stores the node being tested
	node_t	*tempNode = NULL; // Pointer to a real NODE
	int workingNode,atNode; // Structures for search

	// Locally declared OPEN list
	ltklist_t	openList;
	openList.head = openList.tail = NULL; // MUST do this!!

	// Safety first again - we don't want crashes!
	if( from==INVALID || to==INVALID )
		return false;

	// Put startnode on the OPEN list
	atNode = from;
	nodefrom[atNode] = INVALID;
	SLLpush_back(&openList, from );
	nodeused[from] = true;
	
	// While there are nodes on the OPEN list AND we are not at destNode
	while( !SLLempty(&openList) && newNode != to )
	{
		counter = 0;

		// Where we are
		atNode = SLLfront(&openList);

		// Safety check
		if( atNode <= INVALID )
			return false;

		// Get a pointer to all the node information
		tempNode = &nodes[atNode];
		// Using an array for FAST access to the path ratrher than a CLOSED list
		newNode = tempNode->links[counter].targetNode;

		// Process this node putting linked nodes on the OPEN list
		while( newNode != INVALID)
		{
			// If newNode NOT on open or closed list
			if( !nodeused[newNode])
			{
				// Mark node as used
				nodeused[newNode] = true;
				// Set up working node for storage on OPEN list
				workingNode = newNode;
				nodefrom[newNode] = atNode;
				// Store it
				SLLpush_back(&openList, workingNode );
			}
			// If node being linked is destNode then quit
			if( newNode == to)
			{
				break;
			}
			// Check we aren't trying to read out of range
			if( ++counter >= MAXLINKS )
				break;
			else
				newNode = tempNode->links[counter].targetNode;
		}

		// ... and remove atNode from the OPEN List
		SLLpop_front(&openList);
	}	// END While

	// Free up the memory we allocated
	SLLdelete (&openList);

	// Optimise stored path with this new information
	if( newNode == to)
	{
		// Make the path using the fromnode array pushing node numbers on in reverse order
		// so we can SLLpop_front them back later
		SLLpush_front(&ent->pathList, newNode );
		// Set the to path in node array because this is shortest path
		path_table[ nodefrom[to] ][ to ] = to;
		// We earlier set our start node to INVALID to set up the termination
		while( 
			(newNode=nodefrom[newNode])!=INVALID // there is a path and
			&& (newNode != from)	// it's not the node we're standing on (safety check)
			)
		{
			// Push it onto the pathlist
			SLLpush_front(&ent->pathList, newNode );
			// Set the path in the node array to match this shortest path
			path_table[ nodefrom[newNode] ][ to ] = newNode;
		}
		return true;
	}
	// else
	return false;
}

//=============================
// LinkExists
//=============================
//
// Used to check we haven't wandered off path!
//
qboolean	AntLinkExists( int from, int to)
{
	int counter =0;
	int	testnode;
	node_t	*tempNode = &nodes[from];

	if( from==INVALID || to==INVALID )
		return false;
	// Check if the link exists
	while( counter < MAXLINKS)
	{
		testnode = tempNode->links[counter].targetNode;
		if( testnode == to)
		{
			// A path exists from,to 
			return true;
		}
		else if( testnode == INVALID )
		{
			// No more links and no path found
			return false;
		}
		counter++;
	}
	// Didn't find it!
	return false;
}

// ****************************************************** //
//=======================================================
// SLL functions used by search code
//=======================================================	

//==============================
// SLLpush_front
//==============================
// Add to the front of the list
//
void		SLLpush_front( ltklist_t *thelist, int nodedata )
{
	slint_t	*temp;

	// Store the current head pointer
	temp = thelist->head;
	// allocate memory for the new data (LEVEL tagged)
	thelist->head = gi.TagMalloc( sizeof(slint_t), TAG_LEVEL);
	// Set up the data and pointer
	thelist->head->nodedata = nodedata;
	thelist->head->next = temp;
	// Check if there;'s a next item
	if( !thelist->head->next)
	{
		// Set the tail pointer = head
		thelist->tail = thelist->head;
	}
}

//==============================
// SLLpop_front
//==============================
// Remove the iten from the front of the list
//
void		SLLpop_front( ltklist_t *thelist )	
{
	slint_t *temp;

	// Store the head pointer
	temp = thelist->head;
	// Check if there's a next item
	if( thelist && thelist->head )
	{
		if( thelist->head == thelist->tail )
		{
			// List is now emptying
			thelist->tail = thelist->head = NULL;
		}
		else
		{
			// Move head to point to next item
			thelist->head = thelist->head->next;
		}
		// Free the memory (LEVEL tagged)
		gi.TagFree( temp );		
	}
	else
	{
		gi.bprintf( PRINT_HIGH, "Attempting to POP an empty list!\n");
	}
}

//==============================
// SLLfront
//==============================
// Get the integer value from the front of the list
// without removing the item (Query the list)
//
int			SLLfront( ltklist_t *thelist )		
{
	if( thelist && !SLLempty( thelist) )
		return( thelist->head->nodedata);
	else
		return INVALID;
}
												
//==============================
// SLLpush_front
//==============================
// Add to the back of the list
//
void		SLLpush_back( ltklist_t *thelist, int nodedata )
{
	slint_t	*temp;

	// Allocate memory for the new item (LEVEL tagged)
	temp = (slint_t *)gi.TagMalloc( sizeof(slint_t), TAG_LEVEL);
	// Store the data
	temp->nodedata = nodedata;
	temp->next = NULL;		// End of the list
	// Store the new item in the list
	// Is the list empty?
	if( !thelist->head )
	{
		// Yes - add as a new item
		thelist->head = temp;
		thelist->tail = temp;
	}
	else
	{
		// No make this the new tail item
		thelist->tail->next = temp;
		thelist->tail = temp;
	}
}

//==============================
// SLLempty
//==============================
// See if the list is empty (false if not empty)
//
qboolean	SLLempty( ltklist_t *thelist )		
{
	// If there is any item in the list then it is NOT empty...
	// If there is a list
	if(thelist)
		return (thelist->head == NULL);
	else	// No list so return empty
		return (true);
}

//===============================
// Delete the list
//===============================
// Avoids memory leaks
//
void	SLLdelete( ltklist_t *thelist )
{
	slint_t	*temp;

	while( !SLLempty( thelist ))
	{
		temp = thelist->head;
		thelist->head = thelist->head->next;
		gi.TagFree( temp );
	}
}


