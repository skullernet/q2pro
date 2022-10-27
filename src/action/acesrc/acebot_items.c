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
 * $Header: /LTK2/src/acesrc/acebot_items.c 6     29/02/00 11:11 Riever $
 *
 * $History: acebot_items.c $
 * 
 * *****************  Version 6  *****************
 * User: Riever       Date: 29/02/00   Time: 11:11
 * Updated in $/LTK2/src/acesrc
 * Added ability to collect more knives.
 * 
 * *****************  Version 5  *****************
 * User: Riever       Date: 24/02/00   Time: 19:59
 * Updated in $/LTK2/src/acesrc
 * Fixed item handling (Broken by new node-height code.
 * 
 * *****************  Version 4  *****************
 * User: Riever       Date: 23/02/00   Time: 23:17
 * Updated in $/LTK2/src/acesrc
 * New door node creation implemented and doors removed from item table.
 * 
 * *****************  Version 3  *****************
 * User: Riever       Date: 23/02/00   Time: 17:24
 * Updated in $/LTK2/src/acesrc
 * Added support for 'sv shownodes on/off'
 * Enabled creation of nodes for ALL doors. (Stage 1 of new method)
 * 
 * *****************  Version 2  *****************
 * User: Riever       Date: 17/02/00   Time: 17:53
 * Updated in $/LTK2/src/acesrc
 * Fixed item list to be in the right order!
 * 
 */

///////////////////////////////////////////////////////////////////////
//
//  acebot_items.c - This file contains all of the 
//                   item handling routines for the 
//                   ACE bot, including fact table support
//           
///////////////////////////////////////////////////////////////////////

#include "../g_local.h"

#include "acebot.h"

int	num_players = 0;
int num_items = 0;
item_table_t item_table[MAX_EDICTS];
edict_t *players[MAX_CLIENTS];		// pointers to all players in the game

///////////////////////////////////////////////////////////////////////
// Add/Remove player from our list
///////////////////////////////////////////////////////////////////////
void ACEIT_RebuildPlayerList( void )
{
	size_t i;

	num_players = 0;

	for( i = 1; i <= game.maxclients; i ++ )
	{
		edict_t *ent = &g_edicts[i];
		if( ent->client && ent->client->pers.connected )
		{
			players[ num_players ] = ent;
			num_players ++;
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Can we get there?
///////////////////////////////////////////////////////////////////////
qboolean ACEIT_IsReachable(edict_t *self, vec3_t goal)
{
	trace_t trace;
	vec3_t v;

	VectorCopy(self->mins,v);
	v[2] += 18; // Stepsize

//AQ2	trace = gi.trace (self->s.origin, v, self->maxs, goal, self, MASK_OPAQUE);
	trace = gi.trace (self->s.origin, v, self->maxs, goal, self, MASK_SOLID|MASK_OPAQUE);
	
	// Yes we can see it
	if (trace.fraction == 1.0)
		return true;
	else
		return false;

}

///////////////////////////////////////////////////////////////////////
// Visiblilty check 
///////////////////////////////////////////////////////////////////////
qboolean ACEIT_IsVisible(edict_t *self, vec3_t goal)
{
	trace_t trace;
	
//AQ2	trace = gi.trace (self->s.origin, vec3_origin, vec3_origin, goal, self, MASK_OPAQUE);
	trace = gi.trace (self->s.origin, vec3_origin, vec3_origin, goal, self, MASK_SOLID|MASK_OPAQUE);
	
	// Yes we can see it
	if (trace.fraction == 1.0)
		return true;
	else
		return false;

}

///////////////////////////////////////////////////////////////////////
//  Weapon changing support
///////////////////////////////////////////////////////////////////////
//AQ2 CHANGE
// Completely rewritten for AQ2!
//
qboolean ACEIT_ChangeWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
		return false;

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (!ent->client->inventory[ammo_index] )//&& !g_select_empty->value)
			clips = false;
		else
			clips = true;
	}

	// see if we're already using it
	if (item == ent->client->weapon)
	{
		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}
		// No ammo - forget it!
		if(!loaded && !clips)
			return false;
		// If it's not loaded - use a new clip
		else if( !loaded )
			Cmd_New_Reload_f ( ent );
		return true;
	}

	// Change to this weapon
	ent->client->newweapon = item;
	ChangeWeapon( ent );
	
	return true;
}

//===============================
// Handling for MK23 (debugging)
//===============================
qboolean ACEIT_ChangeMK23SpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got MP23\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 1)
			clips = false;
		else
			clips = true;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got MK23 Ammo\n");
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload MK23\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// Not current weapon, check ammo
	if( (ent->client->mk23_rds < 1) && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No MK23 Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to MK23\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}

