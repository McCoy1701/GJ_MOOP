#ifndef __DEV_MODE_H__
#define __DEV_MODE_H__

#include "console.h"
#include "npc.h"

void DevModeInit( Console_t* console );
void DevModeSetNPCs( NPC_t* list, int* count );
int  DevModeActive( void );
int  DevModeInput( void );
void DevModeDraw( void );

#endif
