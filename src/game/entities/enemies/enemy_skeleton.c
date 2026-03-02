#include <stdlib.h>

#include "enemies.h"
#include "combat.h"
#include "visibility.h"

#define SKEL_CHASE_TURNS 4

/* AI states */
#define ST_ACQUIRE   0
#define ST_TELEGRAPH 1
#define ST_SHOOT     2
#define ST_COOLDOWN1 3
#define ST_COOLDOWN2 4

/* ---- helpers ---- */

/* Check if player is on a clear cardinal line within range.
   Returns 1 and sets out_dr/out_dc to the firing direction. */
static int has_clear_shot( Enemy_t* e, int pr, int pc, int range,
                           int (*walkable)(int,int),
                           int* out_dr, int* out_dc )
{
  if ( e->row == pr && e->col != pc )
  {
    /* Same row - shot along col axis */
    int dc = ( pc > e->col ) ? 1 : -1;
    int dist = abs( pc - e->col );
    if ( dist > range ) return 0;
    int cc = e->col;
    for ( int s = 0; s < dist; s++ )
    {
      cc += dc;
      if ( !walkable( e->row, cc ) ) return 0;
      if ( EnemyBlockedByNPC( e->row, cc ) ) return 0;
    }
    *out_dr = 0;
    *out_dc = dc;
    return 1;
  }
  else if ( e->col == pc && e->row != pr )
  {
    /* Same col - shot along row axis */
    int dr = ( pr > e->row ) ? 1 : -1;
    int dist = abs( pr - e->row );
    if ( dist > range ) return 0;
    int cr = e->row;
    for ( int s = 0; s < dist; s++ )
    {
      cr += dr;
      if ( !walkable( cr, e->col ) ) return 0;
      if ( EnemyBlockedByNPC( cr, e->col ) ) return 0;
    }
    *out_dr = dr;
    *out_dc = 0;
    return 1;
  }
  return 0;
}

/* Move toward target (greedy 4-dir) */
static void move_toward( Enemy_t* e, int target_r, int target_c,
                         int (*walkable)(int,int),
                         Enemy_t* all, int count )
{
  static const int dx[] = { 1, -1, 0, 0 };
  static const int dy[] = { 0, 0, 1, -1 };

  int cur_dist = abs( target_r - e->row ) + abs( target_c - e->col );
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
    int nd = abs( target_r - nr ) + abs( target_c - nc );
    if ( nd < best_dist )
    {
      best_dist = nd;
      best_r = nr;
      best_c = nc;
    }
  }

  if ( best_r != e->row || best_c != e->col )
  {
    e->row = best_r;
    e->col = best_c;
  }
}

/* Move away from target (retreat) */
static void move_away( Enemy_t* e, int target_r, int target_c,
                       int (*walkable)(int,int),
                       Enemy_t* all, int count )
{
  static const int dx[] = { 1, -1, 0, 0 };
  static const int dy[] = { 0, 0, 1, -1 };

  int cur_dist = abs( target_r - e->row ) + abs( target_c - e->col );
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
    int nd = abs( target_r - nr ) + abs( target_c - nc );
    if ( nd > best_dist )
    {
      best_dist = nd;
      best_r = nr;
      best_c = nc;
    }
  }

  if ( best_r != e->row || best_c != e->col )
  {
    e->row = best_r;
    e->col = best_c;
  }
}

/* Strafe: move to a neighbor that keeps roughly the same distance */
static void move_strafe( Enemy_t* e, int target_r, int target_c,
                         int (*walkable)(int,int),
                         Enemy_t* all, int count )
{
  static const int dx[] = { 1, -1, 0, 0 };
  static const int dy[] = { 0, 0, 1, -1 };

  int cur_dist = abs( target_r - e->row ) + abs( target_c - e->col );
  int best_diff = 999;
  int best_r = e->row;
  int best_c = e->col;

  for ( int i = 0; i < 4; i++ )
  {
    int nr = e->row + dx[i];
    int nc = e->col + dy[i];
    if ( !walkable( nr, nc ) )            continue;
    if ( EnemyAt( all, count, nr, nc ) ) continue;
    if ( EnemyBlockedByNPC( nr, nc ) )   continue;
    int nd = abs( target_r - nr ) + abs( target_c - nc );
    int diff = abs( nd - cur_dist );
    if ( diff < best_diff || ( diff == best_diff && ( rand() & 1 ) ) )
    {
      best_diff = diff;
      best_r = nr;
      best_c = nc;
    }
  }

  if ( best_r != e->row || best_c != e->col )
  {
    e->row = best_r;
    e->col = best_c;
  }
}