//===============================
// Handling for HC (debugging)
//===============================
qboolean ACEIT_ChangeHCSpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got HC\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 2)
			clips = false;
		else
			clips = true;
	}
	// Check ammo
	if( (ent->client->cannon_rds < 2) )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No HC Ammo\n");
		loaded = false;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
/*		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}*/
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got HC Ammo\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload HC\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// Not current weapon, check ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No HC Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to HandCannon\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}

//===============================
// Handling for Sniper Rifle (debugging)
//===============================
qboolean ACEIT_ChangeSniperSpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got Sniper Rifle\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 1)
			clips = false;
		else
			clips = true;
	}
	// Check ammo
	if( (ent->client->sniper_rds < 1) )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No Sniper Ammo\n");
		loaded = false;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
/*		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}*/
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got Sniper Ammo\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload Sniper Rifle\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// No ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No Sniper Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to Sniper Rifle\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}
//===============================
// Handling for M3 (debugging)
//===============================
qboolean ACEIT_ChangeM3SpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got M3\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 1)
			clips = false;
		else
			clips = true;
	}
	// Check ammo
	if( (ent->client->shot_rds < 1) )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No M3 Ammo\n");
		loaded = false;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
/*		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}*/
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got M3 Ammo\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload M3\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// No ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No M3 Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to M3\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}
//===============================
// Handling for M4 (debugging)
//===============================
qboolean ACEIT_ChangeM4SpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got MP4\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 1)//&& !g_select_empty->value)
			clips = false;
		else
			clips = true;
	}
	// Check ammo
	if( (ent->client->m4_rds < 1) )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No M4 Ammo\n");
		loaded = false;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
/*		//Do we have one in the chamber?
		if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}*/
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got M4 Ammo\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload M4\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// No ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No M4 Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to M4\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}
//===============================
// Handling for MP5 (debugging)
//===============================
qboolean ACEIT_ChangeMP5SpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got MP5\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
//		ammo_item = FindItem(MP5_AMMO_NAME);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 1)
		{
//			gi.bprintf(PRINT_HIGH,"Out of MP5 Mags\n");
			clips = false;
		}
		else
			clips = true;
	}
	// check ammo
	if( (ent->client->mp5_rds < 1) )
	{
//		gi.bprintf(PRINT_HIGH,"No MP5 Ammo\n");
		loaded = false;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Stopping MP5 use - no ammo\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded  && clips)
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload MP5\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// Not current weapon, check ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No MP5 Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to MP5\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}

//===============================
// Handling for DUAL PISTOLS (debugging)
//===============================
qboolean ACEIT_ChangeDualSpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got Dual Pistols\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 2)//&& !g_select_empty->value)
			clips = false;
		else
			clips = true;
	}
	// Not current weapon, check ammo
	if( (ent->client->dual_rds < 2) )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No Dual Ammo\n");
		loaded = false;
	}

	// see if we're already using it
	if (item == ent->client->weapon)
	{
/*		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}*/
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got Dual Ammo\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload Dual\n");
			Cmd_New_Reload_f ( ent );
		}
		return true;
	}

	// Not current weapon, check ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No Dual Ammo\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to Dual\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}
//AQ2 END

