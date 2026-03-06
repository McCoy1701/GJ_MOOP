#include <stdlib.h>
#include <string.h>

#include "enemies.h"
#include "pathfinding.h"
#include "visibility.h"
#include "combat_vfx.h"
#include "spell_vfx.h"
#include "console.h"
#include "game_events.h"

#define TOTEM_COOLDOWN    5
#define HEAL_AMOUNT       2
#define HEAL_RANGE        6   /* Manhattan distance */
#define DEFAULT_CHASE     4

/* ---- A* blocker (avoids player tile) ---- */

typedef struct
{
  int (*walkable)(int,int);
  int player_row, player_col;
  Enemy_t* all;
  int count;
} ShamanPathCtx_t;

static int shaman_blocked( int r, int c, void* ctx )
{
  ShamanPathCtx_t* p = ctx;
  if ( !p->walkable( r, c ) )                     return 1;
  if ( r == p->player_row && c == p->player_col )  return 1;
  if ( EnemyBlockedByNPC( r, c ) )                 return 1;
  if ( EnemyMobileAt( p->all, p->count, r, c ) )   return 1;
  return 0;
}

/* ---- helpers ---- */

/* Only count nearby allies the shaman can actually see */
static int has_allies( Enemy_t* all, int count, Enemy_t* self )
{
  int sight = g_enemy_types[self->type_idx].sight_range;
  if ( sight < 6 ) sight = 6;

  for ( int i = 0; i < count; i++ )
  {
    if ( &all[i] == self || !all[i].alive ) continue;
    if ( strcmp( g_enemy_types[all[i].type_idx].ai, "static" ) == 0 ) continue;
    int dr = abs( all[i].row - self->row );
    int dc = abs( all[i].col - self->col );
    if ( dr + dc <= sight
         && los_clear( self->row, self->col, all[i].row, all[i].col ) )
      return 1;
  }
  return 0;
}

static int totem_alive( Enemy_t* all, int count )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( !all[i].alive ) continue;
    if ( strcmp( g_enemy_types[all[i].type_idx].ai, "static" ) == 0 )
      return 1;
  }
  return 0;
}

/* Move toward target (A* pathfinding, avoids player tile) */
static void move_toward( Enemy_t* e, int target_row, int target_col,
                         int player_row, int player_col,
                         int (*walkable)(int,int), Enemy_t* all, int count )
{
  ShamanPathCtx_t ctx = { walkable, player_row, player_col, all, count };
  PathNode_t path[PATH_MAX_LEN];
  int len = PathfindAStar( e->row, e->col, target_row, target_col,
                           EnemyGridW(), EnemyGridH(),
                           shaman_blocked, &ctx, path );
  if ( len >= 2
       && !EnemyMobileAt( all, count, path[1].row, path[1].col )
       && !EnemyBlockedByNPC( path[1].row, path[1].col ) )
  {
    e->row = path[1].row;
    e->col = path[1].col;
  }
}

/* Move away from target (flee) */
static void move_away( Enemy_t* e, int target_row, int target_col,
                       int player_row, int player_col,
                       int (*walkable)(int,int), Enemy_t* all, int count )
{
  static const int dx[] = { 1, -1, 0, 0 };
  static const int dy[] = { 0, 0, 1, -1 };

  int cur_dist = abs( target_row - e->row ) + abs( target_col - e->col );
  int best_dist = cur_dist;
  int best_r = e->row, best_c = e->col;

  for ( int i = 0; i < 4; i++ )
  {
    int nr = e->row + dx[i];
    int nc = e->col + dy[i];
    if ( !walkable( nr, nc ) )            continue;
    if ( nr == player_row && nc == player_col ) continue;
    if ( EnemyMobileAt( all, count, nr, nc ) ) continue;
    if ( EnemyBlockedByNPC( nr, nc ) )   continue;

    int nd = abs( target_row - nr ) + abs( target_col - nc );
    if ( nd > best_dist )
    { best_dist = nd; best_r = nr; best_c = nc; }
  }

  /* If can't flee further, try lateral move */
  if ( best_r == e->row && best_c == e->col )
  {
    int start = rand() % 4;
    for ( int j = 0; j < 4; j++ )
    {
      int i  = ( start + j ) % 4;
      int nr = e->row + dx[i];
      int nc = e->col + dy[i];
      if ( !walkable( nr, nc ) )            continue;
      if ( nr == player_row && nc == player_col ) continue;
      if ( EnemyMobileAt( all, count, nr, nc ) ) continue;
      if ( EnemyBlockedByNPC( nr, nc ) )   continue;
      best_r = nr; best_c = nc; break;
    }
  }

  e->row = best_r;
  e->col = best_c;
}

