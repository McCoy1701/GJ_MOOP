#ifndef __GAME_INPUT_H__
#define __GAME_INPUT_H__

#include "world.h"
#include "game_viewport.h"
#include "console.h"
#include "enemies.h"
#include "npc.h"

void GameInputInit( World_t* w, GameCamera_t* cam, Console_t* con,
                    Enemy_t* enemies, int* num_enemies,
                    NPC_t* npcs, int* num_npcs );

void GameInputFrameBegin( float dt );
int  GameInputOverlays( void );
int  GameInputEsc( void );
void GameInputInventory( void );
int  GameInputTargetMode( void );
int  GameInputLookMode( void );
void GameInputAutoMove( void );
void GameInputMovement( void );
void GameInputMouse( void );
void GameInputZoom( void );

void GameInputStartAutoPath( int goal_r, int goal_c );

int  GameInputHoverRow( void );
int  GameInputHoverCol( void );
int  GameInputTurnSkipped( void );
void GameInputClearTurnSkipped( void );

#endif
