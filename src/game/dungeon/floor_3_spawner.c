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

  int cult_e = EnemyTypeByKey( "cultist" );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "void_slime" ), 37, 10, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "cult_caster" ), 16, 36, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 21, 28, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "cult_caster" ), 31, 27, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "cult_caster" ), 36, 27, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "elder_horror" ), 28, 3, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "elder_horror" ), 56, 11, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "elder_horror" ), 52, 6, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "elder_horror" ), 54, 16, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "void_slime" ), 20, 4, tw, th );

  /* The Sentry - automaton before the Gatekeeper's chamber */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "sentry" ),
            16, 12, tw, th );

  /* Sentry's Door - blocks entry to boss room until gate_opened */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "sentrys_door" ),
            11, 12, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "sentrys_door" ),
            11, 13, tw, th );

  /* Keeper's Gate - blocks exit until gatekeeper is dead */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "keepers_gate" ),
            4, 9, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "keepers_gate" ),
            5, 9, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "keepers_gate" ),
            6, 9, tw, th );

  /* Victory altars - one per class, past the Keeper's Gate */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "end_table_red" ),
            4, 2, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "end_table_green" ),
            5, 2, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "end_table_blue" ),
            6, 2, tw, th );

  /* The Gatekeeper - final boss */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "gatekeeper" ),
              7, 11, tw, th );

  /* Humming Stones - red (healer), green (buff), blue (ranged) */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "hum_stone_red" ),
              5, 12, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "hum_stone_green" ),
              7, 12, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "hum_stone_blue" ),
              7, 10, tw, th );

  /* Greta's Door - blocks passage until Greta is killed */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "gretas_door" ),
            45, 16, tw, th );

  /* Ex-cultist town (room t) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "muri" ),
            60, 52, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "drem" ),
            55, 53, tw, th );
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "murl" ),
            57, 49, tw, th );
  ShopSpawn( world );

  /* Second shop - consumable-only rug near the sentry */
  {
    static const int rug2[6][2] = {
      {17,12}, {18,12}, {19,12},
      {17,13}, {18,13}, {19,13}
    };
    ShopSpawnConsumableRug( world, rug2 );
  }

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
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              64, 64, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              70, 58, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "horror" ),
              55, 35, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              54, 33, tw, th );

  /* Baby horrors */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              59, 26, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              61, 26, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "lost_horror" ),
              43, 22, tw, th );

  /* Cultist enemies (room w) */
  EnemySpawn( enemies, num_enemies, cult_e, 20, 44, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 22, 44, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 54, 40, tw, th );
  EnemySpawn( enemies, num_enemies, cult_e, 59, 43, tw, th );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "cult_caster" ), 61, 43, tw, th );

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

  /* Lost horrors (enemies only — consumable drops are in DUF) */
  {
    int lh = EnemyTypeByKey( "lost_horror" );
    EnemySpawn( enemies, num_enemies, lh, 45, 12, tw, th );
    EnemySpawn( enemies, num_enemies, lh, 67, 46, tw, th );
    EnemySpawn( enemies, num_enemies, lh, 67, 40, tw, th );
    EnemySpawn( enemies, num_enemies, lh, 57, 39, tw, th );
  }

  /* Consumable/item spawns from DUF */
  DungeonSpawnFromDUF( "resources/data/floors/floor_03/floor_03_consumable_spawns.duf",
                       items, num_items, world );

  /* Void portals */
  ITilePlace( world, 38, 9, ITILE_VOID_PORTAL );
  ITilePlace( world, 42, 13, ITILE_VOID_PORTAL );
  ITilePlace( world, 47, 9, ITILE_VOID_PORTAL );
  ITilePlace( world, 38, 4, ITILE_VOID_PORTAL );
  ITilePlace( world, 17, 4, ITILE_VOID_PORTAL );
  ITilePlace( world, 22, 5, ITILE_VOID_PORTAL );
  ITilePlace( world, 50, 26, ITILE_VOID_PORTAL );
  ITilePlace( world, 60, 31, ITILE_VOID_PORTAL );
  ITileVoidPortalScatter();

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
