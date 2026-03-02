#ifndef __COMBAT_H__
#define __COMBAT_H__

#include "console.h"
#include "enemies.h"
#include "ground_items.h"

void CombatInit( Console_t* con );
void CombatSetEnemies( Enemy_t* list, int* count );
void CombatSetGroundItems( GroundItem_t* list, int* count );
void CombatUpdate( float dt );

/* Player attacks enemy. Enemy retaliates if still alive. Returns 1 if enemy dies. */
int  CombatAttack( Enemy_t* e );

/* Single enemy attacks the player. */
void CombatEnemyHit( Enemy_t* e );

/* Handle all effects of an enemy dying: defeated message, kill flag,
   gold drop, item drop, on-death hazard. Caller checks hp <= 0 first.
   Sets alive = 0. */
void CombatHandleEnemyDeath( Enemy_t* e );

/* Hit effect state - screen shake + red flash */
float CombatFlashAlpha( void );
float CombatShakeOX( void );
float CombatShakeOY( void );

#endif