//===============================
// Handling for Grenades (debugging)
//===============================
qboolean ACEIT_ChangeGrenSpecialWeapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;
	qboolean	loaded = true;
	qboolean	clips = true;

	// Has not picked up weapon yet
	if(!ent->client->inventory[ITEM_INDEX(item)])
	{
//		gi.bprintf(PRINT_HIGH,"Not got Grenades\n");
		return false;
	}

	// Do we have ammo for it?
	if (item->ammo)
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);
		if (ent->client->inventory[ammo_index] < 1)
			clips = false;
		else
			clips = true;
	}
	// Check ammo
	if( (ent->client->shot_rds < 1) )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No Grenades\n");
		loaded = false;
	}
	// see if we're already using it
	if (item == ent->client->weapon)
	{
/*		//Do we have one in the chamber?
        if( ent->client->weaponstate == WEAPON_END_MAG)
		{
			loaded = false;
		}*/
		// No ammo - forget it!
		if(!loaded && !clips)
		{
//			gi.bprintf(PRINT_HIGH,"Not got Grenades\n");
			DropSpecialWeapon ( ent );
			return false;
		}
		// If it's not loaded - use a new clip
/*		else if( !loaded )
		{
//			gi.bprintf(PRINT_HIGH,"Need to reload \n");
			Cmd_New_Reload_f ( ent );
		}
*/		return true;
	}

	// No ammo
	if( !loaded && !clips )
	{
//		gi.bprintf(PRINT_HIGH,"No change - No Grenades\n");
		return false;
	}

	// Change to this weapon
//	gi.bprintf(PRINT_HIGH,"Changing to Grenades\n");
	ent->client->newweapon = item;
	ChangeWeapon ( ent );
	return true;
}





extern gitem_armor_t jacketarmor_info;
extern gitem_armor_t combatarmor_info;
extern gitem_armor_t bodyarmor_info;

///////////////////////////////////////////////////////////////////////
// Check if we can use the armor
///////////////////////////////////////////////////////////////////////
qboolean ACEIT_CanUseArmor (gitem_t *item, edict_t *other)
{
/*
	int				old_armor_index;
	gitem_armor_t	*oldinfo;
	gitem_armor_t	*newinfo;
	int				newcount;
	float			salvage;
	int				salvagecount;

	// get info on new armor
	newinfo = (gitem_armor_t *)item->info;

	old_armor_index = ArmorIndex (other);

	// handle armor shards specially
	if (item->tag == ARMOR_SHARD)
		return true;
	
	// get info on old armor
	if (old_armor_index == ITEM_INDEX(FindItem("Jacket Armor")))
		oldinfo = &jacketarmor_info;
	else if (old_armor_index == ITEM_INDEX(FindItem("Combat Armor")))
		oldinfo = &combatarmor_info;
	else // (old_armor_index == body_armor_index)
		oldinfo = &bodyarmor_info;

	if (newinfo->normal_protection <= oldinfo->normal_protection)
	{
		// calc new armor values
		salvage = newinfo->normal_protection / oldinfo->normal_protection;
		salvagecount = salvage * newinfo->base_count;
		newcount = other->client->inventory[old_armor_index] + salvagecount;

		if (newcount > oldinfo->max_count)
			newcount = oldinfo->max_count;

		// if we're already maxed out then we don't need the new armor
		if (other->client->inventory[old_armor_index] >= newcount)
			return false;

	}

	return true;
*/
	return false;
}


