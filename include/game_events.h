#ifndef __GAME_EVENTS_H__
#define __GAME_EVENTS_H__

#include "console.h"
#include "items.h"

typedef enum
{
  EVT_LOOK_EQUIPMENT,
  EVT_LOOK_CONSUMABLE,
  EVT_EQUIP,
  EVT_UNEQUIP,
  EVT_SWAP_EQUIP,
  EVT_USE_CONSUMABLE,
} GameEventType_t;

void GameEventsInit( Console_t* c );
void GameEvent( GameEventType_t type, int index );
void GameEventSwap( int new_idx, int old_idx );

#endif
