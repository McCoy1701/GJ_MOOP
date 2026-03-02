#include <stdlib.h>

#include "enemies.h"
#include "visibility.h"

#define DEFAULT_CHASE_TURNS  4

void EnemyBasicAITick( Enemy_t* e, int player_row, int player_col,
                   int (*walkable)(int,int),
                   Enemy_t* all, int count )
{
  if ( !e->alive ) return;

  int sight = g_enemy_types[e->type_idx].sight_range;

  int dr = player_row - e->row;
  int dc = player_col - e->col;
  int dist = abs( dr ) + abs( dc );

  int can_see = ( dist <= sight
                  && los_clear( e->row, e->col, player_row, player_col ) );

  /* Determine target tile */
  int target_row, target_col;

  if ( can_see )
  {
    /* Direct pursuit - remember where we saw the player */
    e->last_seen_row = player_row;
    e->last_seen_col = player_col;
    e->chase_turns   = DEFAULT_CHASE_TURNS;
    target_row = player_row;
    target_col = player_col;
  }
  else if ( e->chase_turns > 0 )
  {
    /* Lost sight - move toward last known position */
    e->chase_turns--;

    /* Already reached last known spot - give up */
    if ( e->row == e->last_seen_row && e->col == e->last_seen_col )
    {
      e->chase_turns = 0;
      return;
    }

    target_row = e->last_seen_row;
    target_col = e->last_seen_col;
  }
  else
  {
    /* Idle - no target */
    return;
  }

  /* Already adjacent to player - stay put, combat handled elsewhere */
  if ( can_see && dist <= 1 ) return;

  /* 4-direction pathfinding: pick the walkable, unoccupied neighbor
     closest to the target (Manhattan distance).  Only move if it
     actually gets us closer - prevents oscillation. */
  static const int dx[] = { 1, -1, 0, 0 };
  static const int dy[] = { 0, 0, 1, -1 };

  int cur_dist = abs( target_row - e->row ) + abs( target_col - e->col );
  int best_dist = cur_dist;
  int best_r = e->row;
  int best_c = e->col;

  for ( int i = 0; i < 4; i++ )
  {
    int nr = e->row + dx[i];
    int nc = e->col + dy[i];

    if ( !walkable( nr, nc ) )            continue;
    if ( EnemyAt( all, count, nr, nc ) ) continue;
    if ( EnemyBlockedByNPC( nr, nc ) )   continue;

    int nd = abs( target_row - nr ) + abs( target_col - nc );
    if ( nd < best_dist )
    {
      best_dist = nd;
      best_r = nr;
      best_c = nc;
    }
  }

  /* Stuck - no closer tile found.  Allow a lateral (same-distance)
     move so the enemy can path around NPCs / other blockers.
     Randomize start direction to avoid deterministic oscillation. */
  if ( best_r == e->row && best_c == e->col )
  {
    int start = rand() % 4;
    for ( int j = 0; j < 4; j++ )
    {
      int i  = ( start + j ) % 4;
      int nr = e->row + dx[i];
      int nc = e->col + dy[i];

      if ( !walkable( nr, nc ) )            continue;
      if ( EnemyAt( all, count, nr, nc ) ) continue;
      if ( EnemyBlockedByNPC( nr, nc ) )   continue;

      int nd = abs( target_row - nr ) + abs( target_col - nc );
      if ( nd <= cur_dist )
      {
        best_r = nr;
        best_c = nc;
        break;
      }
    }
  }

  if ( best_r != e->row || best_c != e->col )
  {
    e->row = best_r;
    e->col = best_c;
  }
}
