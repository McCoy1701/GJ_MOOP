#include <Archimedes.h>
#include "tween.h"
#include "transitions.h"

/* ================================================================== */
/* OUTRO — class_select fade-out, jump+turn, drop into viewport        */
/* ================================================================== */

#define OUTRO_NONE  0
#define OUTRO_FADE  1
#define OUTRO_JUMP  2   /* bounce up + flip */
#define OUTRO_DROP  3   /* shrink + move to game viewport spot */
#define OUTRO_LAND  4   /* landing bounce at target */

static int outro_phase = OUTRO_NONE;
static int outro_done_flag = 0;
static TweenManager_t outro_tweens;

static float outro_ui_alpha;      /* 1 → 0: class select UI fades out */
static float outro_vp_alpha;      /* 0 → 1: game viewport box fades in */
static aTimer_t* outro_vp_timer;  /* delay before viewport fade-in */
static int   outro_vp_started;    /* 1 once viewport fade has begun */
static float outro_char_oy;       /* bounce Y offset */
static float outro_drop_t;        /* drop interpolation: 0 → 1 */
static int   outro_flipped;       /* 1 after the sprite flips */
static float outro_flip_delay;    /* dummy tween target for delayed flip */

/* ------------------------------------------------------------------ */
/* Phase callbacks                                                     */
/* ------------------------------------------------------------------ */

static void outro_finish( void* data )
{
  (void)data;
  outro_phase = OUTRO_NONE;
  outro_done_flag = 1;
}

static void outro_start_land( void* data )
{
  (void)data;
  outro_phase = OUTRO_LAND;

  outro_char_oy = -12.0f;

  /* Landing bounce at target position */
  TweenFloatWithCallback( &outro_tweens, &outro_char_oy, 0.0f,
                           0.5f, TWEEN_EASE_OUT_BOUNCE,
                           outro_finish, NULL );
}

static void outro_start_drop( void* data )
{
  (void)data;
  outro_phase = OUTRO_DROP;

  outro_drop_t = 0.0f;

  /* Move + shrink into the game viewport spot, chain to landing bounce */
  TweenFloatWithCallback( &outro_tweens, &outro_drop_t, 1.0f,
                           0.7f, TWEEN_EASE_IN_OUT_CUBIC,
                           outro_start_land, NULL );
}

static void outro_do_flip( void* data )
{
  (void)data;
  outro_flipped = 1;
}