/* Cooldown movement: retreat / strafe / chase depending on distance */
static void cooldown_move( Enemy_t* e, int pr, int pc,
                           int (*walkable)(int,int),
                           Enemy_t* all, int count )
{
  int dist = abs( pr - e->row ) + abs( pc - e->col );

  if ( dist <= 2 )
    move_away( e, pr, pc, walkable, all, count );
  else if ( dist <= 4 )
    move_strafe( e, pr, pc, walkable, all, count );
  else
    move_toward( e, pr, pc, walkable, all, count );
}

/* Chase toward last known or give up if arrived */
static void do_chase( Enemy_t* e, int (*walkable)(int,int),
                      Enemy_t* all, int count )
{
  if ( e->chase_turns <= 0 ) return;
  e->chase_turns--;
  if ( e->row == e->last_seen_row && e->col == e->last_seen_col )
  {
    e->chase_turns = 0;
    return;
  }
  move_toward( e, e->last_seen_row, e->last_seen_col,
               walkable, all, count );
}

/* ---- main tick ---- */

void EnemySkeletonTick( Enemy_t* e, int player_row, int player_col,
                        int (*walkable)(int,int),
                        Enemy_t* all, int count )
{
  if ( !e->alive ) return;

  EnemyType_t* t = &g_enemy_types[e->type_idx];
  int dr = player_row - e->row;
  int dc = player_col - e->col;
  int dist = abs( dr ) + abs( dc );

  int can_see = ( dist <= t->sight_range
                  && los_clear( e->row, e->col, player_row, player_col ) );

  /* Update chase memory */
  if ( can_see )
  {
    e->last_seen_row = player_row;
    e->last_seen_col = player_col;
    e->chase_turns   = SKEL_CHASE_TURNS;
  }

  switch ( e->ai_state )
  {
    case ST_ACQUIRE:
    {
      /* Check for an actual clear shot (player on cardinal line) */
      int shot_dr, shot_dc;
      if ( can_see
           && has_clear_shot( e, player_row, player_col, t->range,
                              walkable, &shot_dr, &shot_dc ) )
      {
        e->ai_dir_row = shot_dr;
        e->ai_dir_col = shot_dc;
        e->ai_state = ST_TELEGRAPH;
        return;
      }

      /* No shot - move toward player to get into position */
      if ( can_see )
        move_toward( e, player_row, player_col, walkable, all, count );
      else
        do_chase( e, walkable, all, count );
      break;
    }

    case ST_TELEGRAPH:
    {
      /* Committed - fire along the locked direction */
      int cr = e->row;
      int cc = e->col;

      for ( int step = 0; step < t->range; step++ )
      {
        cr += e->ai_dir_row;
        cc += e->ai_dir_col;
        if ( !walkable( cr, cc ) ) break;
        if ( EnemyBlockedByNPC( cr, cc ) ) break;

        if ( cr == player_row && cc == player_col )
        {
          CombatEnemyHit( e );
          break;
        }
      }

      e->ai_state = ST_COOLDOWN1;
      break;
    }

    case ST_COOLDOWN1:
    {
      if ( can_see )
        cooldown_move( e, player_row, player_col, walkable, all, count );
      else
        do_chase( e, walkable, all, count );
      e->ai_state = ST_COOLDOWN2;
      break;
    }

    case ST_COOLDOWN2:
    {
      if ( can_see )
        cooldown_move( e, player_row, player_col, walkable, all, count );
      else
        do_chase( e, walkable, all, count );
      e->ai_state = ST_ACQUIRE;
      break;
    }

    default:
      e->ai_state = ST_ACQUIRE;
      break;
  }
}
