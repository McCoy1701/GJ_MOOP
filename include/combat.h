#ifndef __COMBAT_H__
#define __COMBAT_H__

#include "console.h"
#include "enemies.h"

void CombatInit( Console_t* con );

/* Player attacks enemy. Enemy retaliates if still alive. Returns 1 if enemy dies. */
int  CombatAttack( Enemy_t* e );

/* Single enemy attacks the player. */
void CombatEnemyHit( Enemy_t* e );

#endif
