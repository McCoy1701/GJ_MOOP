#include <string.h>
#include <Archimedes.h>

#include <stdlib.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "maps.h"
#include "console.h"
#include "game_events.h"
#include "combat.h"
#include "combat_vfx.h"
#include "movement.h"
#include "visibility.h"
#include "enemies.h"
#include "placed_traps.h"
#include "spell_vfx.h"
#include "interactive_tile.h"
#include "room_enumerator.h"
#include "game_turns.h"

extern Player_t player;

static Console_t* con;
static int consumable_used = 0;

static World_t*  ge_world       = NULL;
static Enemy_t*  ge_enemies     = NULL;
static int*      ge_enemy_count = NULL;

static aSoundEffect_t sfx_spark;
static aSoundEffect_t sfx_frost;
static aSoundEffect_t sfx_fireball;
static aSoundEffect_t sfx_arrow;
static aSoundEffect_t sfx_merc_swing;

void GameEventsInit( Console_t* c )
{
  con = c;
  a_AudioLoadSound( "resources/soundeffects/spark.wav",    &sfx_spark );
  a_AudioLoadSound( "resources/soundeffects/frost.wav",    &sfx_frost );
  a_AudioLoadSound( "resources/soundeffects/fireball.wav", &sfx_fireball );
  a_AudioLoadSound( "resources/soundeffects/arrow_shoot.wav", &sfx_arrow );
  a_AudioLoadSound( "resources/soundeffects/merc_swing.wav",  &sfx_merc_swing );
}

void GameEventsSetWorld( World_t* world, Enemy_t* enemies, int* enemy_count )
{
  ge_world       = world;
  ge_enemies     = enemies;
  ge_enemy_count = enemy_count;
}

void GameEventsNewTurn( void )        { consumable_used = 0; }
int  GameEventsConsumableUsed( void ) { return consumable_used; }

Console_t* GameEventsGetConsole( void ) { return con; }

static void evt_LookEquipment( int index )
{
  EquipmentInfo_t* e = &g_equipment[index];

  ConsolePushF( con, e->color, "You looked at %s", e->name );
  if ( e->damage > 0 )
    ConsolePushF( con, (aColor_t){ 0xeb, 0xed, 0xe9, 255 }, "  DMG: +%d", e->damage );
  if ( e->defense > 0 )
    ConsolePushF( con, (aColor_t){ 0xeb, 0xed, 0xe9, 255 }, "  DEF: +%d", e->defense );
  if ( strcmp( e->effect, "none" ) != 0 )
    ConsolePushF( con, (aColor_t){ 0xde, 0x9e, 0x41, 255 }, "  %s (%d)", e->effect, e->effect_value );
  ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 }, "  %s", e->description );
}

static void evt_LookConsumable( int index )
{
  ConsumableInfo_t* c = &g_consumables[index];

  ConsolePushF( con, c->color, "You looked at %s", c->name );
  if ( c->bonus_damage > 0 )
    ConsolePushF( con, (aColor_t){ 0xeb, 0xed, 0xe9, 255 }, "  DMG: +%d", c->bonus_damage );
  if ( strcmp( c->effect, "none" ) != 0 && strlen( c->effect ) > 0 )
    ConsolePushF( con, (aColor_t){ 0xde, 0x9e, 0x41, 255 }, "  %s", c->effect );
  ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 }, "  %s", c->description );
}

static void evt_LookMap( int index )
{
  MapInfo_t* m = &g_maps[index];

  ConsolePushF( con, m->color, "You looked at %s", m->name );
  ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 }, "  %s", m->description );
}

static void evt_Equip( int index )
{
  EquipmentInfo_t* e = &g_equipment[index];
  ConsolePushF( con, e->color, "You equip %s", e->name );
  PlayerRecalcStats();
}

static void evt_Unequip( int index )
{
  EquipmentInfo_t* e = &g_equipment[index];
  ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 }, "%s unequipped %s",
                player.name, e->name );
  PlayerRecalcStats();
}