/* Try to heal a nearby damaged enemy. Returns 1 if healed. */
static int try_heal( Enemy_t* e, Enemy_t* all, int count )
{
  Enemy_t* best = NULL;
  int best_missing = 0;

  for ( int i = 0; i < count; i++ )
  {
    if ( &all[i] == e || !all[i].alive ) continue;
    if ( strcmp( g_enemy_types[all[i].type_idx].ai, "static" ) == 0 ) continue;

    int dr = abs( all[i].row - e->row );
    int dc = abs( all[i].col - e->col );
    if ( dr + dc > HEAL_RANGE ) continue;

    int max_hp  = g_enemy_types[all[i].type_idx].hp;
    int missing = max_hp - all[i].hp;
    if ( missing > 0 && missing > best_missing )
    {
      best = &all[i];
      best_missing = missing;
    }
  }

  if ( !best ) return 0;

  int max_hp = g_enemy_types[best->type_idx].hp;
  int heal = HEAL_AMOUNT;
  if ( best->hp + heal > max_hp )
    heal = max_hp - best->hp;

  best->hp += heal;
  SpellVFXHeal( e->world_x, e->world_y, best->world_x, best->world_y );
  CombatVFXSpawnNumber( best->world_x, best->world_y, heal,
                        (aColor_t){ 0x75, 0xa7, 0x43, 255 } );
  CombatVFXSpawnText( e->world_x, e->world_y,
                      "Heals!", (aColor_t){ 0x75, 0xa7, 0x43, 255 } );

  EnemyType_t* st = &g_enemy_types[e->type_idx];
  EnemyType_t* bt = &g_enemy_types[best->type_idx];
  ConsolePushF( GameEventsGetConsole(), (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                "%s heals %s for %d HP.", st->name, bt->name, heal );
  return 1;
}

/* ---- Main shaman AI tick ---- */

void EnemyShamanTick( Enemy_t* e, int player_row, int player_col,
                      int (*walkable)(int,int),
                      Enemy_t* all, int count )
{
  if ( !e->alive ) return;
  if ( e->stun_turns > 0 || e->root_turns > 0 ) return;

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

  int allies = has_allies( all, count, e );

  /* ======== SUPPORT MODE: allies alive ======== */
  if ( allies && ( can_see || e->chase_turns > 0 ) )
  {
    /* Totem cooldown tick */
    if ( e->ai_state > 0 )
      e->ai_state--;

    /* Place totem if none alive and off cooldown */
    if ( !totem_alive( all, count ) && e->ai_state == 0 )
    {
      int ti = EnemyShamanSpawnTotem( e->row, e->col, walkable, all, count );
      if ( ti >= 0 )
      {
        e->ai_state = TOTEM_COOLDOWN;
        return; /* costs the turn */
      }
    }

    /* Try to heal a nearby damaged ally */
    if ( try_heal( e, all, count ) )
      return; /* healing costs the turn */

    /* Flee from player */
    move_away( e, player_row, player_col, player_row, player_col, walkable, all, count );
    return;
  }

  /* ======== COMBAT MODE: no allies, fight like a rat ======== */
  if ( !can_see && e->chase_turns <= 0 ) return; /* idle */
  if ( can_see && dist <= 1 ) return;             /* adjacent, attack handled */

  int target_row = can_see ? player_row : e->last_seen_row;
  int target_col = can_see ? player_col : e->last_seen_col;

  if ( e->row == target_row && e->col == target_col )
  {
    e->chase_turns = 0;
    return;
  }

  move_toward( e, target_row, target_col, player_row, player_col, walkable, all, count );
}
