#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>

#include "dungeon.h"
#include "player.h"
#include "items.h"
#include "movement.h"
#include "shop.h"

extern Player_t player;

/* Spawn a random class-appropriate consumable at (row, col).
   First 3 consumables in the DUF are tier 1, next 3 are tier 2. */
static int get_class_idx( void )
{
  for ( int i = 0; i < 3; i++ )
    if ( strcmp( player.name, g_classes[i].name ) == 0 ) return i;
  return 0;
}

static void spawn_random_consumable( GroundItem_t* items, int* num_items,
                                     int row, int col, int tw, int th )
{
  FilteredItem_t f[16];
  int nf = ItemsBuildFiltered( get_class_idx(), f, 16, 0 );
  int cons[3], nc = 0;
  for ( int i = 0; i < nf && nc < 3; i++ )
  {
    if ( f[i].type == FILTERED_CONSUMABLE )
      cons[nc++] = f[i].index;
  }
  if ( nc > 0 )
    GroundItemSpawn( items, num_items, cons[rand() % nc], row, col, tw, th );
}

static void spawn_random_t2_consumable( GroundItem_t* items, int* num_items,
                                        int row, int col, int tw, int th )
{
  FilteredItem_t f[16];
  int nf = ItemsBuildFiltered( get_class_idx(), f, 16, 0 );
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

/* Spawn a random floor-1 enemy (rat, skeleton, or slime) at (x, y) */
static void spawn_random_enemy( Enemy_t* enemies, int* num_enemies,
                                int x, int y, int tw, int th )
{
  static const char* types[] = { "rat", "skeleton", "slime" };
  EnemySpawn( enemies, num_enemies,
              EnemyTypeByKey( types[rand() % 3] ), x, y, tw, th );
}

/* Spawn the class-matching elite enemy at (x, y) */
static void spawn_class_elite( Enemy_t* enemies, int* num_enemies,
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




/* Spawn a random tier-2 class-appropriate consumable at (x, y) */
static void spawn_t2_consumable( GroundItem_t* items, int* num_items,
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

/* ====== Floor 2 spawner ====== */
static void spawn_floor_2( NPC_t* npcs, int* num_npcs,
                            Enemy_t* enemies, int* num_enemies,
                            GroundItem_t* items, int* num_items,
                            World_t* world )
{
  int tw = world->tile_w, th = world->tile_h;
  (void)0;

  /* Room 0 (entry room, x=47..53, y=4..8):
     4 corner-not-corner positions, shuffled among:
     Laura (fighting NPC), Glorbnax (NPC), rat, goblin grunt */
  {
    int corners[4][2] = { {48,5}, {52,5}, {48,7}, {52,7} };

    /* Fisher-Yates shuffle */
    for ( int i = 3; i > 0; i-- )
    {
      int j = rand() % ( i + 1 );
      int tr = corners[i][0], tc = corners[i][1];
      corners[i][0] = corners[j][0]; corners[i][1] = corners[j][1];
      corners[j][0] = tr;            corners[j][1] = tc;
    }

    NPCSpawn( npcs, num_npcs, NPCTypeByKey( "laura" ),
              corners[0][0], corners[0][1], tw, th );
    NPCSpawn( npcs, num_npcs, NPCTypeByKey( "glorbnax_the_serene" ),
              corners[1][0], corners[1][1], tw, th );
    EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
                corners[2][0], corners[2][1], tw, th );
    EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "goblin_grunt" ),
                corners[3][0], corners[3][1], tw, th );
  }

  /* Cave spider in the %%%% room, just before Burble */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "spider" ),
              28, 6, tw, th );

  /* Burble - gate guard, beside the W door into the peace tribe town */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "burble_the_observant" ),
            20, 6, tw, th );

  /* Grishnak - peace goblin in town square, just south of the shop */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "grishnak_the_unyielding" ),
            13, 5, tw, th );

  /* Nettle - tea shop / reluctant shopkeeper in ) room */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "nettle_the_brewer" ),
            10, 3, tw, th );
  ShopSpawn( world );

  /* Thistlewick - meditating in the ( room */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "thistlewick_the_still" ),
            4, 6, tw, th );

  /* Clink - goblin banker at the end of the * room */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "clink_the_counter" ),
            17, 10, tw, th );

  /* Red slimes - scattered across the bottom half + the --- room */
  int rs = EnemyTypeByKey( "red_slime" );

  /* --- room (top, cols 20-24) */
  EnemySpawn( enemies, num_enemies, rs, 22, 1, tw, th );

  /* Bottom rooms */
  EnemySpawn( enemies, num_enemies, rs, 24, 25, tw, th );  /* room 6 */
  EnemySpawn( enemies, num_enemies, rs, 54, 25, tw, th );  /* room ~ */
  EnemySpawn( enemies, num_enemies, rs, 76, 19, tw, th );  /* room 9 */

  /* Bottom corridors */
  EnemySpawn( enemies, num_enemies, rs, 28, 15, tw, th );  /* left vertical */
  EnemySpawn( enemies, num_enemies, rs, 42, 21, tw, th );  /* center vertical */
  EnemySpawn( enemies, num_enemies, rs, 58, 17, tw, th );  /* right vertical */

  /* Spiders - in barren bottom rooms */
  int sp = EnemyTypeByKey( "spider" );
  EnemySpawn( enemies, num_enemies, sp, 24, 20, tw, th );  /* room 5 */
  EnemySpawn( enemies, num_enemies, sp, 36, 20, tw, th );  /* room 7 */
  EnemySpawn( enemies, num_enemies, sp, 62, 20, tw, th );  /* room 8 */
  EnemySpawn( enemies, num_enemies, sp, 44, 26, tw, th );  /* room @ */
  EnemySpawn( enemies, num_enemies, sp, 74, 26, tw, th );  /* room ! */

  /* Tier 1 consumables - help player get started in floor02*/
  spawn_random_consumable( items, num_items, 50, 4, tw, th );  /* \ room 0 */
  spawn_random_consumable( items, num_items, 46, 14, tw, th );  /* \ south coor*/
  spawn_random_consumable( items, num_items, 56, 14, tw, th );  /* \ south coor*/
  spawn_random_consumable( items, num_items, 30, 6, tw, th );  /* \ spider room */



  /* Tier 2 consumables - scattered in bottom rooms and corridors */
  spawn_t2_consumable( items, num_items, 23, 19, tw, th );  /* room 5 */
  spawn_t2_consumable( items, num_items, 37, 19, tw, th );  /* room 7 */
  spawn_t2_consumable( items, num_items, 62, 19, tw, th );  /* room 8 */
  spawn_t2_consumable( items, num_items, 46, 25, tw, th );  /* room @ */
  spawn_t2_consumable( items, num_items, 53, 25, tw, th );  /* room ~ */
  spawn_t2_consumable( items, num_items, 76, 25, tw, th );  /* room ! */
  spawn_t2_consumable( items, num_items, 35, 22, tw, th );  /* bottom corridor left */
  spawn_t2_consumable( items, num_items, 65, 22, tw, th );  /* bottom corridor right */

  /* Rare tier 2 consumables - 1 universal, 3 class-gated */
  {
    const char* cls = PlayerClassKey();
    const char* rare;
    if ( strcmp( cls, "mercenary" ) == 0 )      rare = "dragon_pepper";
    else if ( strcmp( cls, "rogue" ) == 0 )     rare = "smoke_bomb";
    else                                         rare = "scroll_teleport";

    int idx;
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 28, 17, tw, th ); /* B path */
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 42, 17, tw, th ); /* G path */
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 56, 17, tw, th ); /* W path */
    idx = ConsumableByKey( rare );
    if ( idx >= 0 ) GroundItemSpawn( items, num_items, idx, 70, 17, tw, th ); /* R path */
  }

  /* Health potions - hidden in bottom corridors (reachable by all classes) */
  {
    int hp = ConsumableByKey( "small_health_potion" );
    if ( hp >= 0 )
    {
      GroundItemSpawn( items, num_items, hp, 28, 20, tw, th );  /* left corridor */
      GroundItemSpawn( items, num_items, hp, 50, 22, tw, th );  /* center corridor */
      GroundItemSpawn( items, num_items, hp, 72, 20, tw, th );  /* right corridor */
    }
  }

  /* === War Goblin Territory (top-right) === */
  {
    int gr = EnemyTypeByKey( "goblin_grunt" );
    int sh = EnemyTypeByKey( "goblin_shaman" );
    int sl = EnemyTypeByKey( "goblin_slinger" );
    int kr = EnemyTypeByKey( "kragthar" );

    /* \ room (Goblin Barracks) - grunt + slinger */
    EnemySpawn( enemies, num_enemies, gr, 63, 2, tw, th );
    EnemySpawn( enemies, num_enemies, sl, 64, 3, tw, th );

    /* ; room (Goblin Den) - shaman + grunt + slinger */
    EnemySpawn( enemies, num_enemies, sh, 80, 2, tw, th );
    EnemySpawn( enemies, num_enemies, gr, 79, 3, tw, th );
    EnemySpawn( enemies, num_enemies, sl, 81, 1, tw, th );

    /* : room (Goblin Warren) - slinger + grunt */
    EnemySpawn( enemies, num_enemies, sl, 91, 3, tw, th );
    EnemySpawn( enemies, num_enemies, gr, 90, 2, tw, th );

    /* Corridor leading to boss */
    EnemySpawn( enemies, num_enemies, gr, 82, 7, tw, th );

    /* q room (Goblin Boss) - Kragthar + shaman + grunt + slinger */
    EnemySpawn( enemies, num_enemies, kr, 90, 11, tw, th );
    EnemySpawn( enemies, num_enemies, sh, 91, 10, tw, th );
    EnemySpawn( enemies, num_enemies, gr, 89, 12, tw, th );
    EnemySpawn( enemies, num_enemies, sl, 92, 12, tw, th );

    /* Consumable loot in each goblin room */
    spawn_random_consumable( items, num_items, 62, 1, tw, th );  /* \ Barracks */
    spawn_random_consumable( items, num_items, 75, 2, tw, th );  /* | Tunnel */
    spawn_random_consumable( items, num_items, 78, 1, tw, th );  /* ; Den */
    spawn_random_consumable( items, num_items, 92, 2, tw, th );  /* : Warren */

    /* Boss room: 1 regular + 1 rare class consumable */
    spawn_random_consumable( items, num_items, 90, 9, tw, th );  /* q Boss */
    spawn_t2_consumable( items, num_items, 93, 11, tw, th );     /* q Boss (rare) */
  }

  /* Middle-band rooms (south): 1 goblin grunt + 1 random consumable each */
  {
    int gr = EnemyTypeByKey( "goblin_grunt" );
    EnemySpawn( enemies, num_enemies, gr, 22, 10, tw, th );  /* room 2 */
    EnemySpawn( enemies, num_enemies, gr, 34, 10, tw, th );  /* room 1 */
    EnemySpawn( enemies, num_enemies, gr, 64, 10, tw, th );  /* room 3 */
    EnemySpawn( enemies, num_enemies, gr, 76, 10, tw, th );  /* room 4 */

    spawn_random_t2_consumable( items, num_items, 21, 9, tw, th );  /* room 2 */
    spawn_random_t2_consumable( items, num_items, 35, 9, tw, th );  /* room 1 */
    spawn_random_t2_consumable( items, num_items, 63, 9, tw, th );  /* room 3 */
    spawn_random_t2_consumable( items, num_items, 77, 9, tw, th );  /* room 4 */
  }

  /* Cave mushrooms for Nettle's quest - one per W-accessible path */
  {
    int cm = ConsumableByKey( "cave_mushroom" );
    if ( cm >= 0 )
    {
      GroundItemSpawn( items, num_items, cm, 28, 24, tw, th );  /* left corridor */
      GroundItemSpawn( items, num_items, cm, 50, 27, tw, th );  /* center corridor */
      GroundItemSpawn( items, num_items, cm, 72, 24, tw, th );  /* right corridor */
    }
  }
}

