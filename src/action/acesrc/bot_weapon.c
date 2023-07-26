//---------------------------------------------
// bot_weapon.c
//
// Copyright (c) 2000 Connor Caple all rights reserved
//
// This file contains the bot weapon handling code
//---------------------------------------------
/*
 * $Log:$
 * 
 */
#include "../g_local.h"

/*	FOR REFERENCE ONLY
#define MK23_NUM                0
#define MP5_NUM                 1
#define M4_NUM                  2
#define M3_NUM                  3
#define HC_NUM                  4
#define SNIPER_NUM              5
#define DUAL_NUM                6
#define KNIFE_NUM               7
#define GRENADE_NUM             8
*/

//-----------------------------------------------
// BOTWP_ChangeMK23Mode
//---------------------------------------------

int		BOTWP_ChangeMK23Mode(edict_t *bot)
{
	if ( bot->client->curr_weap != MK23_NUM )
		return INVALID;	// (-1)
	// Change the weapon mode
	Cmd_Weapon_f(bot);
	return (BOTWP_GetMK23Mode(bot));
}

//-----------------------------------------------
// BOTWP_ChangeSniperMode
//
// SNIPER_1X 0	 SNIPER_2X 1
// SNIPER_4X 2	 SNIPER_6X 3
//---------------------------------------------

int		BOTWP_ChangeSniperMode(edict_t *bot)
{
	int	iLastMode;

	iLastMode = BOTWP_GetSniperMode(bot);
	if ( bot->client->curr_weap != SNIPER_NUM )
		return INVALID;	// (-1)
	// Don't change modes too quickly
	//if ( (bot->fLastZoomTime + 0.3) < level.time)
	if( level.framenum >= bot->fLastZoomTime + 0.3 * HZ )
	{
		// Change the weapon mode
		Cmd_Weapon_f(bot);
		if( iLastMode != BOTWP_GetSniperMode(bot))
			bot->fLastZoomTime = level.framenum;
	}
	return (BOTWP_GetSniperMode(bot));
}

//---------------------------------------------
// BOTWP_GetMK23Mode
//---------------------------------------------

int			BOTWP_GetMK23Mode(edict_t *bot)
{
	return (bot->client->pers.mk23_mode );
}

//---------------------------------------------
// BOTWP_GetSniperMode
//---------------------------------------------

int			BOTWP_GetSniperMode(edict_t *bot)
{
	return (bot->client->resp.sniper_mode );
}

//---------------------------------------------
// BOTWP_RemoveSniperZoomMode
//---------------------------------------------
void	BOTWP_RemoveSniperZoomMode(edict_t *bot)
{
	if ( bot->client->curr_weap != SNIPER_NUM )
		return;
	if( BOTWP_GetSniperMode(bot) != 0 )
	{
		BOTWP_ChangeSniperMode(bot);
	}
}

//---------------------------------------------
// BOTWP_ChooseWeapon
//
// Choose the best weapon for bot
//---------------------------------------------

