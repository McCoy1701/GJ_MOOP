#ifndef __GAME_TURNS_H__
#define __GAME_TURNS_H__

#include "console.h"
#include "enemies.h"
#include "npc.h"
#include "ground_items.h"
#include "world.h"

void  GameTurnsInit( Console_t* con, aSoundEffect_t* click,
                     Enemy_t* enemies, int* num_enemies,
                     NPC_t* npcs, int* num_npcs,
                     GroundItem_t* items, int* num_items,
                     World_t* world );

void  GameTurnsUpdateSystems( float dt );
void  GameTurnsHandleTurnEnd( float dt, int turn_skipped );
void  GameTurnsGetPlayerTile( int* r, int* c );

float GameTurnsEnemyDelay( void );
void  GameTurnsSetEnemyDelay( float d );

/* Hint arrow state */
float GameTurnsHintTimer( void );
float GameTurnsSkipHintTimer( void );
float GameTurnsInvExpandHintTimer( void );
void  GameTurnsTickHint( float dt );
void  GameTurnsShowSkipHint( void );

#endif
