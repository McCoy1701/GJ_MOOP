#include <string.h>
#include <Archimedes.h>

#include <stdlib.h>

#include "defines.h"
#include "npc.h"
#include "dialogue.h"
#include "player.h"
#include "draw_utils.h"
#include "game_viewport.h"
#include "world.h"
#include "enemies.h"
#include "combat.h"
#include "combat_vfx.h"
#include "visibility.h"
#include "room_enumerator.h"
#include "tween.h"

extern Player_t player;

/* ---- NPC combat animation state machine ---- */

#define NPC_TURN_IDLE  0
#define NPC_TURN_ANIM  1

static TweenManager_t npc_tweens;
static int npc_turn_state = NPC_TURN_IDLE;

#define NPC_ACT_NONE   0
#define NPC_ACT_MOVE   1
#define NPC_ACT_ATTACK 2

typedef struct
{
  int action;
  int enemy_idx;        /* target enemy (attack) */
  int dest_r, dest_c;   /* destination tile (move) */
} NPCAction_t;

static NPCAction_t npc_actions[MAX_NPCS];
static int npc_action_count = 0;
static int npc_action_idx   = 0;

/* Saved pointers for async processing */
static NPC_t*   t_npcs       = NULL;
static int      t_npc_count  = 0;
static Enemy_t* t_enemies    = NULL;
static int      t_num_enemies = 0;

/* Lunge callback data */
typedef struct
{
  float* target;
  float  home;
} NPCLunge_t;

static NPCLunge_t npc_lunge_data[MAX_NPCS * 2];
static int npc_lunge_count = 0;

static void npc_lunge_back( void* data )
{
  NPCLunge_t* lb = (NPCLunge_t*)data;
  TweenFloat( &npc_tweens, lb->target, lb->home, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

static void start_next_npc_action( void );

static int tweens_inited = 0;

void NPCsInit( NPC_t* list, int* count )
{
  memset( list, 0, sizeof( NPC_t ) * MAX_NPCS );
  *count = 0;
}

NPC_t* NPCSpawn( NPC_t* list, int* count,
                  int type_idx, int row, int col,
                  int tile_w, int tile_h )
{
  if ( *count >= MAX_NPCS || type_idx < 0 || type_idx >= g_num_npc_types )
    return NULL;

  NPC_t* n = &list[*count];
  n->type_idx = type_idx;
  n->row      = row;
  n->col      = col;
  n->world_x   = row * tile_w + tile_w / 2.0f;
  n->world_y   = col * tile_h + tile_h / 2.0f;
  n->alive     = 1;
  n->home_room = RoomAt( row, col );
  ( *count )++;
  return n;
}

NPC_t* NPCAt( NPC_t* list, int count, int row, int col )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( list[i].alive && list[i].row == row && list[i].col == col )
      return &list[i];
  }
  return NULL;
}

void NPCsDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                   NPC_t* list, int count,
                   World_t* world, int gfx_mode )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;
    NPCType_t* nt = &g_npc_types[list[i].type_idx];

    /* Face toward the player */
    int face_left = ( player.world_x < list[i].world_x );

    /* Shadow */
    GV_DrawFilledRect( vp_rect, cam,
                       list[i].world_x, list[i].world_y + 8.0f,
                       10.0f, 3.0f,
                       (aColor_t){ 0, 0, 0, 80 } );

    if ( nt->image && gfx_mode == GFX_IMAGE )
    {
      if ( face_left )
        GV_DrawSpriteFlipped( vp_rect, cam, nt->image,
                              list[i].world_x, list[i].world_y,
                              (float)world->tile_w, (float)world->tile_h, 'x' );
      else
        GV_DrawSprite( vp_rect, cam, nt->image,
                       list[i].world_x, list[i].world_y,
                       (float)world->tile_w, (float)world->tile_h );
    }
    else
    {
      float sx, sy;
      GV_WorldToScreen( vp_rect, cam,
                        list[i].world_x - world->tile_w / 2.0f,
                        list[i].world_y - world->tile_h / 2.0f,
                        &sx, &sy );
      float half_w = cam->half_h * ( vp_rect.w / vp_rect.h );
      int dw = (int)( world->tile_w * ( vp_rect.w / ( half_w * 2.0f ) ) );
      int dh = (int)( world->tile_h * ( vp_rect.h / ( cam->half_h * 2.0f ) ) );

      a_DrawGlyph( d_StringPeek( nt->glyph ), (int)sx, (int)sy, dw, dh,
                   nt->color, (aColor_t){ 0, 0, 0, 0 }, FONT_CODE_PAGE_437 );
    }
  }
}

