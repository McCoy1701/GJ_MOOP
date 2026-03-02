#ifndef __POISON_POOL_H__
#define __POISON_POOL_H__

#include <Archimedes.h>
#include "console.h"
#include "world.h"
#include "game_viewport.h"

#define MAX_POISON_POOLS 16

typedef struct
{
  int      row, col;
  int      turns_remaining;
  int      damage;
  int      active;
  aColor_t color;
} PoisonPool_t;

void          PoisonPoolInit( Console_t* con );
void          PoisonPoolSpawn( int row, int col, int duration, int damage,
                               aColor_t color );
PoisonPool_t* PoisonPoolAt( int row, int col );
void          PoisonPoolTick( int player_row, int player_col );
void          PoisonPoolDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                                 World_t* world, int gfx_mode );

#endif
