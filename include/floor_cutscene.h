#ifndef __FLOOR_CUTSCENE_H__
#define __FLOOR_CUTSCENE_H__

#include "npc.h"
#include "enemies.h"

void  FloorCutsceneRegister( NPC_t* npcs, int num_npcs,
                             Enemy_t* enemies, int num_enemies );
int   FloorCutsceneUpdate( float dt );   /* returns 1 while blocking input */
float FloorCutscenePlayerOY( void );     /* player drop Y-offset */

#endif
