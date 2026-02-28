#include <stdlib.h>

#include "enemies.h"

void EnemyRatTick( Enemy_t* e, int player_row, int player_col,
                   int (*walkable)(int,int),
                   Enemy_t* all, int count )
{
  if ( !e->alive ) return;

  int dr = player_row - e->row;
  int dc = player_col - e->col;

  /* Already adjacent — stay put, combat handled elsewhere */
  if ( abs( dr ) + abs( dc ) <= 1 ) return;

  /* Try the axis with the larger gap first */
  int try_r = e->row;
  int try_c = e->col;

  if ( abs( dr ) >= abs( dc ) )
  {
    try_r += ( dr > 0 ) ? 1 : -1;
    if ( walkable( try_r, try_c ) && !EnemyAt( all, count, try_r, try_c ) )
      goto move;

    /* Fallback: try the other axis */
    try_r = e->row;
    try_c += ( dc > 0 ) ? 1 : -1;
    if ( walkable( try_r, try_c ) && !EnemyAt( all, count, try_r, try_c ) )
      goto move;
  }
  else
  {
    try_c += ( dc > 0 ) ? 1 : -1;
    if ( walkable( try_r, try_c ) && !EnemyAt( all, count, try_r, try_c ) )
      goto move;

    /* Fallback: try the other axis */
    try_c = e->col;
    try_r += ( dr > 0 ) ? 1 : -1;
    if ( walkable( try_r, try_c ) && !EnemyAt( all, count, try_r, try_c ) )
      goto move;
  }

  return;  /* Couldn't move */

move:
  e->row = try_r;
  e->col = try_c;
  /* Snap world position — caller's tile_w/h not available,
     but we can derive from current world_x and old row.
     Instead, EnemyRatTick doesn't know tile size, so game_scene
     updates world_x/y after calling this. */
}
