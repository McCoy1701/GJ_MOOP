#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>

#include "enemies.h"
#include "combat.h"
#include "world.h"
#include "tween.h"

static World_t* world = NULL;
static TweenManager_t tweens;

/* Turn state machine */
#define TURN_IDLE     0
#define TURN_MOVING   1   /* move tweens in progress */
#define TURN_ATTACK   2   /* lunge tweens in progress */

static int turn_state = TURN_IDLE;

/* Saved from EnemiesStartTurn for use in phase 2 */
static Enemy_t* turn_list  = NULL;
static int      turn_count = 0;
static int      turn_pr, turn_pc;

void EnemiesSetWorld( World_t* w )
{
  world = w;
  InitTweenManager( &tweens );
  turn_state = TURN_IDLE;
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

/* --- Phase 2: adjacent enemies lunge + attack --- */

static void start_attacks( void )
{
  lunge_count = 0;

  for ( int i = 0; i < turn_count; i++ )
  {
    if ( !turn_list[i].alive ) continue;
    int dr = turn_pr - turn_list[i].row;
    int dc = turn_pc - turn_list[i].col;
    if ( abs( dr ) + abs( dc ) != 1 ) continue;

    float lunge_dist = 3.0f;

    if ( lunge_count + 1 < MAX_ENEMIES * 2 )
    {
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
    }

    CombatEnemyHit( &turn_list[i] );
  }

  if ( lunge_count > 0 )
    turn_state = TURN_ATTACK;
  else
    turn_state = TURN_IDLE;
}

/* --- Public API --- */

void EnemiesStartTurn( Enemy_t* list, int count,
                       int player_row, int player_col,
                       int (*walkable)(int,int) )
{
  turn_list  = list;
  turn_count = count;
  turn_pr    = player_row;
  turn_pc    = player_col;

  int moved = 0;

  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;
    EnemyType_t* t = &g_enemy_types[list[i].type_idx];

    int old_row = list[i].row;
    int old_col = list[i].col;

    if ( strcmp( t->ai, "basic" ) == 0 )
      EnemyRatTick( &list[i], player_row, player_col, walkable, list, count );

    if ( list[i].row != old_row || list[i].col != old_col )
    {
      float tx = list[i].row * world->tile_w + world->tile_w / 2.0f;
      float ty = list[i].col * world->tile_h + world->tile_h / 2.0f;
      TweenFloat( &tweens, &list[i].world_x, tx, 0.2f, TWEEN_EASE_OUT_CUBIC );
      TweenFloat( &tweens, &list[i].world_y, ty, 0.2f, TWEEN_EASE_OUT_CUBIC );
      moved++;
    }
  }

  if ( moved > 0 )
    turn_state = TURN_MOVING;
  else
    start_attacks();  /* No movement — go straight to attacks */
}

void EnemiesUpdate( float dt )
{
  if ( turn_state == TURN_IDLE ) return;

  UpdateTweens( &tweens, dt );

  if ( GetActiveTweenCount( &tweens ) == 0 )
  {
    if ( turn_state == TURN_MOVING )
      start_attacks();
    else if ( turn_state == TURN_ATTACK )
      turn_state = TURN_IDLE;
  }
}

int EnemiesTurning( void )
{
  return turn_state != TURN_IDLE;
}
