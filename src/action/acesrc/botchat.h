//-----------------------------------------------------------------------------
//
//  $Logfile: /LicenseToKill/src/acesrc/botchat.h $

// $Revision: 3 $
//   $Author: Riever $
//     $Date: 28/09/99 7:30 $
/*
 * $Log: /LicenseToKill/src/acesrc/botchat.h $
 * 
 * 3     28/09/99 7:30 Riever
 * Altered function names for LTK
 * 
 * 2     28/09/99 7:10 Riever
 * Modified for LTK
 * 
 * 1     28/09/99 6:57 Riever
 * Initial import of chat files
 */
//
/*
        botchat.h
*/

#ifndef __BOTCHAT_H__
#define __BOTCHAT_H__

void LTK_Chat (edict_t *bot, edict_t *object, int speech);
void LTK_Say (edict_t *ent, char *what);

/*
 * NOTE: DBC = DroneBot Chat
 */

#define DBC_WELCOME 0   // When a bot is created
#define DBC_KILLED 1    // When a bot is killed ;-)
#define DBC_INSULT 2    // When a bot kills someone (player/bot)


#endif  // __DB_CHAT_H__