static void evt_SwapEquip( int new_idx, int old_idx )
{
  EquipmentInfo_t* new_e = &g_equipment[new_idx];
  EquipmentInfo_t* old_e = &g_equipment[old_idx];
  ConsolePushF( con, new_e->color, "%s swapped %s for %s",
                player.name, old_e->name, new_e->name );
  PlayerRecalcStats();
}

static void evt_UseConsumable( int index )
{
  ConsumableInfo_t* c = &g_consumables[index];
  ConsolePushF( con, c->color, "You use %s", c->name );
}

int GameEventUseConsumable( int consumable_index )
{
  if ( consumable_index < 0 || consumable_index >= g_num_consumables )
    return 0;

  ConsumableInfo_t* c = &g_consumables[consumable_index];

  /* One consumable per turn (quest items bypass) */
  if ( consumable_used && strcmp( c->type, "quest" ) != 0 )
  {
    ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                  "You can only use one consumable per turn." );
    return 0;
  }

  /* Potions - instant use, no attack required */
  if ( strcmp( c->type, "potion" ) == 0 )
  {
    /* Empower - buff next attack to deal double damage */
    if ( strcmp( c->effect, "empower" ) == 0 )
    {
      PlayerApplyBuff( 0, "empower", 0 );
      ConsolePushF( con, c->color,
                    "You drink %s. Next attack deals double damage!",
                    c->name );
      consumable_used = 1;
      return 1;
    }

    /* Standard heal potion */
    int healed = c->heal;
    if ( player.hp + healed > player.max_hp )
      healed = player.max_hp - player.hp;

    if ( healed <= 0 )
    {
      ConsolePushF( con, c->color, "You are already at full health." );
      return 0;
    }

    PlayerHeal( healed );
    CombatVFXSpawnNumber( player.world_x, player.world_y, healed,
                          (aColor_t){ 0x75, 0xa7, 0x43, 255 } );
    ConsolePushF( con, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                  "You drink %s. Restored %d HP.", c->name, healed );

    consumable_used = 1;
    return 1;
  }

  /* Food - buff next attack */
  if ( strcmp( c->type, "food" ) == 0 )
  {
    PlayerApplyBuff( c->bonus_damage, c->effect, c->heal );

    if ( strcmp( c->effect, "none" ) != 0 && strlen( c->effect ) > 0 )
      ConsolePushF( con, c->color, "You eat %s. Next attack: %s (+%d dmg).",
                    c->name, c->effect, c->bonus_damage );
    else
      ConsolePushF( con, c->color, "You eat %s. Next attack: +%d dmg.",
                    c->name, c->bonus_damage );

    consumable_used = 1;
    return 1;
  }

  /* Quest items - generic handler with optional heal/message/give */
  if ( strcmp( c->type, "quest" ) == 0 )
  {
    /* Lure - requires specific location, handled in Phase 4 */
    if ( strcmp( c->effect, "lure" ) == 0 )
    {
      int pr, pc;
      PlayerGetTile( &pr, &pc );
      if ( RoomAt( pr, pc ) != ROOM_RAT_HOLE )
      {
        ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                      "The smell wafts away. Nothing to lure here." );
        return 0;
      }

      ITileBreak( ge_world, 5, 2 );

      int rat_king_idx = EnemyTypeByKey( "rat_king" );
      int rat_idx      = EnemyTypeByKey( "rat" );
      if ( rat_king_idx >= 0 )
        EnemySpawn( ge_enemies, ge_enemy_count, rat_king_idx, 5, 2,
                    ge_world->tile_w, ge_world->tile_h );
      if ( rat_idx >= 0 )
      {
        /* Candidate spots inside Room 24 - skip the player's tile */
        static const int rat_pos[][2] = { {2,1}, {4,1}, {1,2}, {3,2} };
        int spawned = 0;
        for ( int i = 0; i < 4 && spawned < 2; i++ )
        {
          if ( rat_pos[i][0] == pr && rat_pos[i][1] == pc ) continue;
          EnemySpawn( ge_enemies, ge_enemy_count, rat_idx,
                      rat_pos[i][0], rat_pos[i][1],
                      ge_world->tile_w, ge_world->tile_h );
          spawned++;
        }
      }

      ConsolePushF( con, (aColor_t){ 0xde, 0x9e, 0x41, 255 },
                    "You hold out the stinky cheese..." );
      ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                    "The wall EXPLODES! The Rat King emerges!" );

      return 1;
    }

    /* Sandwich - set flag, then fall through to generic handler */
    if ( strcmp( c->effect, "eat_sandwich" ) == 0 )
      FlagSet( "sandwich_eaten", 1 );

    /* Generic quest item: heal + message + give item */
    if ( c->heal > 0 )
    {
      PlayerHeal( c->heal );
      CombatVFXSpawnNumber( player.world_x, player.world_y, c->heal,
                            (aColor_t){ 0x75, 0xa7, 0x43, 255 } );
    }
    if ( c->use_message[0] )
      ConsolePushF( con, c->color, "%s", c->use_message );
    if ( c->gives[0] )
    {
      int gi = ConsumableByKey( c->gives );
      if ( gi >= 0 )
        InventoryAdd( INV_CONSUMABLE, gi );
    }
    return 1;
  }

  ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                "%s can't use %s yet.", player.name, c->name );
  return 0;
}