void DungeonSpawn( NPC_t* npcs, int* num_npcs,
                   Enemy_t* enemies, int* num_enemies,
                   GroundItem_t* items, int* num_items,
                   World_t* world )
{
  extern int g_current_floor;
  if ( g_current_floor == 2 )
  {
    spawn_floor_2( npcs, num_npcs, enemies, num_enemies,
                   items, num_items, world );
    return;
  }

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
  spawn_random_consumable( items, num_items, 8, 27,
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
  spawn_random_consumable( items, num_items, 3, 31,
                           world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              2, 12, world->tile_w, world->tile_h );    /* room 4 (merc) */
  spawn_random_consumable( items, num_items, 3, 11,
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
  spawn_random_consumable( items, num_items, 12, 45,
                           world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              23, 45, world->tile_w, world->tile_h );   /* blue chamber */
  spawn_random_consumable( items, num_items, 22, 46,
                           world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              32, 45, world->tile_w, world->tile_h );   /* green chamber */
  spawn_random_consumable( items, num_items, 31, 46,
                           world->tile_w, world->tile_h );

  spawn_random_consumable( items, num_items, 5, 45,
                           world->tile_w, world->tile_h ); /* white chamber reward */

  /* Rogue Annex */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              29, 13, world->tile_w, world->tile_h );
  spawn_random_consumable( items, num_items, 30, 12,
                           world->tile_w, world->tile_h );

  /* Merc Annex */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "rat" ),
              30, 18, world->tile_w, world->tile_h );
  spawn_random_consumable( items, num_items, 31, 17,
                           world->tile_w, world->tile_h );

  /* Merc Upper Barracks */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "skeleton" ),
              30, 3, world->tile_w, world->tile_h );
  spawn_random_consumable( items, num_items, 31, 2,
                           world->tile_w, world->tile_h );

  /* Merc Lower Barracks */
  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              30, 8, world->tile_w, world->tile_h );
  spawn_random_consumable( items, num_items, 31, 7,
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
    }
  }

  /* Hallway encounters */
  spawn_random_enemy( enemies, num_enemies,
                      1, 12, world->tile_w, world->tile_h );   /* left corridor */
  spawn_random_consumable( items, num_items, 1, 8,
                           world->tile_w, world->tile_h );

  EnemySpawn( enemies, num_enemies, EnemyTypeByKey( "slime" ),
              5, 23, world->tile_w, world->tile_h );           /* junction corridor */
  spawn_random_consumable( items, num_items, 9, 23,
                           world->tile_w, world->tile_h );

  spawn_random_enemy( enemies, num_enemies,
                      31, 22, world->tile_w, world->tile_h );  /* east corridor */
  spawn_random_consumable( items, num_items, 29, 26,
                           world->tile_w, world->tile_h );

  /* Shop in Room 9 (SE room) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "dungeonbargains" ),
            22, 28, world->tile_w, world->tile_h );
  ShopSpawn( world );

  /* Side room off white corridor */
  spawn_random_enemy( enemies, num_enemies,
                      9, 42, world->tile_w, world->tile_h );

  /* Class elite - guards elite room, drops class weapon */
  spawn_class_elite( enemies, num_enemies,
                     4, 45, world->tile_w, world->tile_h );

  /* Fallen adventurer's journal in elite room */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "journal" ),
            3, 44, world->tile_w, world->tile_h );

  /* Upper Passage (\ room) */
  spawn_random_enemy( enemies, num_enemies,
                      14, 3, world->tile_w, world->tile_h );
  spawn_random_enemy( enemies, num_enemies,
                      16, 2, world->tile_w, world->tile_h );
  spawn_random_consumable( items, num_items, 15, 2,
                           world->tile_w, world->tile_h );

  /* Lower Passage (| room, cols 14-19, rows 36-37) */
  spawn_random_enemy( enemies, num_enemies,
                      15, 36, world->tile_w, world->tile_h );
  spawn_random_enemy( enemies, num_enemies,
                      17, 37, world->tile_w, world->tile_h );
  spawn_random_consumable( items, num_items, 16, 36,
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
        spawn_random_t2_consumable( items, num_items,
                                    spots[i][0], spots[i][1], tw, th );
    }
  }

  /* Stairway in exit chamber (past rat hole room) */
  NPCSpawn( npcs, num_npcs, NPCTypeByKey( "stairway" ),
            10, 1, world->tile_w, world->tile_h );
}
