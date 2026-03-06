#include <Archimedes.h>

#include <stdio.h>

#include "combat.h"
#include "combat_vfx.h"
#include "enemies.h"
#include "player.h"
#include "console.h"
#include "items.h"
#include "maps.h"
#include "tween.h"
#include "dialogue.h"
#include "movement.h"
#include "poison_pool.h"
#include "game_viewport.h"
#include "visibility.h"
#include "game_turns.h"
#include "dev_mode.h"

extern Player_t player;

static Console_t* console = NULL;
static aSoundEffect_t sfx_hit;

/* Enemy list for buff effects (cleave, reach) */
static Enemy_t* combat_enemies     = NULL;
static int*     combat_enemy_count = NULL;

/* Ground items for enemy drops */
static GroundItem_t* combat_ground_items = NULL;
static int*          combat_ground_count = NULL;

/* Screen shake + red flash on hit */
static TweenManager_t hit_tweens;
static float hit_shake_x = 0;
static float hit_shake_y = 0;
static float hit_flash_alpha = 0;

static void shake_back_x( void* data )
{
  (void)data;
  TweenFloat( &hit_tweens, &hit_shake_x, 0.0f, 0.08f, TWEEN_EASE_OUT_CUBIC );
}

static void shake_back_y( void* data )
{
  (void)data;
  TweenFloat( &hit_tweens, &hit_shake_y, 0.0f, 0.08f, TWEEN_EASE_OUT_CUBIC );
}

static void trigger_hit_effect( int dmg )
{
  StopTweensForTarget( &hit_tweens, &hit_shake_x );
  StopTweensForTarget( &hit_tweens, &hit_shake_y );

  /* Scale shake intensity by damage */
  float intensity = 1.0f;
  float duration  = 0.04f;
  if ( dmg >= 3 )      { intensity = 3.0f; duration = 0.06f; }
  else if ( dmg == 2 ) { intensity = 1.8f; duration = 0.05f; }

  float sx = ( ( rand() % 2 ) ? 2.0f : -2.0f ) * intensity;
  float sy = ( ( rand() % 2 ) ? 1.5f : -1.5f ) * intensity;
  hit_shake_x = 0;
  hit_shake_y = 0;
  TweenFloatWithCallback( &hit_tweens, &hit_shake_x, sx, duration,
                           TWEEN_EASE_OUT_QUAD, shake_back_x, NULL );
  TweenFloatWithCallback( &hit_tweens, &hit_shake_y, sy, duration,
                           TWEEN_EASE_OUT_QUAD, shake_back_y, NULL );

  /* Red flash - scale alpha by damage */
  float flash = ( dmg >= 3 ) ? 120.0f : ( dmg == 2 ) ? 80.0f : 60.0f;
  if ( hit_flash_alpha < 1.0f )
  {
    hit_flash_alpha = flash;
    float fade_dur = ( dmg >= 3 ) ? 0.5f : ( dmg == 2 ) ? 0.4f : 0.35f;
    TweenFloat( &hit_tweens, &hit_flash_alpha, 0.0f, fade_dur, TWEEN_EASE_OUT_QUAD );
  }
}

void CombatSetEnemies( Enemy_t* list, int* count )
{
  combat_enemies     = list;
  combat_enemy_count = count;
}

void CombatSetGroundItems( GroundItem_t* list, int* count )
{
  combat_ground_items = list;
  combat_ground_count = count;
}

void CombatInit( Console_t* con )
{
  console = con;
  a_AudioLoadSound( "resources/soundeffects/hit_impact.wav", &sfx_hit );
  InitTweenManager( &hit_tweens );
  hit_shake_x = 0;
  hit_shake_y = 0;
  hit_flash_alpha = 0;
}

void CombatUpdate( float dt )
{
  UpdateTweens( &hit_tweens, dt );
}

float CombatFlashAlpha( void ) { return hit_flash_alpha; }
float CombatShakeOX( void )    { return hit_shake_x; }
float CombatShakeOY( void )    { return hit_shake_y; }

