#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>

#include "enemies.h"
#include "npc.h"
#include "combat.h"
#include "combat_vfx.h"
#include "world.h"
#include "tween.h"
#include "placed_traps.h"

static World_t* world = NULL;
static NPC_t*   npc_list  = NULL;
static int*     npc_count = NULL;
static TweenManager_t tweens;

/* Turn state machine */
#define TURN_IDLE     0
#define TURN_MOVING   1   /* one enemy moving at a time */
#define TURN_ATTACK   2   /* one enemy lunging at a time */

static int turn_state = TURN_IDLE;

/* Saved from EnemiesStartTurn for use across phases */
static Enemy_t* turn_list  = NULL;
static int      turn_count = 0;
static int      turn_pr, turn_pc;
static int      was_adjacent[MAX_ENEMIES]; /* adjacent before moving */
static int      did_move[MAX_ENEMIES];     /* moved this turn */
static int      move_idx;                  /* current enemy being processed */
static int (*turn_walkable)(int,int);

void EnemiesSetWorld( World_t* w )
{
  world = w;
  InitTweenManager( &tweens );
  turn_state = TURN_IDLE;
}

void EnemiesSetNPCs( void* npcs, int* num )
{
  npc_list  = (NPC_t*)npcs;
  npc_count = num;
}

int EnemyBlockedByNPC( int row, int col )
{
  if ( !npc_list || !npc_count ) return 0;
  return NPCAt( npc_list, *npc_count, row, col ) != NULL;
}

/* --- Lunge callback data --- */

typedef struct
{
  float* target;
  float  home;
} LungeBack_t;

static LungeBack_t lunge_data[MAX_ENEMIES * 2];
static int lunge_count = 0;

