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
 * $Header: /LTK2/src/acesrc/acebot.h 9     29/02/00 11:20 Riever $
 *
 * $Log: /LTK2/src/acesrc/acebot.h $
 * 
 * 9     29/02/00 11:20 Riever
 * ChooseWeapon changed to qboolean
 * 
 * 8     27/02/00 13:07 Riever
 * Changed enums to defines for better compatibility.
 * 
 * 7     24/02/00 3:07 Riever
 * BOTUT_Cmd_Say_f proto added
 * 
 * 6     23/02/00 17:24 Riever
 * Added support for 'sv shownodes on/off'
 * Enabled creation of nodes for ALL doors. (Stage 1 of new method)
 * 
 * 5     21/02/00 23:45 Riever
 * Added GT_ goal selection support and ROAM code protos. Altered movement
 * trace lengths to be shorter.
 * 
 * 4     21/02/00 15:16 Riever
 * Bot now has the ability to roam on dry land. Basic collision functions
 * written. Active state skeletal implementation.
 * 
 * 3     20/02/00 20:27 Riever
 * Added new members and definitions ready for 2nd generation of bots.
 * 
 * 2     17/02/00 17:53 Riever
 * Fixed item list to be in the right order!
 * 
 */

///////////////////////////////////////////////////////////////////////	
//
//  acebot.h - Main header file for ACEBOT
// 
// 
///////////////////////////////////////////////////////////////////////

#ifndef _ACEBOT_H
#define _ACEBOT_H

// Bots think at the server framerate to make sure they move smoothly.
#define BOT_FPS (game.framerate)

// Only 100 allowed for now (probably never be enough edicts for 'em)
#define MAX_BOTS 100

// Platform states
#define	STATE_TOP			0
#define	STATE_BOTTOM		1
#define STATE_UP			2
#define STATE_DOWN			3

// Maximum nodes
#define MAX_NODES 1200

// Link types
#define INVALID -1

// Node types
#define NODE_MOVE 0
#define NODE_LADDER 1
#define NODE_PLATFORM 2
#define NODE_TELEPORTER 3
#define NODE_ITEM 4
#define NODE_WATER 5
#define NODE_GRAPPLE 6
#define NODE_JUMP 7
#define NODE_DOOR 8	// - RiEvEr
#define NODE_ALL 99 // For selecting all nodes

// Density setting for nodes
#define NODE_DENSITY 96

// Maximum links per node
#define MAXLINKS 12

//AQ2 ADD
extern cvar_t	*ltk_skill;	// Skill setting for bots, range 0-10	
extern cvar_t	*ltk_showpath;	// Toggles display of bot paths in debug mode
extern cvar_t	*ltk_chat;	// Chat setting for bots, off or on (0,1)
extern cvar_t	*ltk_routing;	// Set to 1 to drop nodes, otherwise you won't do it!
extern cvar_t	*ltk_botfile;	// Set this to adjust which botdata file to load, default is botdata
extern cvar_t	*ltk_loadbots;	// Set to 0 to not load bots from ltk_botfile value, 1 for normal operation
extern cvar_t	*ltk_showbots;  // Set to 1 to have bots show up as normal players in server listings

extern int lights_camera_action;

//AQ2 END

// Bot state types
#define STATE_STAND 0
#define STATE_MOVE 1
#define STATE_ATTACK 2
#define STATE_WANDER 3
#define STATE_FLEE 4
#define STATE_POSITION 5
#define STATE_COVER 6

// New state definitions
#define	BS_WAIT		0
#define	BS_DEAD		1
#define	BS_ROAM		2
#define	BS_PASSIVE	3
#define	BS_ACTIVE	4
#define	BS_SECURE	5
#define	BS_RETREAT	6
#define	BS_HOLD		7
#define	BS_SUPPORT	8

// Secondary states
#define	BSS_NONE		0
#define	BSS_POSITION	1
#define	BSS_COLLECT		2
#define	BSS_SEEKENEMY	4
#define	BSS_ATTACK		8


// Goal Types (Extensible)
#define	GT_NONE			0
#define	GT_POSITION		1
#define	GT_ENEMY		2
#define	GT_ITEM			3


#define MOVE_LEFT 0
#define MOVE_RIGHT 1
#define MOVE_FORWARD 2
#define MOVE_BACK 3

// Used in the BOTCOL functions
#define		TRACE_DOWN		128
#define		TRACE_DOWN_STRAFE	32 // don't go off ledges when strafing!
#define		TRACE_DIST		48 //dropped from 256
#define		TRACE_DIST_STRAFE	24 // watch that edge!
#define		TRACE_DIST_SHORT	32 // for forwards motion
#define		TRACE_DIST_LADDER	24
#define		TRACE_DIST_JUMP		128 // to jump over gaps
#define		TJUMP_DIST		40  //just enough to stand on
#define		TWATER_DIST		8	// For getting to edge in water
#define		TWEDGE_DIST		16 // ledge width required to leave water
#define		TCRAWL_DIST		32 // not worth crawling otherwise	
#define		TMOVE_DIST		16 // wall detection
#define		MASK_DEADLY		(CONTENTS_LAVA|CONTENTS_SLIME) // ouch!

