#ifndef BOTNAV_H
#define BOTNAV_H
//-----------------------------------------------------------------------------
//
//  $Logfile:: /LicenseToKill/src/acesrc/botnav.h                             $
// $Revision:: 6                                                              $
//   $Author:: Riever                                                         $
//     $Date:: 1/10/99 18:20                                                  $
//
// Copyright (C) 1999 by Connor Caple
// All rights reserved.
//-----------------------------------------------------------------------------
/*
 * $Log: /LicenseToKill/src/acesrc/botnav.h $
 * 
 * 6     1/10/99 18:20 Riever
 * Reduced intensive searches to once every 0.5 secs.
 * 
 * 5     27/09/99 14:33 Riever
 * Added SLLdelete function to free up memory and help prevent any leaks.
 * There may still be a necessity to free up bot pathLists at respawn -
 * awaiting views from those who know how to spot memory leaks!
 * 
 * 4     25/09/99 11:20 Riever
 * Tidied up errors in declarations
 * 
 * 3     25/09/99 9:36 Riever
 * Added all SLL definitions
 * 
 * 2     25/09/99 8:24 Riever
 * 
 */

#define ANT_FREQ	0.5	// Time gap between calls to the processor intensive search

	  // ----------- new Pathing Algorithm stuff -----
	  qboolean	AntPathMove( edict_t *ent );				// Called in item and enemy route functions
	  void		AntInitSearch( edict_t *ent );				// Resets all the path lists etc.
	  qboolean	AntStartSearch( edict_t *ent, int from, int to);	// main entry to path algorithms
	  qboolean	AntQuickPath( edict_t *ent, int from, int to );	// backup path system
	  qboolean	AntFindPath( edict_t *ent, int from, int to);		// Optimised path system
	  qboolean	AntLinkExists( int from, int to);	// Detects if we are off the path

// --------- AI Tactics Values -------------
enum{
		AIRoam,			// Basic item collection AI
		AIAttack,		// Basic Attack Enemy AI
		AIAttackCollect,// Attack Enemy while collecting Item
		AICamp,			// Camp at a suitable location and collect the item on respawn
		AISnipe,
		AIAmbush
};

// ** Single Linked List (SLL) implementation **

// ** slint_t is a list member ie: one item on the list
typedef struct slint{
	struct slint	*next;	// pointer to the next list member
	int				nodedata;	// The node number we're storing
} slint_t;

// ** ltklist_t is the actual list and contains a number of slint_t members
// It is a double-ended singly linked list (ie, we can add things to the front or back)
// We need head and tail pointers to speed up push_back operations
typedef struct{
	slint_t		*head;	// Front of the list
	slint_t		*tail;	// Back of the list
}	ltklist_t;

// Now we have to define the operations that can happen on the list
// All will be prefixed with "SLL" so we know what they are working on when we read the code

void		SLLpush_front( ltklist_t *thelist, int nodedata );// Add to the front of the list
void		SLLpop_front( ltklist_t *thelist );	// Remove the iten from the front of the list
int			SLLfront( ltklist_t *thelist );		// Get the integer value from the front of the list..
												// ..without removing the item (Query the list)
void		SLLpush_back( ltklist_t *thelist, int nodedata );	// Add to the back of the list
qboolean	SLLempty( ltklist_t *thelist );		// See if the list is empty (false if not empty)
void		SLLdelete( ltklist_t *thelist );	// Free all memory from a list

#endif
