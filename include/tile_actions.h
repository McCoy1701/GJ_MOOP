#ifndef __TILE_ACTIONS_H__
#define __TILE_ACTIONS_H__

#include "world.h"
#include "game_viewport.h"
#include "console.h"
#include "enemies.h"

void TileActionsInit( World_t* w, GameCamera_t* cam, Console_t* con,
                      aSoundEffect_t* move, aSoundEffect_t* click );

/* Process tile action menu input.  Returns 1 if input was consumed. */
int  TileActionsLogic( int mouse_moved, Enemy_t* enemies, int num_enemies );

/* Draw the tile action context menu. */
void TileActionsDraw( Enemy_t* enemies, int num_enemies );

/* Open the tile action menu at grid (row, col). */
void TileActionsOpen( int row, int col, int on_self );

int  TileActionsIsOpen( void );
void TileActionsClose( void );
int  TileActionsGetRow( void );
int  TileActionsGetCol( void );

#endif