// Movement speeds
#define	SPEED_CAREFUL	10
#define	SPEED_ROAM		100
#define SPEED_WALK      200
#define SPEED_RUN       400

#define EYES_FREQ       0.2     // Every n seconds the bot's eyes will be checked
#define ROOT2           1.41421 // Square root of 2 (i.e. ROOT2^2 = 2)
#define COS90           -0.34202
#define STRIDESIZE		24

#define VEC_ORIGIN tv(0,0,0)


typedef struct nodelink_s
{
	short int		targetNode;
	float	cost; // updated for pathsearch algorithm

}nodelink_t; // RiEvEr


// Node structure
typedef struct node_s
{
	vec3_t origin; // Using Id's representation
	int type;   // type of node
	short int nodenum;	// node number - RiEvEr
//	short int lightlevel;	// obvious... - RiEvEr
	nodelink_t	links[MAXLINKS];	// store all links. - RiEvEr

} node_t;

typedef struct item_table_s
{
	int item;
	float weight;
	edict_t *ent;
	int node;

} item_table_t;

extern int num_players;
extern edict_t *players[MAX_CLIENTS];		// pointers to all players in the game

// extern decs
extern node_t nodes[MAX_NODES]; 
extern item_table_t item_table[MAX_EDICTS];
extern qboolean debug_mode;
extern qboolean shownodes_mode;	// RiEvEr - for the new command "sv shownodes on/off"
extern int numnodes;
extern int num_items;

// id Function Protos I need
void     LookAtKiller (edict_t *self, edict_t *inflictor, edict_t *attacker);
void     ClientObituary (edict_t *self, edict_t *inflictor, edict_t *attacker);
void     TossClientWeapon (edict_t *self);
void     ClientThink (edict_t *ent, usercmd_t *ucmd);
void     SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles);
void     ClientUserinfoChanged (edict_t *ent, char *userinfo);
void     CopyToBodyQue (edict_t *ent);
qboolean ClientConnect (edict_t *ent, char *userinfo);
void     Use_Plat (edict_t *ent, edict_t *other, edict_t *activator);

// acebot_ai.c protos
void     ACEAI_Think (edict_t *self);
void     ACEAI_PickLongRangeGoal(edict_t *self);
void     ACEAI_PickShortRangeGoal(edict_t *self);
void	 ACEAI_PickSafeGoal(edict_t *self);
qboolean ACEAI_FindEnemy(edict_t *self, int *total);
qboolean ACEAI_ChooseWeapon(edict_t *self);
void ACEAI_Cmd_Choose( edict_t *ent, char *s );
void ACEAI_Cmd_Choose_Weapon_Num( edict_t *ent, int num );
void ACEAI_Cmd_Choose_Item_Num( edict_t *ent, int num );

// acebot_cmds.c protos
qboolean ACECM_Commands(edict_t *ent);
void     ACECM_Store(void);

// acebot_items.c protos
void     ACEIT_RebuildPlayerList( void );
qboolean ACEIT_IsVisible(edict_t *self, vec3_t goal);
qboolean ACEIT_IsReachable(edict_t *self,vec3_t goal);
qboolean ACEIT_ChangeWeapon (edict_t *ent, gitem_t *item);
//AQ2 ADD
qboolean ACEIT_ChangeMK23SpecialWeapon (edict_t *ent, gitem_t *item);
qboolean ACEIT_ChangeHCSpecialWeapon (edict_t *ent, gitem_t *item);
qboolean ACEIT_ChangeSniperSpecialWeapon (edict_t *ent, gitem_t *item);
qboolean ACEIT_ChangeM4SpecialWeapon (edict_t *ent, gitem_t *item);
qboolean ACEIT_ChangeM3SpecialWeapon (edict_t *ent, gitem_t *item);
qboolean ACEIT_ChangeMP5SpecialWeapon (edict_t *ent, gitem_t *item);
qboolean ACEIT_ChangeDualSpecialWeapon (edict_t *ent, gitem_t *item);
//AQ2 END
qboolean ACEIT_CanUseArmor (gitem_t *item, edict_t *other);
float    ACEIT_ItemNeed( edict_t *self, edict_t *item_ent );
int      ACEIT_ClassnameToIndex( char *classname );
void     ACEIT_BuildItemNodeTable (qboolean rebuild);

// acebot_movement.c protos
qboolean ACEMV_SpecialMove(edict_t *self,usercmd_t *ucmd);
void     ACEMV_Move(edict_t *self, usercmd_t *ucmd);
void     ACEMV_Attack (edict_t *self, usercmd_t *ucmd);
void     ACEMV_Wander (edict_t *self, usercmd_t *ucmd);

