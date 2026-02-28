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

/* Lunge toward (dr,dc) and snap back — attack animation */
void PlayerLunge( int dr, int dc );

/* Bump into a wall — plays impact sound + viewport shake */
void PlayerWallBump( int dr, int dc );

int   PlayerIsMoving( void );
int   PlayerFacingLeft( void );
int   RapidMoveActive( void );
float PlayerBounceOY( void );
float PlayerShakeOX( void );
float PlayerShakeOY( void );

/* rendering.c — draw player sprite with bounce/facing */
#include "game_viewport.h"

void PlayerDraw( aRectf_t vp_rect, GameCamera_t* cam, int gfx_mode );

#endif
