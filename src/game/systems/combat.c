#include <Archimedes.h>

#include "combat.h"
#include "enemies.h"
#include "player.h"
#include "console.h"
#include "items.h"

extern Player_t player;

static Console_t* console = NULL;
static aSoundEffect_t sfx_hit;

void CombatInit( Console_t* con )
{
  console = con;
  a_AudioLoadSound( "resources/soundeffects/hit_impact.wav", &sfx_hit );
}

int CombatAttack( Enemy_t* e )
{
  EnemyType_t* t = &g_enemy_types[e->type_idx];
  int pdmg = PlayerStat( "damage" );
  if ( pdmg < 1 ) pdmg = 1;
  e->hp -= pdmg;

  a_AudioPlaySound( &sfx_hit, NULL );
  ConsolePushF( console, (aColor_t){ 220, 180, 80, 255 },
                "You hit %s for %d damage.", t->name, pdmg );

  if ( e->hp <= 0 )
  {
    e->alive = 0;
    ConsolePushF( console, (aColor_t){ 80, 220, 80, 255 },
                  "You defeated the %s!", t->name );
    return 1;
  }

  /* Enemy retaliates */
  CombatEnemyHit( e );
  return 0;
}

void CombatEnemyHit( Enemy_t* e )
{
  EnemyType_t* t = &g_enemy_types[e->type_idx];
  int edmg = t->damage - PlayerStat( "defense" );
  if ( edmg < 1 ) edmg = 1;
  player.hp -= edmg;

  ConsolePushF( console, (aColor_t){ 220, 80, 80, 255 },
                "%s hits you for %d damage.", t->name, edmg );

  if ( player.hp <= 0 )
  {
    player.hp = 0;
    ConsolePush( console, "You have fallen...",
                 (aColor_t){ 200, 40, 40, 255 } );
  }
}
