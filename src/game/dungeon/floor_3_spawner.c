#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>

#include "dungeon.h"
#include "dungeon_spawner.h"
#include "interactive_tile.h"
#include "player.h"
#include "items.h"
#include "movement.h"
#include "shop.h"

void SpawnFloor3( NPC_t* npcs, int* num_npcs,
                  Enemy_t* enemies, int* num_enemies,
                  GroundItem_t* items, int* num_items,
                  World_t* world )
{
  int tw = world->tile_w, th = world->tile_h;

  int cult = NPCTypeByKey( "cultist" );

  /* Greta - in her chamber west of room 0, through the W door */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "greta" ),
            33, 71, tw, th );

  /* Church door - blocks entry until Greta opens the way */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "church_door" ),
            42, 69, tw, th );

  /* Church door 2 - blocks exit until betrayal/defy */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "church_door2" ),
            42, 47, tw, th );

  /* Cultists - church hall and corridors */
  NPCSpawn( npcs, num_npcs, cult, 37, 64, tw, th );
  NPCSpawn( npcs, num_npcs, cult, 42, 65, tw, th );
  NPCSpawn( npcs, num_npcs, cult, 47, 64, tw, th );
  NPCSpawn( npcs, num_npcs, cult, 40, 55, tw, th );
  NPCSpawn( npcs, num_npcs, cult, 44, 55, tw, th );
  NPCSpawn( npcs, num_npcs, cult, 44, 51, tw, th );
  NPCSpawn( npcs, num_npcs, cult, 40, 51, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "cultist" ), 49, 22, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "cultist" ), 49, 21, tw, th );

  /* Ex-cultist town (room t) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "muri" ),
            60, 52, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "drem" ),
            55, 53, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "murl" ),
            57, 49, tw, th );
  ShopSpawn( world );

  /* Found Horror — Bloop (room )) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "found_horror" ),
            15, 55, tw, th );

  /* Lost Horror (room () */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              15, 52, tw, th );

  /* Horrors */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              50, 70, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              54, 58, tw, th );
  SpawnRandomT2Consumable( items, num_items, 54, 57, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              64, 64, tw, th );
  SpawnRandomT2Consumable( items, num_items, 64, 63, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              70, 58, tw, th );
  SpawnRandomT2Consumable( items, num_items, 70, 57, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              55, 35, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              54, 33, tw, th );
  SpawnRandomT2Consumable( items, num_items, 52, 36, tw, th );

  /* Baby horrors */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              59, 26, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              61, 26, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              43, 22, tw, th );

  SpawnRandomT2Consumable( items, num_items, 49, 26, tw, th );

  /* Cultist enemies (room w) */
  int cult_e = EnemyTypeByKey( "cultist" );
  EnemySpawn( enemies, num_enemies, cult_e, 20, 44, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 22, 44, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 54, 40, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 59, 43, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 61, 43, tw, th );

  /* Void slimes */
  int vs = EnemyTypeByKey( "void_slime" );
  EnemySpawn( enemies, num_enemies, vs, 34, 47, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 34, 43, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 39, 40, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 50, 44, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 62, 58, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 17, 64, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 9, 64, tw, th );
  EnemySpawn( enemies, num_enemies, vs, 68, 33, tw, th );

  /* Small health potion */
  {
    int shp = ConsumableByKey( "small_health_potion" );
    if ( shp >= 0 )
    {
      GroundItemSpawn( items, num_items, shp, 61, 58, tw, th );
      GroundItemSpawn( items, num_items, shp, 61, 29, tw, th );
    }
  }

  /* T2 consumables beside slimes */
  SpawnRandomT2Consumable( items, num_items, 35, 47, tw, th );
  SpawnRandomT2Consumable( items, num_items, 35, 43, tw, th );
  SpawnRandomT2Consumable( items, num_items, 40, 40, tw, th );

  /* Medium health potion */
  {
    int mhp = ConsumableByKey( "medium_health_potion" );
    if ( mhp >= 0 )
      GroundItemSpawn( items, num_items, mhp, 41, 44, tw, th );
  }

  SpawnRandomT2Consumable( items, num_items, 26, 43, tw, th );
  SpawnRandomT2Consumable( items, num_items, 49, 43, tw, th );

  /* Medium health potion - entry area */
  {
    int mhp2 = ConsumableByKey( "medium_health_potion" );
    if ( mhp2 >= 0 )
      GroundItemSpawn( items, num_items, mhp2, 42, 72, tw, th );
  }

  /* T2 consumables */
  SpawnRandomT2Consumable( items, num_items, 55, 66, tw, th );
  SpawnRandomT2Consumable( items, num_items, 53, 72, tw, th );
  SpawnRandomT2Consumable( items, num_items, 67, 70, tw, th );
  SpawnRandomT2Consumable( items, num_items, 42, 60, tw, th );
  SpawnRandomConsumable( items, num_items, 13, 59, tw, th );
  SpawnRandomT2Consumable( items, num_items, 57, 29, tw, th );
  SpawnRandomT2Consumable( items, num_items, 66, 33, tw, th );
  {
    int bag_idx = ConsumableByKey( "bag" );
    if ( bag_idx >= 0 )
      GroundItemSpawn( items, num_items, bag_idx, 67, 33, tw, th );
  }

  /* Max Health pickup */
  {
    int mh_idx = ConsumableByKey( "max_health" );
    if ( mh_idx >= 0 )
      GroundItemSpawn( items, num_items, mh_idx, 60, 26, tw, th );
  }

  /* Lost horrors + class-based rare consumable */
  {
    int lh = EnemyTypeByKey( "lost_horror" );
    const char* cls = PlayerClassKey();
    const char* rare;
    if ( strcmp( cls, "mercenary" ) == 0 )      rare = "dragon_pepper";
    else if ( strcmp( cls, "rogue" ) == 0 )     rare = "smoke_bomb";
    else                                         rare = "scroll_teleport";

    int idx;
    EnemySpawn( enemies, num_enemies, lh, 67, 46, tw, th );
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 67, 45, tw, th );

    EnemySpawn( enemies, num_enemies, lh, 67, 40, tw, th );
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 67, 39, tw, th );

    EnemySpawn( enemies, num_enemies, lh, 57, 39, tw, th );
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 57, 38, tw, th );
  }

  /* Large sack */
  {
    int sack_idx = ConsumableByKey( "sack" );
    if ( sack_idx >= 0 )
      GroundItemSpawn( items, num_items, sack_idx, 18, 66, tw, th );
  }
  SpawnRandomT2Consumable( items, num_items, 16, 66, tw, th );

  /* Urns - placed by the cult along corridors and flanking doorways */
  ITilePlace( world, 40, 53, ITILE_URN );   /* entry corridor, near top */
  ITilePlace( world, 57, 35, ITILE_URN );   /* entry corridor, before turn */
  ITilePlace( world, 48, 45, ITILE_URN );   /* flanking corridor to room q */
  ITilePlace( world, 27, 43, ITILE_URN );   /* west passage near room 2 */
  ITilePlace( world, 44, 53, ITILE_URN );   /* church hall corridor */
  ITilePlace( world, 15, 49, ITILE_URN );   /* west corridor near church */
  ITilePlace( world, 44, 66, ITILE_URN );   /* south corridor near rooms 0/1 */
  ITilePlace( world, 40, 66, ITILE_URN );   /* south corridor, paired */
  ITileUrnScatterGold();
}