///////////////////////////////////////////////////////////////////////
// Determines the NEED for an item
//
// This function can be modified to support new items to pick up
// Any other logic that needs to be added for custom decision making
// can be added here. For now it is very simple.
///////////////////////////////////////////////////////////////////////
float ACEIT_ItemNeed( edict_t *self, edict_t *item_ent )
{
	gitem_t *item = NULL;
	int band = 0;

	if( ! item_ent )
		return 0;

	// Make sure domination flags are always considered important short-range goals.
	if( dom->value && (Q_stricmp( item_ent->classname, "item_flag" ) == 0) )
		return (DomFlagOwner(item_ent) != self->client->resp.team) ? 9000 : 0;

	item = FindItemByClassname( item_ent->classname );
	if( ! item )
		return 0;

	band = INV_AMMO(self,BAND_NUM) ? 1 : 0;

	switch( item->typeNum )
	{
		case FLAG_T1_NUM:
		case FLAG_T2_NUM:
			// If we're carrying it already, ignore it.
			if( INV_AMMO(self,item->typeNum) )
				return 0.0;
			// If we have the other flag, we really want to capture it!
			else if( INV_AMMO(self,FLAG_T1_NUM) || INV_AMMO(self,FLAG_T2_NUM) )
				return 1000.0;
			// Dropped flags are a very high priority.
			else if( item_ent->spawnflags & DROPPED_ITEM )
				return 100.0;
			// Go for the enemy flag at a fairly high priority.
			else if( self->client->resp.team != item->typeNum + 1 - FLAG_T1_NUM )
				return 2.0;
			else
				return 0.0;

		case MP5_NUM:
		case M4_NUM:
		case M3_NUM:
		case HC_NUM:
		case SNIPER_NUM:
		case DUAL_NUM:
			if( (self->client->unique_weapon_total < unique_weapons->value + band) && ! INV_AMMO(self,item->typeNum) )
				return 0.7;
			else
				return 0.0;

		case SIL_NUM:
		case SLIP_NUM:
		case BAND_NUM:
		case KEV_NUM:
		case LASER_NUM:
		case HELM_NUM:
			if( (self->client->unique_item_total < unique_items->value) && ! INV_AMMO(self,item->typeNum) )
				return 0.6;
			else
				return 0.0;

		case MK23_ANUM:
			if( INV_AMMO(self,item->typeNum) < self->client->max_pistolmags )
				return 0.3;
			else
				return 0.0;

		case M4_ANUM:
			if( INV_AMMO(self,item->typeNum) < self->client->max_m4mags )
				return 0.3;
			else
				return 0.0;

		case MP5_ANUM:
			if( INV_AMMO(self,item->typeNum) < self->client->max_mp5mags )
				return 0.3;
			else
				return 0.0;

		case SNIPER_ANUM:
			if( INV_AMMO(self,item->typeNum) < self->client->max_sniper_rnds )
				return 0.3;
			else
				return 0.0;

		case SHELL_ANUM:
			if( INV_AMMO(self,item->typeNum) < self->client->max_shells )
				return 0.3;
			else
				return 0.0;

		case KNIFE_NUM:
			if( INV_AMMO(self,item->typeNum) < self->client->knife_max )
				return 0.3;
			else
				return 0.0;

		default:
			return 0.0;
	}
}


///////////////////////////////////////////////////////////////////////
// Convert a classname to its index value
//
// I prefer to use integers/defines for simplicity sake. This routine
// can lead to some slowdowns I guess, but makes the rest of the code
// easier to deal with.
///////////////////////////////////////////////////////////////////////
int ACEIT_ClassnameToIndex( char *classname )
{
	gitem_t *item = FindItemByClassname( classname );
	if( item )
		return items[item->typeNum].index;
	return INVALID;
}


