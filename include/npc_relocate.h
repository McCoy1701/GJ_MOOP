#ifndef __NPC_RELOCATE_H__
#define __NPC_RELOCATE_H__

#include "npc.h"

void NPCRelocateInit( NPC_t* npcs, int* count );
void NPCRelocate( int npc_type_idx, int new_row, int new_col, int fade );
int  NPCRelocateUpdate( float dt );   /* returns 1 while blocking input */
void NPCRelocateDraw( void );

#endif