void CombatHandleEnemyDeath( Enemy_t* e )
{
  EnemyType_t* t = &g_enemy_types[e->type_idx];

  e->alive = 0;
  ConsolePushF( console, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                "You defeated the %s!", t->name );

  char kill_flag[MAX_NAME_LENGTH + 8];
  snprintf( kill_flag, sizeof( kill_flag ), "%s_kills", t->key );
  FlagIncr( kill_flag );

  /* Gold drop */
  if ( t->gold_drop > 0 )
  {
    int gold_bonus = PlayerEquipEffect( "gold_bonus" );
    int total = t->gold_drop + gold_bonus;
    PlayerAddGold( total );
    ConsolePushF( console, (aColor_t){ 0xda, 0xaf, 0x20, 255 },
                  "Picked up %d gold.", total );
    if ( gold_bonus > 0 )
      CombatVFXSpawnText( e->world_x, e->world_y - 8,
                          "+1g", (aColor_t){ 0xda, 0xaf, 0x20, 255 } );
  }

  /* Drop item on death - check maps first, then consumables */
  if ( t->drop_item[0] != '\0' && combat_ground_items && combat_ground_count )
  {
    int mi = MapByKey( t->drop_item );
    if ( mi >= 0 )
    {
      GroundItemSpawnMap( combat_ground_items, combat_ground_count,
                          mi, e->row, e->col, 16, 16 );
      ConsolePushF( console, g_maps[mi].color,
                    "The %s dropped %s!", t->name, g_maps[mi].name );
    }
    else
    {
      int ci = ConsumableByKey( t->drop_item );
      if ( ci >= 0 )
      {
        GroundItemSpawn( combat_ground_items, combat_ground_count,
                         ci, e->row, e->col, 16, 16 );
        ConsolePushF( console, g_consumables[ci].color,
                      "The %s dropped %s!", t->name, g_consumables[ci].name );
      }
      else
      {
        int ei = EquipmentByKey( t->drop_item );
        if ( ei >= 0 )
        {
          GroundItemSpawnEquipment( combat_ground_items, combat_ground_count,
                                    ei, e->row, e->col, 16, 16 );
          ConsolePushF( console, g_equipment[ei].color,
                        "The %s dropped %s!", t->name, g_equipment[ei].name );
        }
      }
    }
  }

  /* On-death hazard: spawn poison pool */
  if ( t->on_death[0] != '\0'
       && strcmp( t->on_death, "poison_pool" ) == 0 )
  {
    PoisonPoolSpawn( e->row, e->col, t->pool_duration, t->pool_damage,
                     t->color );
    ConsolePushF( console, t->color,
                  "The %s dissolves into a pool of goo!", t->name );
    GameTurnsShowSkipHint();
  }

  /* Linked death: shaman dies → totem crumbles */
  if ( strcmp( t->ai, "shaman" ) == 0
       && combat_enemies && combat_enemy_count )
  {
    for ( int i = 0; i < *combat_enemy_count; i++ )
    {
      if ( !combat_enemies[i].alive ) continue;
      if ( strcmp( g_enemy_types[combat_enemies[i].type_idx].ai, "static" ) != 0 )
        continue;
      combat_enemies[i].alive = 0;
      CombatVFXSpawnText( combat_enemies[i].world_x, combat_enemies[i].world_y,
                          "Crumbles!", (aColor_t){ 160, 120, 60, 255 } );
      ConsolePushF( console, (aColor_t){ 160, 120, 60, 255 },
                    "The War Totem crumbles!" );
    }
  }

  /* Linked death: horror dies → baby horrors dissolve */
  if ( strcmp( t->ai, "horror" ) == 0
       && combat_enemies && combat_enemy_count )
  {
    for ( int i = 0; i < *combat_enemy_count; i++ )
    {
      if ( !combat_enemies[i].alive ) continue;
      if ( strcmp( g_enemy_types[combat_enemies[i].type_idx].ai, "baby_horror" ) != 0 )
        continue;
      combat_enemies[i].alive = 0;
      CombatVFXSpawnText( combat_enemies[i].world_x, combat_enemies[i].world_y,
                          "Dissolves!", (aColor_t){ 140, 40, 80, 255 } );
      ConsolePushF( console, (aColor_t){ 140, 40, 80, 255 },
                    "The Baby Horror dissolves!" );
    }
  }
}

/* Totem aura: sum a stat field from nearby static enemies */
#define TOTEM_RADIUS 3

static int totem_buff_at( Enemy_t* target, int use_damage )
{
  if ( !combat_enemies || !combat_enemy_count ) return 0;
  int bonus = 0;
  for ( int i = 0; i < *combat_enemy_count; i++ )
  {
    Enemy_t* t = &combat_enemies[i];
    if ( !t->alive || t == target ) continue;
    EnemyType_t* et = &g_enemy_types[t->type_idx];
    if ( strcmp( et->ai, "static" ) != 0 ) continue;
    int dr = abs( t->row - target->row );
    int dc = abs( t->col - target->col );
    if ( dr + dc <= TOTEM_RADIUS )
      bonus += use_damage ? et->damage : et->defense;
  }
  return bonus;
}

int CombatTotemDefenseFor( Enemy_t* e )
{
  return totem_buff_at( e, 0 );
}

/* Helper: deal damage to an enemy, handle death + kill tracking.
   Returns 1 if the enemy died. */
