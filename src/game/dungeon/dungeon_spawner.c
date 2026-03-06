#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>

#include "dungeon.h"
#include "dungeon_spawner.h"
#include "dialogue.h"
#include "player.h"
#include "items.h"
#include "movement.h"
#include "shop.h"

extern Player_t player;

/* ====== Shared spawn helpers ====== */

int SpawnerGetClassIdx( void )
{
  for ( int i = 0; i < 3; i++ )
    if ( strcmp( player.name, g_classes[i].name ) == 0 ) return i;
  return 0;
}

void SpawnRandomConsumable( GroundItem_t* items, int* num_items,
                            int row, int col, int tw, int th )
{
  FilteredItem_t f[16];
  int nf = ItemsBuildFiltered( SpawnerGetClassIdx(), f, 16, 0 );
  int cons[3], nc = 0;
  for ( int i = 0; i < nf && nc < 3; i++ )
  {
    if ( f[i].type == FILTERED_CONSUMABLE )
      cons[nc++] = f[i].index;
  }
  if ( nc > 0 )
    GroundItemSpawn( items, num_items, cons[rand() % nc], row, col, tw, th );
}

void SpawnRandomT2Consumable( GroundItem_t* items, int* num_items,
                              int row, int col, int tw, int th )
{
  FilteredItem_t f[16];
  int nf = ItemsBuildFiltered( SpawnerGetClassIdx(), f, 16, 0 );
  int skip = 0, nc = 0;
  int cons[3];
  for ( int i = 0; i < nf && nc < 3; i++ )
  {
    if ( f[i].type != FILTERED_CONSUMABLE ) continue;
    if ( skip < 3 ) { skip++; continue; }  /* skip tier 1 */
    cons[nc++] = f[i].index;
  }
  if ( nc > 0 )
    GroundItemSpawn( items, num_items, cons[rand() % nc], row, col, tw, th );
}

void SpawnRandomEnemy( Enemy_t* enemies, int* num_enemies,
                       int x, int y, int tw, int th )
{
  static const char* types[] = { "rat", "skeleton", "slime" };
  EnemySpawn( enemies, num_enemies,
              EnemyTypeByKey( types[rand() % 3] ), x, y, tw, th );
}

void SpawnClassElite( Enemy_t* enemies, int* num_enemies,
                      int x, int y, int tw, int th )
{
  const char* cls = PlayerClassKey();
  const char* key;

  if ( strcmp( cls, "mercenary" ) == 0 )      key = "elite_merc";
  else if ( strcmp( cls, "rogue" ) == 0 )     key = "elite_rogue";
  else if ( strcmp( cls, "mage" ) == 0 )      key = "elite_mage";
  else return;

  int ti = EnemyTypeByKey( key );
  if ( ti >= 0 )
    EnemySpawn( enemies, num_enemies, ti, x, y, tw, th );
}

void SpawnT2Consumable( GroundItem_t* items, int* num_items,
                        int x, int y, int tw, int th )
{
  const char* cls = PlayerClassKey();
  const char* keys[3];

  if ( strcmp( cls, "mercenary" ) == 0 )
  {
    keys[0] = "iron_loaf";  keys[1] = "mutton_leg";  keys[2] = "spiced_stew";
  }
  else if ( strcmp( cls, "rogue" ) == 0 )
  {
    keys[0] = "steel_arrow"; keys[1] = "venom_arrow"; keys[2] = "serrated_trap";
  }
  else
  {
    keys[0] = "scroll_lightning"; keys[1] = "scroll_blizzard"; keys[2] = "scroll_inferno";
  }

  int idx = ConsumableByKey( keys[rand() % 3] );
  if ( idx >= 0 )
    GroundItemSpawn( items, num_items, idx, x, y, tw, th );
}

/* ====== Deferred spawns (flag-triggered, called per frame) ====== */

void DungeonDeferredSpawns( NPC_t* npcs, int num_npcs,
                            Enemy_t* enemies, int* num_enemies,
                            World_t* world )
{
  if ( DialogueActive() ) return;

  int tw = world->tile_w, th = world->tile_h;

  /* Floor 2: Hop barricade — teleport player south, spawn Horror */
  if ( g_current_floor == 2
       && FlagGet( "barricade_hopped" ) && !FlagGet( "horror_spawned" ) )
  {
    FlagSet( "horror_spawned", 1 );
    PlayerSetWorldPos( 59 * tw + tw / 2.0f, 41 * th + th / 2.0f );
    EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
                59, 47, tw, th );
  }

  /* Floor 3: Church door — unlocks when Greta escorts or is defied */
  if ( g_current_floor == 3
       && ( FlagGet( "church_escort" ) || FlagGet( "greta_defied" ) )
       && !FlagGet( "church_door_unlocked" ) )
  {
    FlagSet( "church_door_unlocked", 1 );
  }

  /* Floor 3: Greta defied — Horror spawns at her old position */
  if ( g_current_floor == 3
       && FlagGet( "greta_defied" )
       && !FlagGet( "greta_defy_horror" ) )
  {
    FlagSet( "greta_defy_horror", 1 );

    /* Horror at Greta's old position */
    EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
                33, 71, tw, th );
  }

  /* Floor 3: Church door opened after defy — cultists go hostile */
  if ( g_current_floor == 3
       && FlagGet( "church_hostile_open" )
       && !FlagGet( "greta_horror_spawned" ) )
  {
    FlagSet( "greta_horror_spawned", 1 );

    int cult_npc = NPCTypeByKey( "cultist" );
    int cult_enemy = EnemyTypeByKey( "cultist" );
    for ( int i = 0; i < num_npcs; i++ )
    {
      if ( npcs[i].alive && npcs[i].type_idx == cult_npc )
      {
        EnemySpawn( enemies, num_enemies, cult_enemy,
                    npcs[i].row, npcs[i].col, tw, th );
        npcs[i].alive = 0;
      }
    }
  }

  /* Floor 3: Greta betrayed — Horror at church + cultists hostile immediately */
  if ( g_current_floor == 3
       && FlagGet( "greta_betrayed" )
       && !FlagGet( "greta_horror_spawned" ) )
  {
    FlagSet( "greta_horror_spawned", 1 );

    /* Horror at church escort position */
    EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
                42, 49, tw, th );

    /* Swap all cultist NPCs for hostile cultist enemies */
    int cult_npc = NPCTypeByKey( "cultist" );
    int cult_enemy = EnemyTypeByKey( "cultist" );
    for ( int i = 0; i < num_npcs; i++ )
    {
      if ( npcs[i].alive && npcs[i].type_idx == cult_npc )
      {
        EnemySpawn( enemies, num_enemies, cult_enemy,
                    npcs[i].row, npcs[i].col, tw, th );
        npcs[i].alive = 0;
      }
    }
  }
}

/* ====== Dispatcher ====== */

void DungeonSpawn( NPC_t* npcs, int* num_npcs,
                   Enemy_t* enemies, int* num_enemies,
                   GroundItem_t* items, int* num_items,
                   World_t* world )
{
  extern int g_current_floor;

  if ( g_current_floor >= 3 )
    SpawnFloor3( npcs, num_npcs, enemies, num_enemies,
                 items, num_items, world );
  else if ( g_current_floor == 2 )
    SpawnFloor2( npcs, num_npcs, enemies, num_enemies,
                 items, num_items, world );
  else
    SpawnFloor1( npcs, num_npcs, enemies, num_enemies,
                 items, num_items, world );
}