/* ---- Animated combat turn ---- */

static void start_next_npc_action( void )
{
  while ( npc_action_idx < npc_action_count )
  {
    int i = npc_action_idx;
    npc_action_idx++;
    NPCAction_t* act = &npc_actions[i];

    if ( act->action == NPC_ACT_ATTACK )
    {
      NPC_t* n = &t_npcs[i];
      if ( !n->alive ) continue;
      NPCType_t* nt = &g_npc_types[n->type_idx];
      Enemy_t* target = &t_enemies[act->enemy_idx];

      /* Deal damage */
      target->hp -= nt->damage;
      target->turns_since_hit = 0;
      CombatVFXSpawnNumber( target->world_x, target->world_y,
                            nt->damage, (aColor_t){ 0xcf, 0x57, 0x3c, 255 } );

      /* Combat bark */
      if ( d_StringGetLength( nt->combat_bark ) > 0 )
        CombatVFXSpawnText( n->world_x, n->world_y,
                            d_StringPeek( nt->combat_bark ), nt->color );

      if ( target->hp <= 0 )
        CombatHandleEnemyDeath( target );

      /* Lunge toward target */
      int dr = target->row - n->row;
      int dc = target->col - n->col;
      float lunge_dist = 3.0f;
      npc_lunge_count = 0;

      NPCLunge_t* lbx = &npc_lunge_data[npc_lunge_count++];
      lbx->target = &n->world_x;
      lbx->home   = n->world_x;
      TweenFloatWithCallback( &npc_tweens, &n->world_x,
                              n->world_x + dr * lunge_dist,
                              0.06f, TWEEN_EASE_OUT_QUAD,
                              npc_lunge_back, lbx );

      NPCLunge_t* lby = &npc_lunge_data[npc_lunge_count++];
      lby->target = &n->world_y;
      lby->home   = n->world_y;
      TweenFloatWithCallback( &npc_tweens, &n->world_y,
                              n->world_y + dc * lunge_dist,
                              0.06f, TWEEN_EASE_OUT_QUAD,
                              npc_lunge_back, lby );

      npc_turn_state = NPC_TURN_ANIM;
      return;
    }

    if ( act->action == NPC_ACT_MOVE )
    {
      NPC_t* n = &t_npcs[i];
      if ( !n->alive ) continue;

      n->row = act->dest_r;
      n->col = act->dest_c;
      float tx = act->dest_r * 16 + 8.0f;
      float ty = act->dest_c * 16 + 8.0f;
      TweenFloat( &npc_tweens, &n->world_x, tx, 0.15f, TWEEN_EASE_OUT_CUBIC );
      TweenFloat( &npc_tweens, &n->world_y, ty, 0.15f, TWEEN_EASE_OUT_CUBIC );

      npc_turn_state = NPC_TURN_ANIM;
      return;
    }
  }

  /* All actions processed */
  npc_turn_state = NPC_TURN_IDLE;
}

