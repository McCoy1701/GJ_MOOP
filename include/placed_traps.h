#ifndef __PLACED_TRAPS_H__
#define __PLACED_TRAPS_H__

#include <Archimedes.h>
#include "console.h"
#include "world.h"
#include "game_viewport.h"

#define MAX_PLACED_TRAPS 16

typedef struct
{
  int row, col;
  int damage;
  int stun_turns;
  int active;
  int cons_idx;
  aImage_t* image;
} PlacedTrap_t;

void           PlacedTrapsInit( Console_t* con );
void           PlacedTrapSpawn( int row, int col, int damage, int stun_turns,
                                int cons_idx, aImage_t* image );
PlacedTrap_t*  PlacedTrapAt( int row, int col );
void           PlacedTrapRemove( PlacedTrap_t* trap );
int            PlacedTrapPickup( int row, int col );
void           PlacedTrapsDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                                   World_t* world, int gfx_mode );

#endif
