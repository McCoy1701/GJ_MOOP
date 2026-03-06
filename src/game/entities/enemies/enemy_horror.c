#include <stdlib.h>
#include <string.h>

#include "enemies.h"
#include "pathfinding.h"
#include "visibility.h"
#include "combat_vfx.h"
#include "console.h"
#include "game_events.h"

#define SPAWN_COOLDOWN    4
#define MAX_BABIES        3
#define DEFAULT_CHASE     6

/* ---- A* blocker ---- */

typedef struct
{
  int (*walkable)(int,int);
  int player_row, player_col;
  Enemy_t* all;
  int count;
} HorrorPathCtx_t;

static int horror_blocked( int r, int c, void* ctx )
{
  HorrorPathCtx_t* p = ctx;
  if ( !p->walkable( r, c ) )                     return 1;
  if ( r == p->player_row && c == p->player_col )  return 1;
  if ( EnemyBlockedByNPC( r, c ) )                 return 1;
  if ( EnemyMobileAt( p->all, p->count, r, c ) )   return 1;
  return 0;
}

/* ---- helpers ---- */

static int count_babies( Enemy_t* all, int count )
{
  int n = 0;
  for ( int i = 0; i < count; i++ )
  {
    if ( !all[i].alive ) continue;
    if ( strcmp( g_enemy_types[all[i].type_idx].ai, "baby_horror" ) == 0 )
      n++;
  }
  return n;
}

static void move_toward( Enemy_t* e, int target_row, int target_col,
                         int player_row, int player_col,
                         int (*walkable)(int,int), Enemy_t* all, int count )
{
  HorrorPathCtx_t ctx = { walkable, player_row, player_col, all, count };
  PathNode_t path[PATH_MAX_LEN];
  int len = PathfindAStar( e->row, e->col, target_row, target_col,
                           EnemyGridW(), EnemyGridH(),
                           horror_blocked, &ctx, path );
  if ( len >= 2
       && !EnemyMobileAt( all, count, path[1].row, path[1].col )
       && !EnemyBlockedByNPC( path[1].row, path[1].col ) )
  {
    e->row = path[1].row;
    e->col = path[1].col;
  }
}

/* ---- Main horror AI tick ---- */

void EnemyHorrorTick( Enemy_t* e, int player_row, int player_col,
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

  /* Update chase memory */
  if ( can_see )
  {
    e->last_seen_row = player_row;
    e->last_seen_col = player_col;
    e->chase_turns   = DEFAULT_CHASE;
  }
  else if ( e->chase_turns > 0 )
  {
    e->chase_turns--;
  }

  if ( !can_see && e->chase_turns <= 0 ) return; /* idle */

  /* Spawn cooldown tick */
  if ( e->ai_state > 0 )
    e->ai_state--;

  /* Spawn a baby horror if under cap and off cooldown */
  if ( count_babies( all, count ) < MAX_BABIES && e->ai_state == 0 )
  {
    int ti = EnemyHorrorSpawnBaby( player_row, player_col, walkable, all, count );
    if ( ti >= 0 )
    {
      e->ai_state = SPAWN_COOLDOWN;
      return; /* costs the turn */
    }
  }

  /* Adjacent = attack (handled by combat system) */
  if ( can_see && dist <= 1 ) return;

  /* Chase the player */
  int target_row = can_see ? player_row : e->last_seen_row;
  int target_col = can_see ? player_col : e->last_seen_col;

  if ( e->row == target_row && e->col == target_col )
  {
    e->chase_turns = 0;
    return;
  }

  move_toward( e, target_row, target_col, player_row, player_col,
               walkable, all, count );
}