static void lunge_back_cb( void* data )
{
  LungeBack_t* lb = (LungeBack_t*)data;
  TweenFloat( &tweens, lb->target, lb->home, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

/* --- Sequential movement: advance to next enemy --- */

static void start_next_move( void );
static void start_next_attack( void );

static void tick_and_move( int i )
{
  /* Stunned enemies skip their turn */
  if ( turn_list[i].stun_turns > 0 ) return;

  EnemyType_t* t = &g_enemy_types[turn_list[i].type_idx];

  int old_row = turn_list[i].row;
  int old_col = turn_list[i].col;
  int old_ai  = turn_list[i].ai_state;

  if ( strcmp( t->ai, "basic" ) == 0 )
    EnemyBasicAITick( &turn_list[i], turn_pr, turn_pc,
                  turn_walkable, turn_list, turn_count );
  else if ( strcmp( t->ai, "ranged_telegraph" ) == 0 )
    EnemySkeletonTick( &turn_list[i], turn_pr, turn_pc,
                       turn_walkable, turn_list, turn_count );

  /* Skeleton just fired - spawn arrow projectile */
  if ( old_ai == 1 && turn_list[i].ai_state == 3 )
  {
    float tw = world->tile_w, th = world->tile_h;
    float sx = turn_list[i].row * tw + tw / 2.0f;
    float sy = turn_list[i].col * th + th / 2.0f;

    int cr = turn_list[i].row, cc = turn_list[i].col;
    for ( int s = 0; s < t->range; s++ )
    {
      int nr = cr + turn_list[i].ai_dir_row;
      int nc = cc + turn_list[i].ai_dir_col;
      if ( nr < 0 || nr >= world->width || nc < 0 || nc >= world->height )
        break;
      int idx = nc * world->width + nr;
      if ( world->background[idx].solid || world->midground[idx].solid )
        break;
      cr = nr;
      cc = nc;
    }

    EnemyProjectileSpawn( sx, sy,
                          cr * tw + tw / 2.0f, cc * th + th / 2.0f,
                          turn_list[i].ai_dir_row,
                          turn_list[i].ai_dir_col );
  }

  if ( turn_list[i].row != old_row || turn_list[i].col != old_col )
  {
    if ( turn_list[i].row < old_row ) turn_list[i].facing_left = 0;
    else if ( turn_list[i].row > old_row ) turn_list[i].facing_left = 1;

    /* Check for placed bear trap on destination tile */
    PlacedTrap_t* trap = PlacedTrapAt( turn_list[i].row, turn_list[i].col );
    if ( trap )
    {
      float wx = turn_list[i].row * world->tile_w + world->tile_w / 2.0f;
      float wy = turn_list[i].col * world->tile_h + world->tile_h / 2.0f;

      turn_list[i].hp -= trap->damage;
      turn_list[i].stun_turns = trap->stun_turns;

      CombatVFXSpawnNumber( wx, wy, trap->damage,
                            (aColor_t){ 0xde, 0x9e, 0x41, 255 } );
      CombatVFXSpawnText( wx, wy, "Trapped!",
                          (aColor_t){ 0xde, 0x9e, 0x41, 255 } );
      PlacedTrapRemove( trap );

      if ( turn_list[i].hp <= 0 )
        CombatHandleEnemyDeath( &turn_list[i] );
    }

    float tx = turn_list[i].row * world->tile_w + world->tile_w / 2.0f;
    float ty = turn_list[i].col * world->tile_h + world->tile_h / 2.0f;
    TweenFloat( &tweens, &turn_list[i].world_x, tx, 0.15f, TWEEN_EASE_OUT_CUBIC );
    TweenFloat( &tweens, &turn_list[i].world_y, ty, 0.15f, TWEEN_EASE_OUT_CUBIC );
    did_move[i] = 1;
  }
}

static void start_next_move( void )
{
  /* Find next alive enemy to move */
  while ( move_idx < turn_count )
  {
    if ( turn_list[move_idx].alive )
    {
      tick_and_move( move_idx );
      if ( did_move[move_idx] )
      {
        /* Tween started - wait for it to finish */
        turn_state = TURN_MOVING;
        move_idx++;
        return;
      }
    }
    move_idx++;
  }

  /* All enemies processed - begin attack phase */
  move_idx = 0;
  start_next_attack();
}

/* --- Sequential attacks: one lunge at a time --- */

static void do_attack( int i )
{
  int dr = turn_pr - turn_list[i].row;
  int dc = turn_pc - turn_list[i].col;

  float lunge_dist = 3.0f;
  lunge_count = 0;

  LungeBack_t* lbx = &lunge_data[lunge_count++];
  lbx->target = &turn_list[i].world_x;
  lbx->home   = turn_list[i].world_x;
  TweenFloatWithCallback( &tweens, &turn_list[i].world_x,
                          turn_list[i].world_x + dr * lunge_dist,
                          0.06f, TWEEN_EASE_OUT_QUAD,
                          lunge_back_cb, lbx );

  LungeBack_t* lby = &lunge_data[lunge_count++];
  lby->target = &turn_list[i].world_y;
  lby->home   = turn_list[i].world_y;
  TweenFloatWithCallback( &tweens, &turn_list[i].world_y,
                          turn_list[i].world_y + dc * lunge_dist,
                          0.06f, TWEEN_EASE_OUT_QUAD,
                          lunge_back_cb, lby );

  CombatEnemyHit( &turn_list[i] );
}

static void start_next_attack( void )
{
  while ( move_idx < turn_count )
  {
    if ( turn_list[move_idx].alive && was_adjacent[move_idx]
         && turn_list[move_idx].stun_turns <= 0 )
    {
      EnemyType_t* at = &g_enemy_types[turn_list[move_idx].type_idx];
      if ( at->range > 0 ) { move_idx++; continue; } /* ranged - no melee */
      int dr = abs( turn_pr - turn_list[move_idx].row );
      int dc = abs( turn_pc - turn_list[move_idx].col );
      if ( dr + dc == 1 )
      {
        do_attack( move_idx );
        turn_state = TURN_ATTACK;
        move_idx++;
        return;
      }
    }
    move_idx++;
  }

  /* No more attackers - decrement stun at end of turn */
  for ( int i = 0; i < turn_count; i++ )
  {
    if ( turn_list[i].alive && turn_list[i].stun_turns > 0 )
      turn_list[i].stun_turns--;
  }
  turn_state = TURN_IDLE;
}

/* --- Public API --- */

void EnemiesStartTurn( Enemy_t* list, int count,
                       int player_row, int player_col,
                       int (*walkable)(int,int) )
{
  turn_list    = list;
  turn_count   = count;
  turn_pr      = player_row;
  turn_pc      = player_col;
  turn_walkable = walkable;

  /* Record which enemies are already adjacent before they move */
  for ( int i = 0; i < count; i++ )
  {
    int dr = abs( player_row - list[i].row );
    int dc = abs( player_col - list[i].col );
    was_adjacent[i] = list[i].alive && ( dr + dc == 1 );
    did_move[i] = 0;
  }

  /* Process status effects before AI runs */
  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;

    /* Poison DOT */
    if ( list[i].poison_ticks > 0 )
    {
      list[i].hp -= list[i].poison_dmg;
      list[i].poison_ticks--;
      CombatVFXSpawnNumber( list[i].world_x, list[i].world_y,
                            list[i].poison_dmg,
                            (aColor_t){ 0x50, 0xc8, 0x50, 255 } );
      if ( list[i].hp <= 0 )
      {
        CombatVFXSpawnText( list[i].world_x, list[i].world_y,
                            "Poisoned!", (aColor_t){ 0x50, 0xc8, 0x50, 255 } );
        CombatHandleEnemyDeath( &list[i] );
      }
    }

    /* Burn DOT */
    if ( list[i].alive && list[i].burn_ticks > 0 )
    {
      list[i].hp -= list[i].burn_dmg;
      list[i].burn_ticks--;
      CombatVFXSpawnNumber( list[i].world_x, list[i].world_y,
                            list[i].burn_dmg,
                            (aColor_t){ 0xff, 0x64, 0x1e, 255 } );
      if ( list[i].hp <= 0 )
      {
        CombatVFXSpawnText( list[i].world_x, list[i].world_y,
                            "Burned!", (aColor_t){ 0xff, 0x64, 0x1e, 255 } );
        CombatHandleEnemyDeath( &list[i] );
      }
    }

    /* Stun/freeze - show VFX (decrement happens at end of turn) */
    if ( list[i].alive && list[i].stun_turns > 0 )
    {
      CombatVFXSpawnText( list[i].world_x, list[i].world_y,
                          "Stunned!", (aColor_t){ 0x64, 0xb4, 0xff, 255 } );
    }
  }

  /* Start sequential movement from enemy 0 */
  move_idx = 0;
  start_next_move();
}

void EnemiesUpdate( float dt )
{
  if ( turn_state == TURN_IDLE ) return;

  UpdateTweens( &tweens, dt );

  if ( GetActiveTweenCount( &tweens ) == 0 )
  {
    if ( turn_state == TURN_MOVING )
      start_next_move();
    else if ( turn_state == TURN_ATTACK )
      start_next_attack();
  }
}

int EnemiesTurning( void )
{
  return turn_state != TURN_IDLE;
}