qboolean	BOTWP_ChooseWeapon(edict_t *bot)
{	
	float fRange;
	vec3_t vTemp;
	
	// if no enemy, then what are we doing here?
	if(!bot->enemy)
		return (true);
//AQ2 CHANGE	
	// Currently always favor the dual pistols!
	//@@ This will become the "bot choice" weapon
//	if(ACEIT_ChangeDualSpecialWeapon(bot,FindItem(DUAL_NAME)))
//   	   return;
//AQ2 END

	// Base selection on distance.
	VectorSubtract (bot->s.origin, bot->enemy->s.origin, vTemp);
	fRange = VectorLength(vTemp);
		
/*	// Longer range 
	if(fRange > 1000)
	{
		if(ACEIT_ChangeSniperSpecialWeapon(bot,FindItem(SNIPER_NAME)))
			return (true);
		
		if(ACEIT_ChangeM3SpecialWeapon(bot,FindItem(M3_NAME)))
			return (true);
		
		if(ACEIT_ChangeM4SpecialWeapon(bot,FindItem(M4_NAME)))
			return (true);
		
		if(ACEIT_ChangeMP5SpecialWeapon(bot,FindItem(MP5_NAME)))
   		   return (true);

		if(ACEIT_ChangeMK23SpecialWeapon(bot,FindItem(MK23_NAME)))
   		   return (true);
	}
	
	// Longer range 
	if(fRange > 700)
	{		
		if(ACEIT_ChangeM3SpecialWeapon(bot,FindItem(M3_NAME)))
			return (true);
		
		if(ACEIT_ChangeM4SpecialWeapon(bot,FindItem(M4_NAME)))
			return (true);
		
		if(ACEIT_ChangeMP5SpecialWeapon(bot,FindItem(MP5_NAME)))
   		   return (true);

		if(ACEIT_ChangeSniperSpecialWeapon(bot,FindItem(SNIPER_NAME)))
			return (true);
	
		if(ACEIT_ChangeMK23SpecialWeapon(bot,FindItem(MK23_NAME)))
   		   return (true);
	}
	
	// Longer range 
	if(fRange > 500)
	{		
		if(ACEIT_ChangeMP5SpecialWeapon(bot,FindItem(MP5_NAME)))
   		   return (true);

		if(ACEIT_ChangeM3SpecialWeapon(bot,FindItem(M3_NAME)))
			return (true);
		
		if(ACEIT_ChangeM4SpecialWeapon(bot,FindItem(M4_NAME)))
			return (true);
		
		if(ACEIT_ChangeSniperSpecialWeapon(bot,FindItem(SNIPER_NAME)))
			return (true);
	
		if(ACEIT_ChangeMK23SpecialWeapon(bot,FindItem(MK23_NAME)))
   		   return (true);
	}*/
	
	// Longer range 
	if(fRange > 250)
	{		
		if(ACEIT_ChangeM4SpecialWeapon(bot,FindItem(M4_NAME)))
			return (true);
		
		if(ACEIT_ChangeMP5SpecialWeapon(bot,FindItem(MP5_NAME)))
   		   return (true);

		if(ACEIT_ChangeM3SpecialWeapon(bot,FindItem(M3_NAME)))
			return (true);
		
		if(ACEIT_ChangeSniperSpecialWeapon(bot,FindItem(SNIPER_NAME)))
			return (true);
	
		if(ACEIT_ChangeDualSpecialWeapon(bot,FindItem(DUAL_NAME)))
   		   return (true);

		if(ACEIT_ChangeMK23SpecialWeapon(bot,FindItem(MK23_NAME)))
   		   return (true);
	}
	
	// Short range	   
	if(ACEIT_ChangeHCSpecialWeapon(bot,FindItem(HC_NAME)))
		return (true);
	
	if(ACEIT_ChangeM3SpecialWeapon(bot,FindItem(M3_NAME)))
		return (true);
	
	if(ACEIT_ChangeM4SpecialWeapon(bot,FindItem(M4_NAME)))
		return (true);
	
	if(ACEIT_ChangeMP5SpecialWeapon(bot,FindItem(MP5_NAME)))
   	   return (true);
	
	if(ACEIT_ChangeDualSpecialWeapon(bot,FindItem(DUAL_NAME)))
   	   return (true);

	if(ACEIT_ChangeMK23SpecialWeapon(bot,FindItem(MK23_NAME)))
   	   return (true);
	
	if(ACEIT_ChangeSniperSpecialWeapon(bot,FindItem(SNIPER_NAME)))
		return (true);
	
	if(ACEIT_ChangeWeapon(bot,FindItem(KNIFE_NAME)))
   	   return (true);
	
	// We have no weapon available for use.
	if(debug_mode)
		gi.bprintf(PRINT_HIGH,"%s: No weapon available...\n",bot->client->pers.netname);
	return (false);

}
