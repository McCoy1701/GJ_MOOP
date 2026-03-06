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

extern Player_t player;

void SpawnFloor1( NPC_t* npcs, int* num_npcs,
                  Enemy_t* enemies, int* num_enemies,
                  GroundItem_t* items, int* num_items,
                  World_t* world )
{
  /* Randomized central room layout:
     4 near-corner positions -> shuffle -> 3 get consumables, 1 gets NPC */
  {
    int corners[4][2] = { {12,16}, {16,16}, {12,20}, {16,20} };

    /* Fisher-Yates shuffle */
    for ( int i = 3; i > 0; i-- )
    {
      int j = rand() % ( i + 1 );
      int tr = corners[i][0], tc = corners[i][1];
      corners[i][0] = corners[j][0]; corners[i][1] = corners[j][1];
      corners[j][0] = tr;            corners[j][1] = tc;
    }

    /* Determine class index for filtered consumables */
    int class_idx = 0;
    for ( int i = 0; i < 3; i++ )
    {
      if ( strcmp( player.name, g_classes[i].name ) == 0 )
      { class_idx = i; break; }
    }

    /* Get class-appropriate consumables */
    FilteredItem_t filtered[16];
    int num_filtered = ItemsBuildFiltered( class_idx, filtered, 16, 0 );

    /* Spawn up to 3 consumables at first 3 shuffled corners */
    int spawned = 0;
    for ( int i = 0; i < num_filtered && spawned < 3; i++ )
    {
      if ( filtered[i].type == FILTERED_CONSUMABLE )
      {
        GroundItemSpawn( items, num_items,
                         filtered[i].index,
                         corners[spawned][0], corners[spawned][1],
                         world->tile_w, world->tile_h );
        spawned++;
      }
    }

    /* 4th corner gets Graf */
    NPCSpawn( npcs, num_npcs, NPCTypeByKey( "graf" ),
              corners[3][0], corners[3][1],
              world->tile_w, world->tile_h );
  }

  /* Spawn Jonathon in his studio */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "jonathon" ),
            23, 4, world->tile_w, world->tile_h );

  /* Spawn enemies at random walkable floor tiles in central room,
     avoiding player (14,18), NPC, and ground item positions */
  {
    /* Collect available floor tiles in central room (x=11..17, y=15..21) */
    int avail[64][2];
    int num_avail = 0;
    for ( int x = 11; x <= 17 && num_avail < 64; x++ )
    {
      for ( int y = 15; y <= 21 && num_avail < 64; y++ )
      {
        if ( x == 14 && y == 18 ) continue; /* player start */
        if ( !TileWalkable( x, y ) ) continue;
        if ( NPCAt( npcs, *num_npcs, x, y ) ) continue;
        if ( GroundItemAt( items, *num_items, x, y ) ) continue;
        avail[num_avail][0] = x;
        avail[num_avail][1] = y;
        num_avail++;
      }
    }

    /* Pick 2 random positions for rat + skeleton */
    if ( num_avail >= 2 )
    {
      int i1 = rand() % num_avail;
      EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
                  avail[i1][0], avail[i1][1],
                  world->tile_w, world->tile_h );

      /* Remove used position */
      avail[i1][0] = avail[num_avail - 1][0];
      avail[i1][1] = avail[num_avail - 1][1];
      num_avail--;

      int i2 = rand() % num_avail;
      EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
                  avail[i2][0], avail[i2][1],
                  world->tile_w, world->tile_h );
    }
  }

  /* New room (southwest, off green-door corridor) */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              7, 26, world->tile_w, world->tile_h );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              6, 25, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 8, 27,
                         world->tile_w, world->tile_h );

  /* Skeleton in the gallery (below Jonathon's studio) */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              23, 11, world->tile_w, world->tile_h );

  /* Undead miners in class-locked rooms */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "miner_mat" ),
              2, 8, world->tile_w, world->tile_h );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "miner_jake" ),
              17, 6, world->tile_w, world->tile_h );
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "miner_chris" ),
              22, 23, world->tile_w, world->tile_h );

  /* Rats in side rooms */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              14, 9, world->tile_w, world->tile_h );   /* north */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              14, 27, world->tile_w, world->tile_h );  /* south */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              4, 18, world->tile_w, world->tile_h );   /* west */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              24, 18, world->tile_w, world->tile_h );  /* east */

  /* Slimes + consumable loot */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              4, 32, world->tile_w, world->tile_h );    /* room ! */
  SpawnRandomConsumable( items, num_items, 3, 31,
                         world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              2, 12, world->tile_w, world->tile_h );    /* room 4 (merc) */
  SpawnRandomConsumable( items, num_items, 3, 11,
                         world->tile_w, world->tile_h );

  /* Red corridor encounters */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              10, 37, world->tile_w, world->tile_h );   /* red hallway blocker */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              13, 39, world->tile_w, world->tile_h );   /* red corridor room */

  /* Blue corridor encounter */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              25, 37, world->tile_w, world->tile_h );   /* blue corridor */

  /* Green corridor encounter */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              29, 38, world->tile_w, world->tile_h );   /* green corridor */

  /* South wing chambers (separate class paths) */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              13, 45, world->tile_w, world->tile_h );   /* red chamber */
  SpawnRandomConsumable( items, num_items, 12, 45,
                         world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              23, 45, world->tile_w, world->tile_h );   /* blue chamber */
  SpawnRandomConsumable( items, num_items, 22, 46,
                         world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              32, 45, world->tile_w, world->tile_h );   /* green chamber */
  SpawnRandomConsumable( items, num_items, 31, 46,
                         world->tile_w, world->tile_h );

  SpawnRandomConsumable( items, num_items, 5, 45,
                         world->tile_w, world->tile_h ); /* white chamber reward */

  /* Rogue Annex */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              29, 13, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 30, 12,
                         world->tile_w, world->tile_h );

  /* Merc Annex */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              30, 18, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 31, 17,
                         world->tile_w, world->tile_h );

  /* Merc Upper Barracks */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              30, 3, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 31, 2,
                         world->tile_w, world->tile_h );

  /* Merc Lower Barracks */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              30, 8, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 31, 7,
                         world->tile_w, world->tile_h );

  /* Class-favored health potions (reachable by all, shortcut for one) */
  {
    int hp_idx = ConsumableByKey( "small_health_potion" );
    if ( hp_idx >= 0 )
    {
      GroundItemSpawn( items, num_items, hp_idx, 26, 5,
                       world->tile_w, world->tile_h );   /* merc barracks corridor */
      GroundItemSpawn( items, num_items, hp_idx, 22, 11,
                       world->tile_w, world->tile_h );   /* gallery (rogue shortcut) */
      GroundItemSpawn( items, num_items, hp_idx, 30, 24,
                       world->tile_w, world->tile_h );   /* east corridor (mage shortcut) */
      GroundItemSpawn( items, num_items, hp_idx, 21, 24,
                       world->tile_w, world->tile_h );
      GroundItemSpawn( items, num_items, hp_idx, 18, 6,
                       world->tile_w, world->tile_h );
    }
  }

  /* Hallway encounters */
  SpawnRandomEnemy( enemies, num_enemies,
                    1, 12, world->tile_w, world->tile_h );   /* left corridor */
  SpawnRandomConsumable( items, num_items, 1, 8,
                         world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              5, 23, world->tile_w, world->tile_h );           /* junction corridor */
  SpawnRandomConsumable( items, num_items, 9, 23,
                         world->tile_w, world->tile_h );

  SpawnRandomEnemy( enemies, num_enemies,
                    31, 22, world->tile_w, world->tile_h );  /* east corridor */
  SpawnRandomConsumable( items, num_items, 29, 26,
                         world->tile_w, world->tile_h );

  /* Shop in Room 9 (SE room) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "dungeonbargains" ),
            22, 28, world->tile_w, world->tile_h );
  ShopSpawn( world );

  /* Side room off white corridor */
  SpawnRandomEnemy( enemies, num_enemies,
                    9, 42, world->tile_w, world->tile_h );
  {
    int bag_idx = ConsumableByKey( "bag" );
    if ( bag_idx >= 0 )
      GroundItemSpawn( items, num_items, bag_idx, 10, 41,
                       world->tile_w, world->tile_h );
  }
  SpawnRandomConsumable( items, num_items, 10, 42,
                         world->tile_w, world->tile_h );

  /* Class elite - guards elite room, drops class weapon */
  SpawnClassElite( enemies, num_enemies,
                   4, 45, world->tile_w, world->tile_h );

  /* Fallen adventurer's journal in elite room */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "journal" ),
            3, 44, world->tile_w, world->tile_h );

  /* Bag near upper passage */
  {
    int bag_idx = ConsumableByKey( "bag" );
    if ( bag_idx >= 0 )
      GroundItemSpawn( items, num_items, bag_idx, 4, 3,
                       world->tile_w, world->tile_h );
  }

  /* Upper Passage (\ room) */
  SpawnRandomEnemy( enemies, num_enemies,
                    14, 3, world->tile_w, world->tile_h );
  SpawnRandomEnemy( enemies, num_enemies,
                    16, 2, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 15, 2,
                         world->tile_w, world->tile_h );

  /* Lower Passage (| room, cols 14-19, rows 36-37) */
  SpawnRandomEnemy( enemies, num_enemies,
                    15, 36, world->tile_w, world->tile_h );
  SpawnRandomEnemy( enemies, num_enemies,
                    17, 37, world->tile_w, world->tile_h );
  SpawnRandomConsumable( items, num_items, 16, 36,
                         world->tile_w, world->tile_h );

  /* Hidden wall treasures: 1 golden ring + 2 rare consumables */
  {
    int spots[3][2] = { {5, 29}, {33, 31}, {18, 39} };
    int ring_spot = rand() % 3;
    int ring_idx  = EquipmentByKey( "golden_ring" );
    int tw = world->tile_w, th = world->tile_h;

    for ( int i = 0; i < 3; i++ )
    {
      if ( i == ring_spot && ring_idx >= 0 )
        GroundItemSpawnEquipment( items, num_items, ring_idx,
                                  spots[i][0], spots[i][1], tw, th );
      else
        SpawnRandomT2Consumable( items, num_items,
                                 spots[i][0], spots[i][1], tw, th );
    }
  }

  /* Sacks — class-gated chambers, only 1 reachable per class */
  {
    int sack_idx = ConsumableByKey( "sack" );
    int tw = world->tile_w, th = world->tile_h;
    if ( sack_idx >= 0 )
    {
      GroundItemSpawn( items, num_items, sack_idx, 14, 45, tw, th );
      GroundItemSpawn( items, num_items, sack_idx, 24, 45, tw, th );
      GroundItemSpawn( items, num_items, sack_idx, 30, 45, tw, th );
    }
  }

  /* Stairway in exit chamber (past rat hole room) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "stairway" ),
            10, 1, world->tile_w, world->tile_h );

  /* Old crates - scattered across corridors and side areas */
  ITilePlace( world, 31, 9, ITILE_OLD_CRATE );    /* upper corridor junction */
  ITilePlace( world, 8, 7, ITILE_OLD_CRATE );     /* horizontal corridor */
  ITilePlace( world, 4, 13, ITILE_OLD_CRATE );    /* west corridor north */
  ITilePlace( world, 1, 22, ITILE_OLD_CRATE );    /* west corridor south */
  ITilePlace( world, 6, 31, ITILE_OLD_CRATE );    /* long east-west corridor */
  ITilePlace( world, 22, 16, ITILE_OLD_CRATE );    /* long east-west corridor */
  ITilePlace( world, 24, 32, ITILE_OLD_CRATE );   /* south corridor west */
  ITilePlace( world, 20, 32, ITILE_OLD_CRATE );   /* south corridor east */
  ITileCrateScatterGold();
}