///////////////////////////////////////////////////////////////////////
// Only called once per level, when saved will not be called again
//
// Downside of the routine is that items can not move about. If the level
// has been saved before and reloaded, it could cause a problem if there
// are items that spawn at random locations.
//
#define DEBUG // uncomment to write out items to a file.
///////////////////////////////////////////////////////////////////////
void ACEIT_BuildItemNodeTable (qboolean rebuild)
{
	edict_t *items;
	int i,item_index;
	vec3_t v,v1,v2;

#ifdef DEBUG
	FILE *pOut; // for testing
	if((pOut = fopen("items.txt","wt"))==NULL)
		return;
#endif
	
	num_items = 0;

	// Add game items
	for(items = g_edicts; items < &g_edicts[globals.num_edicts]; items++)
	{
		// filter out crap
		if(items->solid == SOLID_NOT)
			continue;
		
		if(!items->classname)
			continue;
		
		/////////////////////////////////////////////////////////////////
		// Items
		/////////////////////////////////////////////////////////////////
		item_index = ACEIT_ClassnameToIndex(items->classname);
		
		////////////////////////////////////////////////////////////////
		// SPECIAL NAV NODE DROPPING CODE
		////////////////////////////////////////////////////////////////
		// Special node dropping for platforms
		if(strcmp(items->classname,"func_plat")==0)
		{
			if(!rebuild)
				ACEND_AddNode(items,NODE_PLATFORM);
			item_index = 99; // to allow to pass the item index test
		}
		
		// Special node dropping for teleporters
		if(strcmp(items->classname,"misc_teleporter_dest")==0 || strcmp(items->classname,"misc_teleporter")==0)
		{
			if(!rebuild)
				ACEND_AddNode(items,NODE_TELEPORTER);
			item_index = 99;
		}

		// Special node dropping for doors - RiEvEr
		if(		
			(strcmp(items->classname,"func_door_rotating")==0)
			||	(strcmp(items->classname,"func_door")==0 )
			)
		{
//			item_index = 99;
			if(!rebuild)
			{
				// add a pointer to the item entity
//				item_table[num_items].ent = items;
//				item_table[num_items].item = item_index;
				// create the node
//				item_table[num_items].node = ACEND_AddNode(items,NODE_DOOR);
				ACEND_AddNode(items,NODE_DOOR);
//#ifdef DEBUG
//				fprintf(pOut,"item: %s node: %d pos: %f %f %f\n",items->classname,
//					item_table[num_items].node, 
//					items->s.origin[0],items->s.origin[1],items->s.origin[2]);
//#endif	
//				num_items++;
//				continue;
			}
			continue;
		}

		#ifdef DEBUG
		if(item_index == INVALID)
			fprintf(pOut,"Rejected item: %s node: %d pos: %f %f %f\n",items->classname,item_table[num_items].node,items->s.origin[0],items->s.origin[1],items->s.origin[2]);
//		else
//			fprintf(pOut,"item: %s node: %d pos: %f %f %f\n",items->classname,item_table[num_items].node,items->s.origin[0],items->s.origin[1],items->s.origin[2]);
		#endif		
	
		if(item_index == INVALID)
			continue;

		// add a pointer to the item entity
		item_table[num_items].ent = items;
		item_table[num_items].item = item_index;
	
		// If new, add nodes for items
		if(!rebuild)
		{
				item_table[num_items].node = ACEND_AddNode(items,NODE_ITEM);
#ifdef DEBUG
			fprintf(pOut,"item: %s node: %d pos: %f %f %f\n",items->classname,item_table[num_items].node,items->s.origin[0],items->s.origin[1],items->s.origin[2]);
#endif		
			num_items++;
		}
		else // Now if rebuilding, just relink ent structures 
		{
			// Find stored location
			for(i=0;i<numnodes;i++)
			{
				if(nodes[i].type == NODE_ITEM ||
				   nodes[i].type == NODE_PLATFORM ||
//@@				   nodes[i].type == NODE_DOOR||		// RiEvEr
				   nodes[i].type == NODE_TELEPORTER) // valid types
				{
					VectorCopy(items->s.origin,v);
					v[2] +=8; // Raised all nodes to get better links.
					
					// Add 16 to item type nodes
					if(nodes[i].type == NODE_ITEM)
						v[2] += 16;
					
					// Add 32 to teleporter
					if(nodes[i].type == NODE_TELEPORTER)
						v[2] += 32;

/*					// Door handling
					if( nodes[i].type == NODE_DOOR)
					{
						vec3_t	position;
						// Find mid point of door max and min and put the node there
						VectorClear(position);
//						VectorCopy(items->s.origin, position);
						// find center of door
						position[0] = items->absmin[0] + ((items->maxs[0] - items->mins[0]) /2);
						position[1] = items->absmin[1] + ((items->maxs[1] - items->mins[1]) /2);
//						position[2] -= 16; // lower it a little
						position[2] = items->absmin[2] + 32;
						// Set location
						VectorCopy(position, v);
						gi.bprintf(PRINT_HIGH, "%d ",items->s.angles[0]);
					}*/

					if(nodes[i].type == NODE_PLATFORM)
					{
						VectorCopy(items->maxs,v1);
						VectorCopy(items->mins,v2);
		
						// To get the center
						v[0] = (v1[0] - v2[0]) / 2 + v2[0];
						v[1] = (v1[1] - v2[1]) / 2 + v2[1];
						v[2] = items->mins[2]+64;
					}

					if(v[0] == nodes[i].origin[0] &&
 					   v[1] == nodes[i].origin[1] &&
					   v[2] == nodes[i].origin[2])
					{
						// found a match now link to facts
						item_table[num_items].node = i;			
#ifdef DEBUG
						fprintf(pOut,"Relink item: %s node: %d pos: %f %f %f\n",items->classname,item_table[num_items].node,items->s.origin[0],items->s.origin[1],items->s.origin[2]);
#endif							
						num_items++;
					
					}
/*					else
					{
#ifdef DEBUG
						fprintf(pOut,"Failed item: %s node: %d pos: %f %f %f\n",items->classname,item_table[num_items].node,items->s.origin[0],items->s.origin[1],items->s.origin[2]);
#endif				
					}*/
				}
			}
		}
		

	}

#ifdef DEBUG
	fclose(pOut);
#endif

}
