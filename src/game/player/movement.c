#include <stdlib.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "tween.h"
#include "movement.h"

extern Player_t player;

static World_t* world;
static TweenManager_t tweens;
static int moving = 0;
static int facing_left = 0;

static aTimer_t* rapid_timer = NULL;
static int rapid_active = 0;

void MovementInit( World_t* w )
{
  world = w;
  InitTweenManager( &tweens );
  moving = 0;
  facing_left = 0;
  if ( !rapid_timer ) rapid_timer = a_TimerCreate();
  rapid_active = 0;
}

void MovementUpdate( float dt )
{
  if ( moving )
  {
    UpdateTweens( &tweens, dt );
    if ( GetActiveTweenCount( &tweens ) == 0 )
      moving = 0;
  }

  if ( rapid_active && a_TimerOneshot( rapid_timer, 500 ) )
    rapid_active = 0;
}

void PlayerGetTile( int* row, int* col )
{
  *row = (int)( player.world_x / world->tile_w );
  *col = (int)( player.world_y / world->tile_h );
}

int TileAdjacent( int r, int c )
{
  int pr, pc;
  PlayerGetTile( &pr, &pc );
  return ( abs( r - pr ) + abs( c - pc ) ) == 1;
}

int TileWalkable( int r, int c )
{
  if ( r < 0 || r >= world->width || c < 0 || c >= world->height ) return 0;
  int idx = c * world->width + r;
  return !world->background[idx].solid;
}

void PlayerStartMove( int r, int c )
{
  int pr, pc;
  PlayerGetTile( &pr, &pc );
  if ( r < pr ) facing_left = 1;
  else if ( r > pr ) facing_left = 0;

  float tx = r * world->tile_w + world->tile_w / 2.0f;
  float ty = c * world->tile_h + world->tile_h / 2.0f;
  TweenFloat( &tweens, &player.world_x, tx, 0.15f, TWEEN_EASE_OUT_CUBIC );
  TweenFloat( &tweens, &player.world_y, ty, 0.15f, TWEEN_EASE_OUT_CUBIC );
  moving = 1;

  a_TimerStart( rapid_timer );
  rapid_active = 1;
}

int PlayerIsMoving( void )   { return moving; }
int PlayerFacingLeft( void ) { return facing_left; }
int RapidMoveActive( void )  { return rapid_active; }
