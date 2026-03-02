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

static void trigger_hit_effect( void )
{
  StopTweensForTarget( &hit_tweens, &hit_shake_x );
  StopTweensForTarget( &hit_tweens, &hit_shake_y );

  float sx = ( ( rand() % 2 ) ? 2.0f : -2.0f );
  float sy = ( ( rand() % 2 ) ? 1.5f : -1.5f );
  hit_shake_x = 0;
  hit_shake_y = 0;
  TweenFloatWithCallback( &hit_tweens, &hit_shake_x, sx, 0.04f,
                           TWEEN_EASE_OUT_QUAD, shake_back_x, NULL );
  TweenFloatWithCallback( &hit_tweens, &hit_shake_y, sy, 0.04f,
                           TWEEN_EASE_OUT_QUAD, shake_back_y, NULL );

  /* Red flash - only start if not already active */
  if ( hit_flash_alpha < 1.0f )
  {
    hit_flash_alpha = 60.0f;
    TweenFloat( &hit_tweens, &hit_flash_alpha, 0.0f, 0.35f, TWEEN_EASE_OUT_QUAD );
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
    PlayerAddGold( t->gold_drop );
    ConsolePushF( console, (aColor_t){ 0xda, 0xaf, 0x20, 255 },
                  "Picked up %d gold.", t->gold_drop );
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
  }
}

/* Helper: deal damage to an enemy, handle death + kill tracking.
   Returns 1 if the enemy died. */
static int deal_damage( Enemy_t* e, int dmg, aColor_t vfx_color )
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

  if ( pdmg < 1 ) pdmg = 1;

  a_AudioPlaySound( &sfx_hit, NULL );
  ConsolePushF( console, (aColor_t){ 0xe8, 0xc1, 0x70, 255 },
                "You hit %s for %d damage.", t->name, pdmg );

  int killed = deal_damage( e, pdmg, (aColor_t){ 0xeb, 0xed, 0xe9, 255 } );

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
  EnemyType_t* t = &g_enemy_types[e->type_idx];
  int edmg = t->damage - PlayerStat( "defense" );
  if ( edmg < 1 ) edmg = 1;
  PlayerTakeDamage( edmg );

  CombatVFXSpawnNumber( player.world_x, player.world_y, edmg,
                        (aColor_t){ 0xcf, 0x57, 0x3c, 255 } );
  trigger_hit_effect();

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
