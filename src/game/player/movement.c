#include <stdlib.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "tween.h"
#include "sound_manager.h"
#include "movement.h"

extern Player_t player;

static World_t* world;
static TweenManager_t tweens;
static int moving = 0;
static int facing_left = 0;

static aTimer_t* rapid_timer = NULL;
static int rapid_active = 0;

/* Bounce animation */
static float bounce_oy = 0;

/* Viewport shake */
static float shake_ox = 0;
static float shake_oy = 0;
static aSoundEffect_t sfx_wall;

/* Lunge animation */
static float lunge_home_x, lunge_home_y;

static void bounce_down( void* data )
{
  (void)data;
  TweenFloat( &tweens, &bounce_oy, 0.0f, 0.09f, TWEEN_EASE_IN_QUAD );
}

static void shake_back( void* data )
{
  (void)data;
  TweenFloat( &tweens, &shake_ox, 0.0f, 0.06f, TWEEN_EASE_OUT_CUBIC );
  TweenFloat( &tweens, &shake_oy, 0.0f, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

void MovementInit( World_t* w )
{
  world = w;
  InitTweenManager( &tweens );
  moving = 0;
  facing_left = 0;
  bounce_oy = 0;
  shake_ox = 0;
  shake_oy = 0;
  if ( !rapid_timer ) rapid_timer = a_TimerCreate();
  rapid_active = 0;

  a_AudioLoadSound( "resources/soundeffects/wall_impact.wav", &sfx_wall );
}

void MovementUpdate( float dt )
{
  if ( moving )
  {
    UpdateTweens( &tweens, dt );
    if ( GetActiveTweenCount( &tweens ) == 0 )
      moving = 0;
  }
  else
  {
    /* Still update tweens for shake/bounce that outlast movement */
    UpdateTweens( &tweens, dt );
  }

  if ( rapid_active
       && a_TimerStarted( rapid_timer )
       && a_TimerGetTicks( rapid_timer ) > 700 )
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
  SoundManagerPlayFootstep();

  /* Little bounce — hop up 2px then back down */
  bounce_oy = 0;
  TweenFloatWithCallback( &tweens, &bounce_oy, -2.0f, 0.08f,
                          TWEEN_EASE_OUT_QUAD, bounce_down, NULL );

  a_TimerStart( rapid_timer );
  rapid_active = 1;
}

static void lunge_back_x( void* data )
{
  (void)data;
  TweenFloat( &tweens, &player.world_x, lunge_home_x, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

static void lunge_back_y( void* data )
{
  (void)data;
  TweenFloat( &tweens, &player.world_y, lunge_home_y, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

void PlayerLunge( int dr, int dc )
{
  if ( dr < 0 ) facing_left = 1;
  else if ( dr > 0 ) facing_left = 0;

  lunge_home_x = player.world_x;
  lunge_home_y = player.world_y;
  float dist = 3.0f;

  TweenFloatWithCallback( &tweens, &player.world_x,
                          player.world_x + dr * dist, 0.06f,
                          TWEEN_EASE_OUT_QUAD, lunge_back_x, NULL );
  TweenFloatWithCallback( &tweens, &player.world_y,
                          player.world_y + dc * dist, 0.06f,
                          TWEEN_EASE_OUT_QUAD, lunge_back_y, NULL );
}

void PlayerWallBump( int dr, int dc )
{
  a_AudioPlaySound( &sfx_wall, NULL );
  shake_ox = 0;
  shake_oy = 0;
  TweenFloatWithCallback( &tweens, &shake_ox, dr * 3.0f, 0.04f,
                          TWEEN_EASE_OUT_QUAD, shake_back, NULL );
  TweenFloatWithCallback( &tweens, &shake_oy, dc * 3.0f, 0.04f,
                          TWEEN_EASE_OUT_QUAD, NULL, NULL );
}

int   PlayerIsMoving( void )   { return moving; }
int   PlayerFacingLeft( void ) { return facing_left; }
int   RapidMoveActive( void )  { return rapid_active; }
float PlayerBounceOY( void )   { return bounce_oy; }
float PlayerShakeOX( void )    { return shake_ox; }
float PlayerShakeOY( void )    { return shake_oy; }