int GameEventUseMap( int map_index )
{
  if ( map_index < 0 || map_index >= g_num_maps )
    return 0;

  MapInfo_t* m = &g_maps[map_index];

  ITile_t* it = ITileAt( m->target_x, m->target_y );
  if ( !it || it->type != ITILE_HIDDEN_WALL )
  {
    ConsolePushF( con, m->color,
                  "You study the %s... but it makes no sense.", m->name );
    return 0;
  }

  if ( !it->revealed )
  {
    ITileReveal( ge_world, m->target_x, m->target_y );
    ConsolePushF( con, (aColor_t){ 0xde, 0x9e, 0x41, 255 },
                  "A secret passage! The wall looks different now." );
  }

  int pr, pc;
  PlayerGetTile( &pr, &pc );

  ConsolePushF( con, m->color,
                "You study the %s...", m->name );
  ConsolePushF( con, (aColor_t){ 0xde, 0x9e, 0x41, 255 },
                "You are at (%d, %d). X marks (%d, %d)!",
                pr, pc, m->target_x, m->target_y );

  return 0;  /* map stays in inventory */
}

void GameEvent( GameEventType_t type, int index )
{
  switch ( type )
  {
    case EVT_LOOK_EQUIPMENT:  evt_LookEquipment( index );  break;
    case EVT_LOOK_CONSUMABLE: evt_LookConsumable( index );  break;
    case EVT_LOOK_MAP:        evt_LookMap( index );         break;
    case EVT_EQUIP:           evt_Equip( index );           break;
    case EVT_UNEQUIP:         evt_Unequip( index );         break;
    case EVT_USE_CONSUMABLE:  evt_UseConsumable( index );   break;
    default: break;
  }
}

void GameEventSwap( int new_idx, int old_idx )
{
  evt_SwapEquip( new_idx, old_idx );
}

/* ---- Targeted consumable effect resolution ---- */

/* Apply totem defense aura to consumable damage */
static int apply_totem_def( Enemy_t* e, int damage )
{
  int def = CombatTotemDefenseFor( e );
  int fd = damage - def;
  return ( fd < 1 ) ? 1 : fd;
}

static void consume_scroll( int inv_slot )
{
  int echo = PlayerEquipEffectMin( "scroll_echo" );
  if ( echo > 0 )
  {
    player.scroll_echo_counter++;
    if ( player.scroll_echo_counter >= echo )
    {
      player.scroll_echo_counter = 0;
      ConsolePushF( con, (aColor_t){ 0x73, 0xbe, 0xd3, 255 },
                    "Scroll Echo! Free cast!" );
      return;
    }
  }
  InventoryRemove( inv_slot );
}