static void outro_start_jump( void* data )
{
  (void)data;
  outro_phase = OUTRO_JUMP;

  outro_char_oy = -18.0f;

  /* Flip mid-air after a short delay */
  outro_flip_delay = 0.0f;
  TweenFloatWithCallback( &outro_tweens, &outro_flip_delay, 1.0f,
                           0.2f, TWEEN_LINEAR,
                           outro_do_flip, NULL );

  /* Short bounce, then chain to drop */
  TweenFloatWithCallback( &outro_tweens, &outro_char_oy, 0.0f,
                           0.3f, TWEEN_EASE_OUT_BOUNCE,
                           outro_start_drop, NULL );
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void TransitionOutroStart( void )
{
  InitTweenManager( &outro_tweens );

  outro_phase      = OUTRO_FADE;
  outro_done_flag  = 0;
  outro_ui_alpha   = 1.0f;
  outro_vp_alpha   = 0.0f;
  outro_vp_started = 0;
  outro_char_oy    = 0.0f;
  outro_drop_t     = 0.0f;
  outro_flipped    = 0;

  /* 1s delay before viewport box fades in */
  if ( !outro_vp_timer ) outro_vp_timer = a_TimerCreate();
  a_TimerStart( outro_vp_timer );

  /* Quick fade out, then chain to jump */
  TweenFloatWithCallback( &outro_tweens, &outro_ui_alpha, 0.0f,
                           0.3f, TWEEN_EASE_IN_QUAD,
                           outro_start_jump, NULL );
}

int TransitionOutroUpdate( float dt )
{
  if ( outro_phase == OUTRO_NONE ) return 0;

  /* Delayed viewport fade-in */
  if ( !outro_vp_started && a_TimerOneshot( outro_vp_timer, 1000 ) )
  {
    outro_vp_started = 1;
    TweenFloat( &outro_tweens, &outro_vp_alpha, 1.0f,
                0.4f, TWEEN_EASE_OUT_CUBIC );
  }

  UpdateTweens( &outro_tweens, dt );
  return 1;
}

void TransitionOutroSkip( void )
{
  StopAllTweens( &outro_tweens );
  outro_ui_alpha  = 0.0f;
  outro_vp_alpha  = 1.0f;
  outro_char_oy   = 0.0f;
  outro_drop_t    = 1.0f;
  outro_flipped   = 1;
  outro_phase     = OUTRO_NONE;
  outro_done_flag = 1;
}

int TransitionOutroActive( void )
{
  return outro_phase != OUTRO_NONE;
}

int TransitionOutroDone( void )
{
  if ( outro_done_flag )
  {
    outro_done_flag = 0;
    return 1;
  }
  return 0;
}

float TransitionGetOutroAlpha( void )    { return outro_ui_alpha; }
float TransitionGetOutroVPAlpha( void )  { return outro_vp_alpha; }
float TransitionGetOutroCharOY( void )   { return outro_char_oy; }
float TransitionGetOutroDropT( void )    { return outro_drop_t; }
int   TransitionGetOutroFlipped( void )  { return outro_flipped; }

/* ================================================================== */
/* INTRO — game_scene zoom + UI slide-in                               */
/* ================================================================== */

#define INTRO_NONE  0
#define INTRO_VP    1   /* viewport fades in first */
#define INTRO_UI    2

static int intro_phase = INTRO_NONE;
static TweenManager_t intro_tweens;

/* Camera zoom */
static float intro_viewport_h;

/* Viewport fade */
static float intro_viewport_alpha;

/* UI panel offsets */
static float intro_topbar_oy;
static float intro_right_ox;
static float intro_console_oy;
static float intro_ui_alpha;
static int   intro_show_labels;

/* ------------------------------------------------------------------ */
/* Phase callbacks                                                     */
/* ------------------------------------------------------------------ */

static void intro_finish( void* data )
{
  (void)data;
  intro_phase = INTRO_NONE;
  intro_show_labels = 1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void TransitionIntroStart( void )
{
  InitTweenManager( &intro_tweens );

  intro_phase          = INTRO_VP;
  intro_show_labels    = 0;
  intro_viewport_h     = 64.0f;
  intro_viewport_alpha = 0.0f;    /* map fades in from black */

  /* UI hidden offscreen */
  intro_topbar_oy  = -80.0f;
  intro_right_ox   = 300.0f;
  intro_console_oy = 160.0f;
  intro_ui_alpha   = 0.0f;

  /* Map fade-in */
  TweenFloat( &intro_tweens, &intro_viewport_alpha, 1.0f,
              0.8f, TWEEN_EASE_OUT_CUBIC );

  /* UI slides in simultaneously */
  TweenFloat( &intro_tweens, &intro_topbar_oy, 0.0f,
              0.6f, TWEEN_EASE_OUT_CUBIC );

  TweenFloat( &intro_tweens, &intro_right_ox, 0.0f,
              0.7f, TWEEN_EASE_OUT_CUBIC );

  TweenFloat( &intro_tweens, &intro_console_oy, 0.0f,
              0.6f, TWEEN_EASE_OUT_CUBIC );

  TweenFloatWithCallback( &intro_tweens, &intro_ui_alpha, 1.0f,
                           0.8f, TWEEN_EASE_IN_OUT_QUAD,
                           intro_finish, NULL );
}

int TransitionIntroUpdate( float dt )
{
  if ( intro_phase == INTRO_NONE ) return 0;
  UpdateTweens( &intro_tweens, dt );
  return 1;
}

void TransitionIntroSkip( void )
{
  StopAllTweens( &intro_tweens );

  intro_viewport_h     = 64.0f;
  intro_viewport_alpha = 1.0f;
  intro_topbar_oy      = 0.0f;
  intro_right_ox       = 0.0f;
  intro_console_oy     = 0.0f;
  intro_ui_alpha       = 1.0f;
  intro_show_labels    = 1;
  intro_phase          = INTRO_NONE;
}

int TransitionIntroActive( void )
{
  return intro_phase != INTRO_NONE;
}

/* ------------------------------------------------------------------ */
/* Getters                                                             */
/* ------------------------------------------------------------------ */

float TransitionGetViewportAlpha( void ) { return intro_viewport_alpha; }
float TransitionGetViewportH( void )   { return intro_viewport_h; }
float TransitionGetTopBarOY( void )    { return intro_topbar_oy; }
float TransitionGetRightOX( void )     { return intro_right_ox; }
float TransitionGetConsoleOY( void )   { return intro_console_oy; }
float TransitionGetUIAlpha( void )     { return intro_ui_alpha; }
int   TransitionShowLabels( void )     { return intro_show_labels; }
