#ifndef __MOVEMENT_H__
#define __MOVEMENT_H__

#include "world.h"

void MovementInit( World_t* w );
void MovementUpdate( float dt );

void PlayerGetTile( int* row, int* col );
int  TileAdjacent( int r, int c );
int  TileWalkable( int r, int c );

/* Start a tween-based move to tile (r,c). Sets facing direction. */
void PlayerStartMove( int r, int c );

int  PlayerIsMoving( void );
int  PlayerFacingLeft( void );
int  RapidMoveActive( void );

#endif