// acebot_nodes.c protos
int      ACEND_FindCost(int from, int to);
int      ACEND_FindCloseReachableNode(edict_t *self, int dist, int type);
int      ACEND_FindClosestReachableNode(edict_t *self, int range, int type);
int ACEND_FindClosestReachableSafeNode(edict_t *self, int range, int type);
void     ACEND_SetGoal(edict_t *self, int goal_node);
qboolean ACEND_FollowPath(edict_t *self);
void     ACEND_GrapFired(edict_t *self);
qboolean ACEND_CheckForLadder(edict_t *self);
void     ACEND_PathMap(edict_t *self);
void     ACEND_InitNodes(void);
void     ACEND_ShowNode(int node);
void     ACEND_DrawPath(edict_t *self);
void     ACEND_ShowPath(edict_t *self, int goal_node);
int      ACEND_AddNode(edict_t *self, int type);
void     ACEND_UpdateNodeEdge(edict_t *self, int from, int to);
void     ACEND_RemoveNodeEdge(edict_t *self, int from, int to);
void     ACEND_ResolveAllPaths(void);
void     ACEND_SaveNodes(void);
void     ACEND_LoadNodes(void);

// acebot_spawn.c protos
void	 ACESP_SaveBots(void);
void	 ACESP_LoadBots(void);
void	 ACESP_LoadBotConfig(void);
edict_t *ACESP_SpawnBotFromConfig( char *inString );
void     ACESP_HoldSpawn(edict_t *self);
void     ACESP_PutClientInServer (edict_t *bot, qboolean respawn, int team);
void     ACESP_Respawn (edict_t *self);
edict_t *ACESP_FindFreeClient (void);
void     ACESP_SetName(edict_t *bot, char *name, char *skin, char *team);
edict_t *ACESP_SpawnBot (char *team, char *name, char *skin, char *userinfo);
void     ACESP_ReAddBots(void);
void     ACESP_RemoveBot(char *name);
void	 safe_cprintf (struct edict_s * ent, int printlevel, const char *fmt, ...);
void     safe_centerprintf (struct edict_s * ent, const char *fmt, ...);
void     debug_printf (char *fmt, ...);
int 	 GetBotCount(void);

// bot_ai.c protos
qboolean	BOTAI_NeedToBandage(edict_t *bot);
void		BOTAI_PickLongRangeGoal(edict_t *self, int	iType);
void		BOTAI_PickShortRangeGoal(edict_t *bot);
void		BOTAI_SetGoal(edict_t *self, int goal_node);
void		BOTAI_Think(edict_t *bot);
qboolean	BOTAI_VisibleEnemy( edict_t *self );

// bot_collision.c protos
qboolean	BOTCOL_CanJumpForward(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanCrawl(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanStand(edict_t	*self);
qboolean	BOTCOL_CanJumpUp(edict_t	*self);
qboolean	BOTCOL_CanMoveForward(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanReachItem(edict_t *bot, vec3_t goal);
qboolean	BOTCOL_CheckShot(edict_t *bot);
qboolean	BOTCOL_WaterMoveForward(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanLeaveWater(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanMoveSafely(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanStrafeSafely(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CanJumpGap(edict_t	*self, vec3_t angles);
qboolean	BOTCOL_CheckBump (edict_t *bot, usercmd_t *ucmd);
qboolean	BOTCOL_Visible (edict_t *bot, edict_t *other);

// bot_combat.c protos
void		BOTCOM_Aim (edict_t *bot, edict_t *target, vec3_t angles);
void		BOTCOM_AimAt (edict_t *bot, vec3_t target, vec3_t angles);
void		BOTCOM_BasicAttack (edict_t *bot, usercmd_t *cmd, vec3_t vTarget);

// bot_movement.c protos
void		BOTMV_Roaming( edict_t *bot, vec3_t angles, usercmd_t *cmd);
float		BOTMV_FindBestDirection(edict_t	*bot, vec3_t vBestDest, vec3_t angles);
void		BOTMV_SetJumpVelocity(edict_t *bot, vec3_t angles, vec3_t vTempDest);
void		BOTMV_SetRandomDirection(edict_t *bot, vec3_t angles);

// bot_states.c protos
void		BOTST_Active( edict_t *bot, vec3_t angles, usercmd_t *cmd);
void		BOTST_Roaming( edict_t *bot, vec3_t angles, usercmd_t *cmd);

//bot_utility.c protos
void		BOTUT_Cmd_Say_f (edict_t *ent, char *pMsg);
void		BOTUT_MakeTargetVector(edict_t *bot, vec3_t angles, vec3_t vDest);
void		BOTUT_ShowNodes (edict_t *ent);
void		BOTUT_TempLaser( vec3_t start, vec3_t end);

// bot_weapon.c protos
int			BOTWP_ChangeMK23Mode(edict_t *bot);
int			BOTWP_ChangeSniperMode(edict_t *bot);
qboolean	BOTWP_ChooseWeapon(edict_t *bot);
int			BOTWP_GetMK23Mode(edict_t *bot);
int			BOTWP_GetSniperMode(edict_t *bot);
void		BOTWP_RemoveSniperZoomMode(edict_t *bot);

#endif
