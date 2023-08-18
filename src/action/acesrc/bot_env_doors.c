/****************************************************************************/
/*                                                                          */
/*    project    : BOT                  (c) 2000 Connor Caple               */
/*    $Header: /LTK2/SRC/ACESRC/bot_env_doors.cpp 3     27/02/00 0:59 Riever $ */
/*                                                                          */
/*    Thanks to William for helping me to figure this stuff out             */
/*    including his giving me CGF code samples to study                     */
/****************************************************************************/
/*
 * $Log: /LTK2/SRC/ACESRC/bot_env_doors.cpp $
 * 
 * 3     27/02/00 0:59 Riever
 * Added BOTENVD_IsDoorOpening and BOTENVD_IsDoorMoving
 * 
 * 2     27/02/00 0:44 Riever
 * Added BOTENVD_IsDoorOpen(edict_t* pDoor) from CGF code
 */

#include "../g_local.h"

//#include "bot_env_doors.h"


// constants

// copied from g_func.c
#define STATE_TOP                       0
#define STATE_BOTTOM                    1
#define STATE_UP                        2
#define STATE_DOWN                      3


void door_use (edict_t *self, edict_t *other, edict_t *activator);


// types (from CGF )
enum   BOT_Door_Type
{
  BOT_DOOR_STANDARD       = 0,
  BOT_DOOR_ROTATING       = 1,
  BOT_DOOR_BUTTONOPERATED = 2  
};
typedef enum BOT_Door_Type BOT_Door_Type_t;

struct BOT_Env_Door
{
  edict_t*        pDoor;
  node_t        iDoorNode;
  BOT_Door_Type_t iDoorType;
  vec3_t          vDoorBackOffLocation;
  vec3_t          vDoorBackOffDirection;
  vec_t           fDoorBackOffTime;
  edict_t*        pButton;
};
typedef struct BOT_Env_Door BOT_Env_Door_t;

//----------------------------------------------
// BOTENVD_IsDoorOpen (from CGF)
//----------------------------------------------
int  BOTENVD_IsDoorOpen
(
edict_t* pDoor
)
{
  return (   (pDoor->moveinfo.state == STATE_TOP)
         ); 
}

//-----------------------------------------------
// BOTENVD_IsDoorOpening (from CGF)
//-----------------------------------------------
int  BOTENVD_IsDoorOpening
(
edict_t* pDoor
)
{
  return (   (pDoor->moveinfo.state == STATE_UP)
          || (pDoor->moveinfo.state == STATE_TOP)
         ); 
}

//-----------------------------------------------
// BOTENVD_IsDoorMoving
//-----------------------------------------------
int		BOTENVD_IsDoorMoving
(
edict_t* pDoor
)
{
	return ( VectorLength(pDoor->avelocity) > 0.0);
}

