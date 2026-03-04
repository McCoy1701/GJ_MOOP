#include <stdlib.h>

#include "enemies.h"
#include "pathfinding.h"
#include "visibility.h"

#define DEFAULT_CHASE_TURNS  4

/* ---- A* blocker ---- */

typedef struct
{
  int (*walkable)(int,int);
} RatPathCtx_t;

static int rat_blocked( int r, int c, void* ctx )
{
  RatPathCtx_t* p = ctx;
  if ( !p->walkable( r, c ) )      return 1;
  if ( EnemyBlockedByNPC( r, c ) ) return 1;
  return 0;
}

/* ---- main tick ---- */

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

  /* Find best adjacent tile to surround target (not occupied by another enemy) */
  static const int adj_r[] = { -1, 1, 0, 0 };
  static const int adj_c[] = { 0, 0, -1, 1 };
  int best_r = target_row, best_c = target_col;
  int best_dist = 9999;

  for ( int d = 0; d < 4; d++ )
  {
    int ar = target_row + adj_r[d];
    int ac = target_col + adj_c[d];
    if ( !walkable( ar, ac ) )                    continue;
    if ( EnemyAt( all, count, ar, ac ) )          continue;
    if ( ar == e->row && ac == e->col )  { best_r = ar; best_c = ac; break; }
    int ad = abs( ar - e->row ) + abs( ac - e->col );
    if ( ad < best_dist ) { best_dist = ad; best_r = ar; best_c = ac; }
  }

  /* A* pathfinding toward best adjacent tile */
  RatPathCtx_t ctx = { walkable };
  PathNode_t path[PATH_MAX_LEN];
  int len = PathfindAStar( e->row, e->col, best_r, best_c,
                           EnemyGridW(), EnemyGridH(),
                           rat_blocked, &ctx, path );
  if ( len >= 2
       && !EnemyAt( all, count, path[1].row, path[1].col ) )
  {
    e->row = path[1].row;
    e->col = path[1].col;
  }
}
