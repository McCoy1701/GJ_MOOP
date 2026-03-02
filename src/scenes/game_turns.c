#include <Archimedes.h>

#include "game_turns.h"
#include "player.h"
#include "items.h"
#include "maps.h"
#include "movement.h"
#include "visibility.h"
#include "combat.h"
#include "combat_vfx.h"
#include "game_events.h"
#include "enemies.h"
#include "npc.h"
#include "ground_items.h"
#include "shop.h"
#include "shop_ui.h"
#include "poison_pool.h"
#include "spell_vfx.h"
#include "room_enumerator.h"
#include "dialogue.h"

extern Player_t player;

static Console_t*      gt_console;
static aSoundEffect_t* gt_sfx_click;
static Enemy_t*        gt_enemies;
static int*            gt_num_enemies;
static NPC_t*          gt_npcs;
static int*            gt_num_npcs;
static GroundItem_t*   gt_items;
static int*            gt_num_items;

/* Turn state */
static struct {
  int   was_moving;
  float enemy_delay;
} turn;

/* Frame-level player tile cache */
static int frame_pr, frame_pc;

/* First-consumable hint arrow */
static int   hint_shown = 0;
static float hint_timer = 0.0f;
#define HINT_DURATION  2.5f

void GameTurnsInit( Console_t* con, aSoundEffect_t* click,
                    Enemy_t* enemies, int* num_enemies,
                    NPC_t* npcs, int* num_npcs,
                    GroundItem_t* items, int* num_items )
{
  gt_console     = con;
  gt_sfx_click   = click;
  gt_enemies     = enemies;
  gt_num_enemies = num_enemies;
  gt_npcs        = npcs;
  gt_num_npcs    = num_npcs;
  gt_items       = items;
  gt_num_items   = num_items;
  turn.was_moving  = 0;
  turn.enemy_delay = 0;
  hint_shown = 0;
  hint_timer = 0.0f;
}

void GameTurnsUpdateSystems( float dt )
{
  MovementUpdate( dt );
  EnemiesUpdate( dt );
  NPCsUpdate( dt );
  EnemyProjectileUpdate( dt );
  CombatUpdate( dt );
  CombatVFXUpdate( dt );
  SpellVFXUpdate( dt );

  PlayerGetTile( &frame_pr, &frame_pc );
  VisibilityUpdate( frame_pr, frame_pc );
}

void GameTurnsHandleTurnEnd( float dt, int turn_skipped )
{
  int turn_ended = ( turn.was_moving && !PlayerIsMoving() ) || turn_skipped;

  if ( turn_ended )
  {
    /* Ground pickup / shop prompt (only if actually moved, not skipped) */
    if ( !turn_skipped )
    {
      ShopItem_t* si = ShopItemAt( frame_pr, frame_pc );
      if ( si )
      {
        ShopUIOpen( (int)( si - g_shop_items ) );
      }
      else
      {
        GroundItem_t* gi = GroundItemAt( gt_items, *gt_num_items,
                                         frame_pr, frame_pc );
        if ( gi )
        {
          if ( gi->item_type == GROUND_EQUIPMENT )
          {
            EquipmentInfo_t* eq = &g_equipment[gi->item_idx];
            if ( !EquipmentClassMatch( gi->item_idx ) )
            {
              ConsolePushF( gt_console, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                            "You can't use %s.", eq->name );
            }
            else
            {
              int slot = InventoryAdd( INV_EQUIPMENT, gi->item_idx );
              if ( slot >= 0 )
              {
                gi->alive = 0;
                a_AudioPlaySound( gt_sfx_click, NULL );
                ConsolePushF( gt_console, eq->color, "Picked up %s.", eq->name );
              }
              else
              {
                ConsolePushF( gt_console, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                              "Inventory full! Can't pick up %s.", eq->name );
              }
            }
          }
          else
          {
            int inv_type = ( gi->item_type == GROUND_MAP ) ? INV_MAP : INV_CONSUMABLE;
            const char* iname;
            aColor_t    icolor;
            if ( gi->item_type == GROUND_MAP )
            {
              iname  = g_maps[gi->item_idx].name;
              icolor = g_maps[gi->item_idx].color;
            }
            else
            {
              iname  = g_consumables[gi->item_idx].name;
              icolor = g_consumables[gi->item_idx].color;
            }

            int slot = InventoryAdd( inv_type, gi->item_idx );
            if ( slot >= 0 )
            {
              gi->alive = 0;
              a_AudioPlaySound( gt_sfx_click, NULL );
              ConsolePushF( gt_console, icolor, "Picked up %s.", iname );

              /* Quest item pickup flags */
              if ( inv_type == INV_CONSUMABLE
                   && strcmp( g_consumables[gi->item_idx].key, "cave_mushroom" ) == 0 )
                FlagIncr( "mushrooms_collected" );

              if ( !hint_shown && inv_type == INV_CONSUMABLE )
              {
                hint_shown = 1;
                hint_timer = HINT_DURATION;
              }
            }
            else
            {
              ConsolePushF( gt_console, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                            "Inventory full! Can't pick up %s.", iname );
            }
          }
        }
      }
    }

    /* Reset trinket effects on room change */
    {
      int room = RoomAt( frame_pr, frame_pc );
      if ( room != player.last_room_id )
        PlayerSetRoom( room );
    }

    /* Turn tick (skip when shop prompt just opened - browsing is free) */
    if ( !ShopUIActive() && !EnemiesTurning() && !NPCsTurning() )
    {
      PoisonPoolTick( frame_pr, frame_pc );
      GameEventsNewTurn();
      PlayerTickTurnsSinceHit();
      for ( int i = 0; i < *gt_num_enemies; i++ )
        if ( gt_enemies[i].alive ) gt_enemies[i].turns_since_hit++;

      if ( EnemiesInCombat( gt_enemies, *gt_num_enemies ) )
      {
        turn.enemy_delay = 0.2f;
      }
      else
      {
        EnemiesStartTurn( gt_enemies, *gt_num_enemies, frame_pr, frame_pc, TileWalkable );
      }

      NPCsCombatTick( gt_npcs, *gt_num_npcs, gt_enemies, *gt_num_enemies );
      NPCsIdleTick( gt_npcs, *gt_num_npcs, gt_console );
    }
  }
  turn.was_moving = PlayerIsMoving();

  /* Enemy turn delay countdown */
  if ( turn.enemy_delay > 0 )
  {
    turn.enemy_delay -= dt;
    if ( turn.enemy_delay <= 0 && !EnemyProjectileInFlight()
         && !SpellVFXActive() )
    {
      turn.enemy_delay = 0;
      EnemiesStartTurn( gt_enemies, *gt_num_enemies, frame_pr, frame_pc, TileWalkable );
    }
  }
}

void GameTurnsGetPlayerTile( int* r, int* c )
{
  *r = frame_pr;
  *c = frame_pc;
}

float GameTurnsEnemyDelay( void )  { return turn.enemy_delay; }
void  GameTurnsSetEnemyDelay( float d ) { turn.enemy_delay = d; }

float GameTurnsHintTimer( void )   { return hint_timer; }
void  GameTurnsTickHint( float dt ) { if ( hint_timer > 0.0f ) hint_timer -= dt; }
