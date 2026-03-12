#include <stdlib.h>
#include <string.h>

#include "enemies.h"
#include "visibility.h"
#include "combat.h"
#include "combat_vfx.h"
#include "spell_vfx.h"
#include "console.h"
#include "game_events.h"
#include "room_enumerator.h"

#define STONE_HEAL_AMOUNT  2
#define STONE_HEAL_RANGE   8   /* Manhattan distance */

/* ---- Stone Healer AI ---- */
/* Immobile. Each turn, heals the most-damaged ally in range. */

void EnemyStoneHealerTick( Enemy_t* e, int player_row, int player_col,
                           int (*walkable)(int,int),
                           Enemy_t* all, int count )
{
  (void)player_row; (void)player_col; (void)walkable;
  if ( !e->alive ) return;
  if ( e->stun_turns > 0 ) return;

  Enemy_t* best = NULL;
  int best_missing = 0;

  for ( int i = 0; i < count; i++ )
  {
    if ( &all[i] == e || !all[i].alive ) continue;
    /* Don't heal other static/stone objects */
    const char* ai = g_enemy_types[all[i].type_idx].ai;
    if ( strcmp( ai, "static" ) == 0 ) continue;
    if ( strcmp( ai, "stone_healer" ) == 0 ) continue;
    if ( strcmp( ai, "stone_ranged" ) == 0 ) continue;

    int dr = abs( all[i].row - e->row );
    int dc = abs( all[i].col - e->col );
    if ( dr + dc > STONE_HEAL_RANGE ) continue;

    int max_hp  = g_enemy_types[all[i].type_idx].hp;
    int missing = max_hp - all[i].hp;
    if ( missing > 0 && missing > best_missing )
    {
      best = &all[i];
      best_missing = missing;
    }
  }

  if ( !best ) return;

  int max_hp = g_enemy_types[best->type_idx].hp;
  int heal = STONE_HEAL_AMOUNT;
  if ( best->hp + heal > max_hp )
    heal = max_hp - best->hp;

  best->hp += heal;
  SpellVFXHeal( e->world_x, e->world_y, best->world_x, best->world_y );
  CombatVFXSpawnNumber( best->world_x, best->world_y, heal,
                        (aColor_t){ 0xc8, 0x50, 0x50, 255 } );
  CombatVFXSpawnText( e->world_x, e->world_y,
                      "Hums!", (aColor_t){ 0xc8, 0x50, 0x50, 255 } );

  EnemyType_t* bt = &g_enemy_types[best->type_idx];
  ConsolePushF( GameEventsGetConsole(), (aColor_t){ 0xc8, 0x50, 0x50, 255 },
                "The Humming Stone heals %s for %d HP.", bt->name, heal );
}

/* ---- Stone Ranged AI ---- */
/* Immobile. Targets player from anywhere. Locks onto a tile, telegraphs, fires. */
/* ai_state: 0,1 = charging, 2 = telegraph (lock tile), 3 = fire at locked tile */
/* ai_dir_row/ai_dir_col store the locked target tile. */

#define SST_CHARGE1    0
#define SST_CHARGE2    1
#define SST_TELEGRAPH  2
#define SST_FIRE       3

/* Stored target tile for rendering the telegraph marker */
static int s_stone_tgt_row = -1;
static int s_stone_tgt_col = -1;
static int s_stone_tgt_active = 0;

int  EnemyStoneTargetActive( void ) { return s_stone_tgt_active; }
int  EnemyStoneTargetRow( void )    { return s_stone_tgt_row; }
int  EnemyStoneTargetCol( void )    { return s_stone_tgt_col; }

void EnemyStoneRangedTick( Enemy_t* e, int player_row, int player_col,
                           int (*walkable)(int,int),
                           Enemy_t* all, int count )
{
  (void)walkable;
  if ( !e->alive ) { s_stone_tgt_active = 0; return; }
  if ( e->stun_turns > 0 ) return;

  /* Only active while the gatekeeper is alive */
  int gk_alive = 0;
  for ( int i = 0; i < count; i++ )
  {
    if ( all[i].alive && strcmp( g_enemy_types[all[i].type_idx].key, "gatekeeper" ) == 0 )
    { gk_alive = 1; break; }
  }
  if ( !gk_alive ) { s_stone_tgt_active = 0; return; }

  /* Only attack when the player is in the same room as the stone */
  int stone_room  = RoomAt( e->row, e->col );
  int player_room = RoomAt( player_row, player_col );
  if ( stone_room <= 0 || player_room != stone_room )
  { s_stone_tgt_active = 0; e->ai_state = SST_CHARGE1; return; }

  switch ( e->ai_state )
  {
    case SST_CHARGE1:
      s_stone_tgt_active = 0;
      e->ai_state = SST_CHARGE2;
      break;

    case SST_CHARGE2:
      e->ai_state = SST_TELEGRAPH;
      /* Lock onto player's current tile */
      e->ai_dir_row = player_row;
      e->ai_dir_col = player_col;
      s_stone_tgt_row = player_row;
      s_stone_tgt_col = player_col;
      s_stone_tgt_active = 1;
      CombatVFXSpawnText( e->world_x, e->world_y,
                          "Charging...", (aColor_t){ 0x50, 0x50, 0xc8, 255 } );
      break;

    case SST_TELEGRAPH:
      /* Target tile stays visible — player has this turn to move */
      e->ai_state = SST_FIRE;
      break;

    case SST_FIRE:
    {
      s_stone_tgt_active = 0;
      float tw = EnemyTileW(), th = EnemyTileH();
      float tx = e->ai_dir_row * tw + tw / 2.0f;
      float ty = e->ai_dir_col * th + th / 2.0f;

      SpellVFXSpark( e->world_x, e->world_y, tx, ty );

      /* Only hit if player is still on the locked tile */
      if ( player_row == e->ai_dir_row && player_col == e->ai_dir_col )
      {
        CombatEnemyHit( e );
        ConsolePushF( GameEventsGetConsole(), (aColor_t){ 0x50, 0x50, 0xc8, 255 },
                      "The Humming Stone blasts you!" );
      }
      else
      {
        ConsolePushF( GameEventsGetConsole(), (aColor_t){ 0x50, 0x50, 0xc8, 255 },
                      "The Humming Stone's bolt misses!" );
      }

      CombatVFXSpawnText( e->world_x, e->world_y,
                          "Hums!", (aColor_t){ 0x50, 0x50, 0xc8, 255 } );
      e->ai_state = SST_CHARGE1;
      break;
    }

    default:
      e->ai_state = SST_CHARGE1;
      break;
  }
}