static int deal_damage( Enemy_t* e, int dmg, aColor_t vfx_color )
{
  /* Totem defense aura: reduce incoming damage */
  int def = totem_buff_at( e, 0 );
  if ( def > 0 )
  {
    dmg -= def;
    if ( dmg < 1 ) dmg = 1;
  }

  e->hp -= dmg;
  e->turns_since_hit = 0;

  CombatVFXSpawnNumber( e->world_x, e->world_y, dmg, vfx_color );

  if ( e->hp <= 0 )
  {
    CombatHandleEnemyDeath( e );
    return 1;
  }
  return 0;
}

/* Same as deal_damage but skips totem defense aura */
static int deal_damage_ignore_def( Enemy_t* e, int dmg, aColor_t vfx_color )
{
  e->hp -= dmg;
  e->turns_since_hit = 0;
  CombatVFXSpawnNumber( e->world_x, e->world_y, dmg, vfx_color );
  if ( e->hp <= 0 )
  {
    CombatHandleEnemyDeath( e );
    return 1;
  }
  return 0;
}

/* Resolve food buff effects after primary hit */
static void resolve_buff( Enemy_t* primary )
{
  (void)primary;
  if ( !player.buff.active ) return;

  /* Lifesteal: heal player */
  if ( strcmp( player.buff.effect, "lifesteal" ) == 0 )
  {
    int heal = player.buff.heal;
    if ( heal > 0 )
    {
      PlayerHeal( heal );
      CombatVFXSpawnNumber( player.world_x, player.world_y, heal,
                            (aColor_t){ 0x75, 0xa7, 0x43, 255 } );
      ConsolePushF( console, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                    "Lifesteal heals %d HP.", heal );
    }
  }
  /* All other buffs (cleave, reach, none) - bonus damage already applied */

  PlayerClearBuff();
}

int CombatAttack( Enemy_t* e )
{
  EnemyType_t* t = &g_enemy_types[e->type_idx];
  int pdmg = PlayerStat( "damage" );

  /* Add food buff bonus damage */
  if ( player.buff.active )
    pdmg += player.buff.bonus_damage;

  /* Passive: first_strike - bonus damage on first attack per room */
  int fs = PlayerEquipEffect( "first_strike" );
  if ( fs > 0 && player.first_strike_active )
  {
    pdmg += fs;
    PlayerConsumeFirstStrike();
    ConsolePushF( console, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                  "First Strike! +%d bonus damage!", fs );
  }

  /* Passive: berserk - +effect_value damage per 40% HP missing (max 2) */
  int berserk = PlayerEquipEffect( "berserk" );
  if ( berserk > 0 && player.max_hp > 0 )
  {
    int missing_pct = ( ( player.max_hp - player.hp ) * 100 ) / player.max_hp;
    int stacks = missing_pct / 40;
    if ( stacks > 2 ) stacks = 2;
    if ( stacks > 0 )
    {
      pdmg += berserk * stacks;
      ConsolePushF( console, (aColor_t){ 0xa5, 0x30, 0x30, 255 },
                    "Berserk! +%d damage!", berserk * stacks );
    }
  }

  /* Empower buff - double total damage */
  if ( player.buff.active && strcmp( player.buff.effect, "empower" ) == 0 )
  {
    pdmg *= 2;
    ConsolePushF( console, (aColor_t){ 60, 85, 110, 255 },
                  "Empowered! Double damage!" );
    PlayerClearBuff();
  }

  /* Passive: armor_break - every Nth attack ignores enemy defense */
  int armor_break = PlayerEquipEffect( "armor_break" );
  int broke_armor = 0;
  if ( armor_break > 0 )
  {
    player.attack_counter++;
    if ( player.attack_counter % armor_break == 0 )
    {
      broke_armor = 1;
      ConsolePushF( console, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                    "CRACK! Defense shattered!" );
    }
  }

  if ( pdmg < 1 ) pdmg = 1;

  a_AudioPlaySound( &sfx_hit, NULL );
  ConsolePushF( console, (aColor_t){ 0xe8, 0xc1, 0x70, 255 },
                "You hit %s for %d damage.", t->name, pdmg );

  int killed = broke_armor
    ? deal_damage_ignore_def( e, pdmg, (aColor_t){ 0xeb, 0xed, 0xe9, 255 } )
    : deal_damage( e, pdmg, (aColor_t){ 0xeb, 0xed, 0xe9, 255 } );

  /* Passive: poison - apply DOT on melee hit */
  int psn = PlayerEquipEffect( "poison" );
  if ( psn > 0 && e->alive )
  {
    e->poison_ticks = 3;
    e->poison_dmg   = psn;
    ConsolePushF( console, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                  "Poisoned %s! (%d dmg for 3 turns)",
                  g_enemy_types[e->type_idx].name, psn );
  }

  /* Resolve buff effects after primary hit */
  if ( player.buff.active )
    resolve_buff( e );

  return killed;
}