void NPCsCombatTick( NPC_t* list, int count,
                     Enemy_t* enemies, int num_enemies )
{
  static const int dx[] = { 1, -1, 0, 0 };
  static const int dy[] = { 0, 0, 1, -1 };

  if ( !tweens_inited )
  {
    InitTweenManager( &npc_tweens );
    tweens_inited = 1;
  }

  t_npcs        = list;
  t_npc_count   = count;
  t_enemies     = enemies;
  t_num_enemies = num_enemies;
  npc_action_count = count;
  npc_action_idx   = 0;

  /* Pre-compute all actions */
  for ( int i = 0; i < count; i++ )
  {
    npc_actions[i].action = NPC_ACT_NONE;

    if ( !list[i].alive ) continue;
    NPCType_t* nt = &g_npc_types[list[i].type_idx];
    if ( !nt->combat ) continue;

    int nr = list[i].row;
    int nc = list[i].col;
    int home = list[i].home_room;

    /* Find closest enemy in the same room */
    int best_idx  = -1;
    int best_dist = 9999;
    for ( int e = 0; e < num_enemies; e++ )
    {
      if ( !enemies[e].alive ) continue;
      if ( RoomAt( enemies[e].row, enemies[e].col ) != home ) continue;
      int d = abs( enemies[e].row - nr ) + abs( enemies[e].col - nc );
      if ( d < best_dist )
      {
        best_dist = d;
        best_idx  = e;
      }
    }

    if ( best_idx < 0 ) continue;

    /* Adjacent - attack */
    if ( best_dist <= 1 )
    {
      npc_actions[i].action    = NPC_ACT_ATTACK;
      npc_actions[i].enemy_idx = best_idx;
      continue;
    }

    /* Move toward target, stay in room */
    Enemy_t* target = &enemies[best_idx];
    int cur_dist = best_dist;
    int move_r = nr, move_c = nc;
    for ( int d = 0; d < 4; d++ )
    {
      int tr = nr + dx[d];
      int tc = nc + dy[d];
      if ( RoomAt( tr, tc ) != home ) continue;
      if ( EnemyAt( enemies, num_enemies, tr, tc ) ) continue;
      if ( NPCAt( list, count, tr, tc ) ) continue;
      if ( tr == (int)( player.world_x / 16 ) &&
           tc == (int)( player.world_y / 16 ) ) continue;
      int nd = abs( target->row - tr ) + abs( target->col - tc );
      if ( nd < cur_dist )
      {
        cur_dist = nd;
        move_r = tr;
        move_c = tc;
      }
    }

    if ( move_r != nr || move_c != nc )
    {
      npc_actions[i].action = NPC_ACT_MOVE;
      npc_actions[i].dest_r = move_r;
      npc_actions[i].dest_c = move_c;
    }
  }

  /* Kick off first action */
  start_next_npc_action();
}

void NPCsUpdate( float dt )
{
  if ( npc_turn_state == NPC_TURN_IDLE ) return;
  UpdateTweens( &npc_tweens, dt );
  if ( GetActiveTweenCount( &npc_tweens ) == 0 )
    start_next_npc_action();
}

int NPCsTurning( void )
{
  return npc_turn_state != NPC_TURN_IDLE;
}

void NPCsIdleTick( NPC_t* list, int count, Console_t* con )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;
    NPCType_t* nt = &g_npc_types[list[i].type_idx];
    if ( nt->idle_cooldown <= 0 || d_StringGetLength( nt->idle_bark ) == 0 ) continue;

    if ( list[i].idle_cd > 0 )
    {
      list[i].idle_cd--;
      continue;
    }

    /* Only bark if the player can see the NPC */
    if ( VisibilityGet( list[i].row, list[i].col ) < 0.01f ) continue;

    /* ~30% chance each eligible turn */
    if ( rand() % 100 < 30 )
    {
      CombatVFXSpawnText( list[i].world_x, list[i].world_y,
                          d_StringPeek( nt->idle_bark ), nt->color );

      if ( con && d_StringGetLength( nt->idle_log ) > 0 )
        ConsolePush( con, d_StringPeek( nt->idle_log ), nt->color );

      list[i].idle_cd = nt->idle_cooldown;
    }
  }
}