int GameEventResolveTarget( int consumable_idx, int inv_slot,
                            int target_row, int target_col,
                            Enemy_t* enemies, int num_enemies )
{
  if ( consumable_idx < 0 || consumable_idx >= g_num_consumables )
    return 0;

  ConsumableInfo_t* c = &g_consumables[consumable_idx];

  /* One consumable per turn */
  if ( consumable_used )
  {
    ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                  "You can only use one consumable per turn." );
    return 0;
  }

  int pr, pc;
  PlayerGetTile( &pr, &pc );
  int dmg = PlayerStat( "damage" ) + c->bonus_damage;
  dmg += PlayerEquipEffect( "amplify" );
  if ( dmg < 1 ) dmg = 1;

  aColor_t hit_color = c->color;

  /* ---- SHOOT: cardinal line projectile ---- */
  if ( strcmp( c->effect, "shoot" ) == 0 )
  {
    a_AudioPlaySound( &sfx_arrow, NULL );
    int dr = ( target_row > pr ) ? 1 : ( target_row < pr ) ? -1 : 0;
    int dc = ( target_col > pc ) ? 1 : ( target_col < pc ) ? -1 : 0;

    Enemy_t* hit = NULL;
    int cr = pr, cc = pc;
    for ( int step = 0; step < c->range; step++ )
    {
      cr += dr;
      cc += dc;
      if ( !TileWalkable( cr, cc ) ) break;
      hit = EnemyAt( enemies, num_enemies, cr, cc );
      if ( hit ) break;
    }

    /* Spawn arrow VFX from player toward end of line */
    float tw = 16.0f, th = 16.0f;
    float sx = pr * tw + tw / 2.0f;
    float sy = pc * th + th / 2.0f;
    float ex = cr * tw + tw / 2.0f;
    float ey = cc * th + th / 2.0f;
    EnemyProjectileSpawn( sx, sy, ex, ey, dr, dc );

    if ( hit )
    {
      EnemyType_t* t = &g_enemy_types[hit->type_idx];
      int fd = apply_totem_def( hit, dmg );
      hit->hp -= fd;
      hit->turns_since_hit = 0;
      CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
      ConsolePushF( con, hit_color, "You shoot %s for %d damage!",
                    t->name, fd );
      if ( hit->hp <= 0 )
        CombatHandleEnemyDeath( hit );
    }
    else
    {
      ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                    "The arrow flies into the darkness..." );
    }

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- PIERCE: cardinal line projectile that hits ALL enemies ---- */
  if ( strcmp( c->effect, "pierce" ) == 0 )
  {
    a_AudioPlaySound( &sfx_arrow, NULL );
    int dr = ( target_row > pr ) ? 1 : ( target_row < pr ) ? -1 : 0;
    int dc = ( target_col > pc ) ? 1 : ( target_col < pc ) ? -1 : 0;

    int cr = pr, cc = pc;
    int end_r = pr, end_c = pc;
    int hits = 0;

    for ( int step = 0; step < c->range; step++ )
    {
      cr += dr;
      cc += dc;
      if ( !TileWalkable( cr, cc ) ) break;
      end_r = cr; end_c = cc;

      Enemy_t* hit = EnemyAt( enemies, num_enemies, cr, cc );
      if ( hit )
      {
        EnemyType_t* t = &g_enemy_types[hit->type_idx];
        int fd = apply_totem_def( hit, dmg );
        hit->hp -= fd;
        hit->turns_since_hit = 0;
        CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
        ConsolePushF( con, hit_color, "%s pierces %s for %d damage!",
                      player.name, t->name, fd );
        if ( hit->hp <= 0 )
          CombatHandleEnemyDeath( hit );
        hits++;
      }
    }

    /* Spawn arrow VFX from player to end of line */
    float tw = 16.0f, th = 16.0f;
    float sx = pr * tw + tw / 2.0f;
    float sy = pc * th + th / 2.0f;
    float ex = end_r * tw + tw / 2.0f;
    float ey = end_c * th + th / 2.0f;
    EnemyProjectileSpawn( sx, sy, ex, ey, dr, dc );

    if ( hits == 0 )
      ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                    "The arrow flies into the darkness..." );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;
  }

  /* ---- STEW_THROW: heal + cardinal line projectile ---- */
  if ( strcmp( c->effect, "stew_throw" ) == 0 )
  {
    a_AudioPlaySound( &sfx_merc_swing, NULL );
    /* Heal first */
    if ( c->heal > 0 )
    {
      int healed = c->heal;
      if ( player.hp + healed > player.max_hp )
        healed = player.max_hp - player.hp;

      if ( healed > 0 )
      {
        PlayerHeal( healed );
        CombatVFXSpawnNumber( player.world_x, player.world_y, healed,
                              (aColor_t){ 0x75, 0xa7, 0x43, 255 } );
        ConsolePushF( con, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                      "You eat %s. Restored %d HP.", c->name, healed );
      }
    }

    /* Throw the bowl */
    int dr = ( target_row > pr ) ? 1 : ( target_row < pr ) ? -1 : 0;
    int dc = ( target_col > pc ) ? 1 : ( target_col < pc ) ? -1 : 0;

    Enemy_t* hit = NULL;
    int cr = pr, cc = pc;
    for ( int step = 0; step < c->range; step++ )
    {
      cr += dr;
      cc += dc;
      if ( !TileWalkable( cr, cc ) ) break;
      hit = EnemyAt( enemies, num_enemies, cr, cc );
      if ( hit ) break;
    }

    float tw = 16.0f, th = 16.0f;
    EnemyProjectileSpawn( pr * tw + tw / 2.0f, pc * th + th / 2.0f,
                          cr * tw + tw / 2.0f, cc * th + th / 2.0f,
                          dr, dc );

    if ( hit )
    {
      EnemyType_t* t = &g_enemy_types[hit->type_idx];
      int fd = apply_totem_def( hit, dmg );
      hit->hp -= fd;
      hit->turns_since_hit = 0;
      CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
      ConsolePushF( con, hit_color, "You hurl the bowl at %s for %d damage!",
                    t->name, fd );
      if ( hit->hp <= 0 )
        CombatHandleEnemyDeath( hit );
    }
    else
    {
      ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                    "The bowl sails into the darkness..." );
    }

    InventoryRemove( inv_slot );
    consumable_used = 1;
    return 2;  /* food = free action */
  }

  /* ---- POISON: cardinal line + DOT ---- */
  if ( strcmp( c->effect, "poison" ) == 0 )
  {
    a_AudioPlaySound( &sfx_arrow, NULL );
    int dr = ( target_row > pr ) ? 1 : ( target_row < pr ) ? -1 : 0;
    int dc = ( target_col > pc ) ? 1 : ( target_col < pc ) ? -1 : 0;

    Enemy_t* hit = NULL;
    int cr = pr, cc = pc;
    for ( int step = 0; step < c->range; step++ )
    {
      cr += dr;
      cc += dc;
      if ( !TileWalkable( cr, cc ) ) break;
      hit = EnemyAt( enemies, num_enemies, cr, cc );
      if ( hit ) break;
    }

    float tw = 16.0f, th = 16.0f;
    EnemyProjectileSpawn( pr * tw + tw / 2.0f, pc * th + th / 2.0f,
                          cr * tw + tw / 2.0f, cc * th + th / 2.0f,
                          dr, dc );

    if ( hit )
    {
      EnemyType_t* t = &g_enemy_types[hit->type_idx];
      int fd = apply_totem_def( hit, dmg );
      hit->hp -= fd;
      hit->turns_since_hit = 0;
      CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );

      /* Apply poison */
      hit->poison_ticks = c->ticks;
      hit->poison_dmg   = c->tick_damage;

      ConsolePushF( con, hit_color,
                    "%s poisons %s! %d dmg + %d poison for %d turns.",
                    player.name, t->name, fd,
                    c->tick_damage, c->ticks );

      if ( hit->hp <= 0 )
        CombatHandleEnemyDeath( hit );
    }
    else
    {
      ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                    "The arrow flies past..." );
    }

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- TRAP_ROOT: place trap on tile ---- */
  if ( strcmp( c->effect, "trap_root" ) == 0 )
  {
    if ( EnemyAt( enemies, num_enemies, target_row, target_col ) )
    {
      ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                    "Can't place a trap under an enemy." );
      return 0;
    }

    PlacedTrapSpawn( target_row, target_col, dmg, 2 );
    ConsolePushF( con, hit_color, "You set a bear trap." );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action - don't end the turn */
  }

  /* ---- SMOKE: damage + stun enemies in radius ---- */
  if ( strcmp( c->effect, "smoke" ) == 0 )
  {
    int stunned = 0;
    for ( int i = 0; i < num_enemies; i++ )
    {
      if ( !enemies[i].alive ) continue;
      int edr = abs( enemies[i].row - target_row );
      int edc = abs( enemies[i].col - target_col );
      if ( edr + edc <= c->radius )
      {
        int fd = apply_totem_def( &enemies[i], dmg );
        enemies[i].hp -= fd;
        enemies[i].stun_turns = c->duration;
        enemies[i].turns_since_hit = 0;
        CombatVFXSpawnNumber( enemies[i].world_x, enemies[i].world_y, fd, hit_color );
        CombatVFXSpawnText( enemies[i].world_x, enemies[i].world_y,
                            "Stunned!", (aColor_t){ 0x78, 0x78, 0x78, 255 } );
        if ( enemies[i].hp <= 0 )
          CombatHandleEnemyDeath( &enemies[i] );
        stunned++;
      }
    }

    if ( stunned > 0 )
      ConsolePushF( con, hit_color,
                    "You throw a smoke bomb! %d enemies hit and stunned for %d turns.",
                    stunned, c->duration );
    else
      ConsolePushF( con, hit_color,
                    "You throw a smoke bomb! The smoke dissipates harmlessly." );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- MAGIC_BOLT: direct damage, no LOS needed ---- */
  if ( strcmp( c->effect, "magic_bolt" ) == 0 )
  {
    Enemy_t* hit = EnemyAt( enemies, num_enemies, target_row, target_col );
    if ( !hit )
    {
      ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                    "No target there." );
      return 0;
    }

    EnemyType_t* t = &g_enemy_types[hit->type_idx];
    a_AudioPlaySound( &sfx_spark, NULL );
    SpellVFXSpark( player.world_x, player.world_y,
                   hit->world_x, hit->world_y );
    { int fd = apply_totem_def( hit, dmg );
      hit->hp -= fd;
      hit->turns_since_hit = 0;
      CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
      ConsolePushF( con, hit_color,
                    "%s zaps %s with a spark for %d damage!",
                    player.name, t->name, fd );
    }

    if ( hit->hp <= 0 )
      CombatHandleEnemyDeath( hit );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- FREEZE: damage + stun ---- */
  if ( strcmp( c->effect, "freeze" ) == 0 )
  {
    Enemy_t* hit = EnemyAt( enemies, num_enemies, target_row, target_col );
    if ( !hit )
    {
      ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                    "No target there." );
      return 0;
    }

    EnemyType_t* t = &g_enemy_types[hit->type_idx];
    a_AudioPlaySound( &sfx_frost, NULL );
    SpellVFXFrost( player.world_x, player.world_y,
                   hit->world_x, hit->world_y );
    { int fd = apply_totem_def( hit, dmg );
      hit->hp -= fd;
      hit->turns_since_hit = 0;
      hit->stun_turns = c->duration;
      CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
      CombatVFXSpawnText( hit->world_x, hit->world_y - 8,
                          "Frozen!", (aColor_t){ 0x64, 0xb4, 0xff, 255 } );
      ConsolePushF( con, hit_color,
                    "%s freezes %s for %d damage! Frozen for %d turns.",
                    player.name, t->name, fd, c->duration );
    }

    if ( hit->hp <= 0 )
      CombatHandleEnemyDeath( hit );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- AOE: damage target + all enemies in aoe_radius ---- */
  if ( strcmp( c->effect, "aoe" ) == 0 )
  {
    a_AudioPlaySound( &sfx_fireball, NULL );
    SpellVFXFireball( player.world_x, player.world_y,
                      target_row, target_col, c->aoe_radius );
    int hits = 0;
    for ( int i = 0; i < num_enemies; i++ )
    {
      if ( !enemies[i].alive ) continue;
      int dr = abs( enemies[i].row - target_row );
      int dc = abs( enemies[i].col - target_col );
      int dist = ( dr > dc ) ? dr : dc;
      if ( dist <= c->aoe_radius )
      {
        int fd = apply_totem_def( &enemies[i], dmg );
        enemies[i].hp -= fd;
        enemies[i].turns_since_hit = 0;
        CombatVFXSpawnNumber( enemies[i].world_x, enemies[i].world_y,
                              fd, hit_color );
        hits++;

        if ( enemies[i].hp <= 0 )
          CombatHandleEnemyDeath( &enemies[i] );
      }
    }

    if ( hits > 0 )
      ConsolePushF( con, hit_color,
                    "You hurl a fireball! %d enemies hit for %d damage!",
                    hits, dmg );
    else
      ConsolePushF( con, hit_color,
                    "You hurl a fireball! It explodes harmlessly." );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- SWAP: swap positions with target enemy ---- */
  if ( strcmp( c->effect, "swap" ) == 0 )
  {
    Enemy_t* hit = EnemyAt( enemies, num_enemies, target_row, target_col );
    if ( !hit )
    {
      ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                    "No target there." );
      return 0;
    }

    EnemyType_t* t = &g_enemy_types[hit->type_idx];

    /* VFX before swap so we capture original positions */
    SpellVFXSwap( player.world_x, player.world_y,
                  hit->world_x, hit->world_y );

    /* Swap grid positions */
    int old_er = hit->row;
    int old_ec = hit->col;
    hit->row = pr;
    hit->col = pc;

    /* Swap world positions */
    float tmp_wx = player.world_x;
    float tmp_wy = player.world_y;
    PlayerSetWorldPos( hit->world_x, hit->world_y );
    hit->world_x = tmp_wx;
    hit->world_y = tmp_wy;

    /* Deal bonus damage to swapped enemy */
    int swap_dmg = dmg + 2;
    int fd = apply_totem_def( hit, swap_dmg );
    hit->hp -= fd;
    hit->turns_since_hit = 0;
    CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
    ConsolePushF( con, hit_color,
                  "%s teleports into %s for %d damage!",
                  player.name, t->name, fd );
    if ( hit->hp <= 0 )
      CombatHandleEnemyDeath( hit );

    /* Cleave around new player position (enemy's old tile) */
    static const int swap_dirs[8][2] = {
      {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };
    for ( int d = 0; d < 8; d++ )
    {
      int ar = old_er + swap_dirs[d][0];
      int ac = old_ec + swap_dirs[d][1];
      Enemy_t* adj = EnemyAt( enemies, num_enemies, ar, ac );
      if ( adj && adj != hit )
      {
        EnemyType_t* at = &g_enemy_types[adj->type_idx];
        int ad = apply_totem_def( adj, swap_dmg );
        adj->hp -= ad;
        adj->turns_since_hit = 0;
        CombatVFXSpawnNumber( adj->world_x, adj->world_y, ad, hit_color );
        ConsolePushF( con, hit_color, "%s crashes into %s for %d damage!",
                      player.name, at->name, ad );
        if ( adj->hp <= 0 )
          CombatHandleEnemyDeath( adj );
      }
    }

    SpellVFXSweep( old_er, old_ec, hit_color );

    consume_scroll( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  /* ---- REACH: 2-tile cardinal thrust, hits both tiles ---- */
  if ( strcmp( c->effect, "reach" ) == 0 )
  {
    a_AudioPlaySound( &sfx_merc_swing, NULL );
    int dr = ( target_row > pr ) ? 1 : ( target_row < pr ) ? -1 : 0;
    int dc = ( target_col > pc ) ? 1 : ( target_col < pc ) ? -1 : 0;

    int hits = 0;
    int cr = pr, cc = pc;
    for ( int step = 0; step < 2; step++ )
    {
      cr += dr;
      cc += dc;
      if ( !TileWalkable( cr, cc ) ) break;

      Enemy_t* hit = EnemyAt( enemies, num_enemies, cr, cc );
      if ( hit )
      {
        EnemyType_t* t = &g_enemy_types[hit->type_idx];
        int fd = apply_totem_def( hit, dmg );
        hit->hp -= fd;
        hit->turns_since_hit = 0;
        CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
        ConsolePushF( con, hit_color, "%s thrusts through %s for %d damage!",
                      player.name, t->name, fd );
        if ( hit->hp <= 0 )
          CombatHandleEnemyDeath( hit );
        hits++;
      }
    }

    SpellVFXThrust( pr, pc, dr, dc, 2, hit_color );

    if ( hits == 0 )
      ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                    "%s thrusts at the air.", player.name );

    InventoryRemove( inv_slot );
    consumable_used = 1;
    return 2;  /* food = free action */
  }

  /* ---- CLEAVE: hit all adjacent enemies ---- */
  if ( strcmp( c->effect, "cleave" ) == 0 )
  {
    a_AudioPlaySound( &sfx_merc_swing, NULL );
    int hits = 0;
    static const int dirs[8][2] = {
      {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };
    for ( int d = 0; d < 8; d++ )
    {
      int ar = pr + dirs[d][0];
      int ac = pc + dirs[d][1];
      Enemy_t* hit = EnemyAt( enemies, num_enemies, ar, ac );
      if ( hit )
      {
        EnemyType_t* t = &g_enemy_types[hit->type_idx];
        int fd = apply_totem_def( hit, dmg );
        hit->hp -= fd;
        hit->turns_since_hit = 0;
        CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
        ConsolePushF( con, hit_color, "%s cleaves %s for %d damage!",
                      player.name, t->name, fd );
        if ( hit->hp <= 0 )
          CombatHandleEnemyDeath( hit );
        hits++;
      }
    }

    SpellVFXSweep( pr, pc, hit_color );

    if ( hits == 0 )
      ConsolePushF( con, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                    "%s cleaves the air.", player.name );

    InventoryRemove( inv_slot );
    consumable_used = 1;
    return 2;  /* food = free action */
  }

  /* ---- FIRE_CONE: expanding cone + burn DOT ---- */
  if ( strcmp( c->effect, "fire_cone" ) == 0 )
  {
    int dr = ( target_row > pr ) ? 1 : ( target_row < pr ) ? -1 : 0;
    int dc = ( target_col > pc ) ? 1 : ( target_col < pc ) ? -1 : 0;
    int perp_r = -dc;
    int perp_c =  dr;

    int hits = 0;
    for ( int step = 1; step <= c->range; step++ )
    {
      int spread = step - 1;
      for ( int s = -spread; s <= spread; s++ )
      {
        int cr = pr + step * dr + s * perp_r;
        int cc = pc + step * dc + s * perp_c;
        if ( !TileWalkable( cr, cc ) ) continue;

        Enemy_t* hit = EnemyAt( enemies, num_enemies, cr, cc );
        if ( hit )
        {
          EnemyType_t* t = &g_enemy_types[hit->type_idx];
          int fd = apply_totem_def( hit, dmg );
          hit->hp -= fd;
          hit->turns_since_hit = 0;
          CombatVFXSpawnNumber( hit->world_x, hit->world_y, fd, hit_color );
          ConsolePushF( con, hit_color, "%s scorches %s for %d damage!",
                        player.name, t->name, fd );

          /* Apply burn DOT */
          if ( c->ticks > 0 )
          {
            hit->burn_ticks = c->ticks;
            hit->burn_dmg   = c->tick_damage;
          }

          if ( hit->hp <= 0 )
            CombatHandleEnemyDeath( hit );
          hits++;
        }
      }
    }

    if ( hits > 0 )
      ConsolePushF( con, hit_color,
                    "%s breathes fire! %d enemies scorched!",
                    player.name, hits );
    else
      ConsolePushF( con, hit_color,
                    "%s breathes fire! The flames lick at empty air.",
                    player.name );

    InventoryRemove( inv_slot );
    consumable_used = 1;
    return 2;  /* free action */
  }

  ConsolePushF( con, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                "Unknown effect: %s", c->effect );
  return 0;
}