void CombatEnemyHit( Enemy_t* e )
{
  if ( DevModeNoclip() ) return;
  EnemyType_t* t = &g_enemy_types[e->type_idx];
  int edmg = t->damage + totem_buff_at( e, 1 ) - PlayerStat( "defense" );
  if ( edmg < 1 ) edmg = 1;

  /* Passive: dodge - every Nth incoming hit is dodged */
  int dodge = PlayerEquipEffect( "dodge" );
  if ( dodge > 0 )
  {
    player.dodge_counter++;
    if ( player.dodge_counter % dodge == 0 )
    {
      CombatVFXSpawnText( player.world_x, player.world_y,
                          "Dodged!", (aColor_t){ 0x75, 0xa7, 0x43, 255 } );
      ConsolePushF( console, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                    "Flickered! Attack dodged!" );
      return;
    }
  }

  PlayerTakeDamage( edmg );

  /* Passive: mana_shield - lethal hit consumes a scroll instead */
  if ( player.hp <= 0 )
  {
    int ms = PlayerEquipEffect( "mana_shield" );
    if ( ms > 0 )
    {
      /* Find a random scroll in inventory */
      int scroll_slots[MAX_INVENTORY];
      int num_scrolls = 0;
      for ( int i = 0; i < player.max_inventory; i++ )
      {
        if ( player.inventory[i].type == INV_CONSUMABLE
             && strcmp( g_consumables[player.inventory[i].index].type, "scroll" ) == 0 )
          scroll_slots[num_scrolls++] = i;
      }
      if ( num_scrolls > 0 )
      {
        int pick = scroll_slots[rand() % num_scrolls];
        const char* sname = g_consumables[player.inventory[pick].index].name;
        player.hp += edmg; /* undo lethal: restore HP to pre-hit value */
        InventoryRemove( pick );
        CombatVFXSpawnText( player.world_x, player.world_y,
                            "Shielded!", (aColor_t){ 0x64, 0x64, 0xc8, 255 } );
        ConsolePushF( console, (aColor_t){ 0x64, 0x64, 0xc8, 255 },
                      "Grimoire Locket consumed %s!", sname );
        return;
      }
    }
  }

  float num_scale = ( edmg >= 3 ) ? 2.5f : ( edmg == 2 ) ? 2.0f : 1.5f;
  CombatVFXSpawnNumberScaled( player.world_x, player.world_y, edmg,
                              (aColor_t){ 0xcf, 0x57, 0x3c, 255 }, num_scale );
  trigger_hit_effect( edmg );

  ConsolePushF( console, (aColor_t){ 0xcf, 0x57, 0x3c, 255 },
                "%s hits you for %d damage.", t->name, edmg );

  if ( player.hp <= 0 )
  {
    ConsolePush( console, "You have fallen...",
                 (aColor_t){ 0xa5, 0x30, 0x30, 255 } );
  }

  /* Passive: thorns - reflect damage back (not if dead) */
  if ( player.hp > 0 )
  {
    int thorns = PlayerEquipEffect( "thorns" );
    if ( thorns > 0 && e->alive )
    {
      ConsolePushF( console, (aColor_t){ 0xde, 0x9e, 0x41, 255 },
                    "Thorns deals %d damage to %s.", thorns, t->name );
      deal_damage( e, thorns, (aColor_t){ 0xde, 0x9e, 0x41, 255 } );
    }
  }
}

void CombatDrawTotemAura( aRectf_t vp_rect, GameCamera_t* cam, World_t* w )
{
  if ( !combat_enemies || !combat_enemy_count ) return;

  int tw = w->tile_w;
  int th = w->tile_h;

  for ( int i = 0; i < *combat_enemy_count; i++ )
  {
    Enemy_t* t = &combat_enemies[i];
    if ( !t->alive ) continue;
    if ( strcmp( g_enemy_types[t->type_idx].ai, "static" ) != 0 ) continue;

    /* Draw aura on tiles within Manhattan distance */
    for ( int dr = -TOTEM_RADIUS; dr <= TOTEM_RADIUS; dr++ )
    {
      for ( int dc = -TOTEM_RADIUS; dc <= TOTEM_RADIUS; dc++ )
      {
        if ( abs( dr ) + abs( dc ) > TOTEM_RADIUS ) continue;
        int r = t->row + dr;
        int c = t->col + dc;
        if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) continue;
        if ( VisibilityGet( r, c ) < 0.01f ) continue;

        float wx = r * tw + tw / 2.0f;
        float wy = c * th + th / 2.0f;

        /* Fade with distance: center brighter, edges dimmer */
        int dist = abs( dr ) + abs( dc );
        int alpha = ( dist == 0 ) ? 50 : 35 - dist * 5;
        if ( alpha < 10 ) alpha = 10;

        GV_DrawFilledRect( vp_rect, cam, wx, wy,
                           (float)tw, (float)th,
                           (aColor_t){ 160, 120, 60, alpha } );
      }
    }
  }
}
