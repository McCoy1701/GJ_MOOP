#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <Archimedes.h>

#include "spell_vfx.h"
#include "tween.h"

/* ------------------------------------------------------------------ */
/*  Data structures                                                    */
/* ------------------------------------------------------------------ */

#define MAX_PROJS      4
#define MAX_ZAPS       2
#define MAX_FLASHES   16
#define ZAP_SEGMENTS   6

typedef struct
{
  float start_wx, start_wy;
  float end_wx, end_wy;
  float progress;           /* 0 → 1 */
  aColor_t color;
  int   shape;              /* 0=diamond (frost), 1=fireball */
  int   active;

  /* Fireball AoE deferred data */
  int   aoe_row, aoe_col, aoe_radius;
} SpellProj_t;

typedef struct
{
  float start_wx, start_wy;
  float end_wx, end_wy;
  float progress;           /* 0 → 1 : how far the bolt extends */
  float alpha;              /* 255 → 0 : fade after bolt reaches target */
  int   seed;
  int   active;
} SpellZap_t;

typedef struct
{
  float    wx, wy;
  float    alpha;
  aColor_t color;
  int      active;
} TileFlash_t;

static World_t*       world;
static TweenManager_t spell_tweens;

static SpellProj_t  projs[MAX_PROJS];
static SpellZap_t   zaps[MAX_ZAPS];
static TileFlash_t  flashes[MAX_FLASHES];

static float    screen_flash_alpha = 0;
static aColor_t screen_flash_color = { 0, 0, 0, 0 };

static float shake_x = 0;
static float shake_y = 0;

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static int find_proj( void )
{
  for ( int i = 0; i < MAX_PROJS; i++ )
    if ( !projs[i].active ) return i;
  return -1;
}

static int find_zap( void )
{
  for ( int i = 0; i < MAX_ZAPS; i++ )
    if ( !zaps[i].active ) return i;
  return -1;
}

static int find_flash( void )
{
  for ( int i = 0; i < MAX_FLASHES; i++ )
    if ( !flashes[i].active ) return i;
  return -1;
}

static void spawn_flash( float wx, float wy, aColor_t color, float start_a,
                          float duration )
{
  int s = find_flash();
  if ( s < 0 ) return;
  flashes[s].wx     = wx;
  flashes[s].wy     = wy;
  flashes[s].color  = color;
  flashes[s].alpha  = start_a;
  flashes[s].active = 1;
  TweenFloat( &spell_tweens, &flashes[s].alpha, 0.0f, duration,
              TWEEN_EASE_OUT_QUAD );
}

static void spawn_screen_flash( aColor_t color, float start_a, float duration )
{
  StopTweensForTarget( &spell_tweens, &screen_flash_alpha );
  screen_flash_color = color;
  screen_flash_alpha = start_a;
  TweenFloat( &spell_tweens, &screen_flash_alpha, 0.0f, duration,
              TWEEN_EASE_OUT_QUAD );
}

/* Shake: push to offset, callback tweens back to 0 */

static void shake_back_x( void* data )
{
  (void)data;
  TweenFloat( &spell_tweens, &shake_x, 0.0f, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

static void shake_back_y( void* data )
{
  (void)data;
  TweenFloat( &spell_tweens, &shake_y, 0.0f, 0.06f, TWEEN_EASE_OUT_CUBIC );
}

static void trigger_shake( float mag_x, float mag_y, float push_dur,
                            float back_dur )
{
  StopTweensForTarget( &spell_tweens, &shake_x );
  StopTweensForTarget( &spell_tweens, &shake_y );
  shake_x = 0;
  shake_y = 0;
  float sx = ( rand() % 2 ) ? mag_x : -mag_x;
  float sy = ( rand() % 2 ) ? mag_y : -mag_y;
  (void)back_dur;
  TweenFloatWithCallback( &spell_tweens, &shake_x, sx, push_dur,
                           TWEEN_EASE_OUT_QUAD, shake_back_x, NULL );
  TweenFloatWithCallback( &spell_tweens, &shake_y, sy, push_dur,
                           TWEEN_EASE_OUT_QUAD, shake_back_y, NULL );
}

/* Simple LCG for deterministic per-frame jitter */
static int lcg_next( int seed )
{
  return ( seed * 1103515245 + 12345 ) & 0x7fffffff;
}

/* ------------------------------------------------------------------ */
/*  Callbacks                                                          */
/* ------------------------------------------------------------------ */

/* Spark: zap line reached target → tile flash + shake + begin fade */
static void spark_arrive_cb( void* data )
{
  SpellZap_t* z = (SpellZap_t*)data;
  /* Tile flash at target */
  spawn_flash( z->end_wx, z->end_wy,
               (aColor_t){ 0xff, 0xff, 0x96, 255 }, 120.0f, 0.2f );
  /* Small shake */
  trigger_shake( 1.5f, 1.0f, 0.03f, 0.06f );
  /* Fade the zap line out */
  TweenFloat( &spell_tweens, &z->alpha, 0.0f, 0.12f, TWEEN_EASE_OUT_QUAD );
}

/* Frost: diamond arrived → tile flash + blue screen tint */
static void frost_arrive_cb( void* data )
{
  SpellProj_t* p = (SpellProj_t*)data;
  p->active = 0;
  spawn_flash( p->end_wx, p->end_wy,
               (aColor_t){ 0x64, 0xb4, 0xff, 255 }, 100.0f, 0.3f );
  spawn_screen_flash( (aColor_t){ 0x64, 0xb4, 0xff, 255 }, 40.0f, 0.25f );
}

/* Fireball: projectile arrived → AoE tile flashes + shake + screen flash */
static void fireball_arrive_cb( void* data )
{
  SpellProj_t* p = (SpellProj_t*)data;
  p->active = 0;

  int tw = world->tile_w;
  int th = world->tile_h;
  int rad = p->aoe_radius;

  /* Spawn tile flashes for all tiles in AoE radius */
  for ( int r = p->aoe_row - rad; r <= p->aoe_row + rad; r++ )
  {
    for ( int c = p->aoe_col - rad; c <= p->aoe_col + rad; c++ )
    {
      if ( r < 0 || r >= world->width || c < 0 || c >= world->height )
        continue;
      int dist = abs( r - p->aoe_row ) + abs( c - p->aoe_col );
      if ( dist > rad ) continue;

      float fx = r * tw + tw / 2.0f;
      float fy = c * th + th / 2.0f;
      /* Center tile brighter */
      float a = ( dist == 0 ) ? 180.0f : 120.0f;
      spawn_flash( fx, fy, (aColor_t){ 0xff, 0x64, 0x1e, 255 }, a, 0.35f );
    }
  }

  /* Big shake */
  trigger_shake( 2.5f, 2.0f, 0.04f, 0.1f );

  /* Orange screen flash */
  spawn_screen_flash( (aColor_t){ 0xff, 0x64, 0x1e, 255 }, 50.0f, 0.3f );
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void SpellVFXInit( World_t* w )
{
  world = w;
  InitTweenManager( &spell_tweens );
  memset( projs, 0, sizeof( projs ) );
  memset( zaps, 0, sizeof( zaps ) );
  memset( flashes, 0, sizeof( flashes ) );
  screen_flash_alpha = 0;
  shake_x = 0;
  shake_y = 0;
}

void SpellVFXUpdate( float dt )
{
  UpdateTweens( &spell_tweens, dt );

  /* Deactivate spent zaps */
  for ( int i = 0; i < MAX_ZAPS; i++ )
  {
    if ( zaps[i].active && zaps[i].progress >= 1.0f && zaps[i].alpha < 1.0f )
      zaps[i].active = 0;
  }

  /* Deactivate spent flashes */
  for ( int i = 0; i < MAX_FLASHES; i++ )
  {
    if ( flashes[i].active && flashes[i].alpha < 1.0f )
      flashes[i].active = 0;
  }

  /* Deactivate spent projs (safety - callbacks should set active=0) */
  for ( int i = 0; i < MAX_PROJS; i++ )
  {
    if ( projs[i].active && projs[i].progress >= 1.0f )
      projs[i].active = 0;
  }

  /* Increment zap seeds for frame-varying jitter */
  for ( int i = 0; i < MAX_ZAPS; i++ )
    if ( zaps[i].active ) zaps[i].seed++;
}

/* ---- Spark (lightning zap) ---- */

void SpellVFXSpark( float px, float py, float tx, float ty )
{
  int s = find_zap();
  if ( s < 0 ) return;

  SpellZap_t* z = &zaps[s];
  z->start_wx = px;
  z->start_wy = py;
  z->end_wx   = tx;
  z->end_wy   = ty;
  z->progress = 0.0f;
  z->alpha    = 255.0f;
  z->seed     = rand();
  z->active   = 1;

  /* Bolt extends over 0.12s, callback triggers impact */
  TweenFloatWithCallback( &spell_tweens, &z->progress, 1.0f, 0.12f,
                           TWEEN_LINEAR, spark_arrive_cb, z );
}

/* ---- Frost (diamond projectile) ---- */

void SpellVFXFrost( float px, float py, float tx, float ty )
{
  int s = find_proj();
  if ( s < 0 ) return;

  SpellProj_t* p = &projs[s];
  p->start_wx = px;
  p->start_wy = py;
  p->end_wx   = tx;
  p->end_wy   = ty;
  p->progress = 0.0f;
  p->color    = (aColor_t){ 0x64, 0xb4, 0xff, 220 };
  p->shape    = 0; /* diamond */
  p->active   = 1;

  TweenFloatWithCallback( &spell_tweens, &p->progress, 1.0f, 0.2f,
                           TWEEN_EASE_IN_QUAD, frost_arrive_cb, p );
}

/* ---- Fireball (projectile + AoE) ---- */

void SpellVFXFireball( float px, float py, int tgt_r, int tgt_c, int radius )
{
  int s = find_proj();
  if ( s < 0 ) return;

  SpellProj_t* p = &projs[s];
  float tw = world->tile_w;
  float th = world->tile_h;
  p->start_wx   = px;
  p->start_wy   = py;
  p->end_wx     = tgt_r * tw + tw / 2.0f;
  p->end_wy     = tgt_c * th + th / 2.0f;
  p->progress   = 0.0f;
  p->color      = (aColor_t){ 0xff, 0x64, 0x1e, 220 };
  p->shape      = 1; /* fireball */
  p->active     = 1;
  p->aoe_row    = tgt_r;
  p->aoe_col    = tgt_c;
  p->aoe_radius = radius;

  TweenFloatWithCallback( &spell_tweens, &p->progress, 1.0f, 0.25f,
                           TWEEN_EASE_IN_QUAD, fireball_arrive_cb, p );
}

/* ---- Swap (dual tile flash) ---- */

void SpellVFXSwap( float p1x, float p1y, float p2x, float p2y )
{
  /* Purple flashes at both positions */
  aColor_t purple = { 0xb4, 0x50, 0xff, 255 };
  spawn_flash( p1x, p1y, purple, 150.0f, 0.3f );
  spawn_flash( p2x, p2y, purple, 150.0f, 0.3f );

  /* Purple screen flash */
  spawn_screen_flash( purple, 40.0f, 0.25f );
}

/* ---- Drawing ---- */

void SpellVFXDraw( aRectf_t vp_rect, GameCamera_t* cam )
{
  /* 1. Tile flashes */
  for ( int i = 0; i < MAX_FLASHES; i++ )
  {
    if ( !flashes[i].active ) continue;
    TileFlash_t* f = &flashes[i];
    aColor_t c = f->color;
    c.a = (int)f->alpha;
    GV_DrawFilledRect( vp_rect, cam, f->wx, f->wy,
                       (float)world->tile_w, (float)world->tile_h, c );
  }

  /* 2. Zap lines (lightning bolt) */
  for ( int zi = 0; zi < MAX_ZAPS; zi++ )
  {
    if ( !zaps[zi].active ) continue;
    SpellZap_t* z = &zaps[zi];

    /* Compute current end point based on progress */
    float cur_ex = z->start_wx + ( z->end_wx - z->start_wx ) * z->progress;
    float cur_ey = z->start_wy + ( z->end_wy - z->start_wy ) * z->progress;

    /* Transform start and current-end to screen coords */
    float s_sx, s_sy, s_ex, s_ey;
    GV_WorldToScreen( vp_rect, cam, z->start_wx, z->start_wy, &s_sx, &s_sy );
    GV_WorldToScreen( vp_rect, cam, cur_ex, cur_ey, &s_ex, &s_ey );

    /* Direction and perpendicular vectors */
    float dx = s_ex - s_sx;
    float dy = s_ey - s_sy;
    float len = sqrtf( dx * dx + dy * dy );
    if ( len < 1.0f ) continue;

    float px = -dy / len;
    float py =  dx / len;

    /* Generate jagged segment endpoints */
    float pts_x[ZAP_SEGMENTS + 1];
    float pts_y[ZAP_SEGMENTS + 1];
    pts_x[0] = s_sx;  pts_y[0] = s_sy;
    pts_x[ZAP_SEGMENTS] = s_ex;  pts_y[ZAP_SEGMENTS] = s_ey;

    int seed = z->seed;
    for ( int j = 1; j < ZAP_SEGMENTS; j++ )
    {
      float t = (float)j / ZAP_SEGMENTS;
      seed = lcg_next( seed );
      float offset = ( ( seed % 9 ) - 4 ) * 1.5f;
      pts_x[j] = s_sx + dx * t + px * offset;
      pts_y[j] = s_sy + dy * t + py * offset;
    }

    int alpha = (int)z->alpha;

    /* Main bolt */
    aColor_t bolt_color = { 0xff, 0xff, 0x96, alpha };
    for ( int j = 0; j < ZAP_SEGMENTS; j++ )
      a_DrawLine( (int)pts_x[j], (int)pts_y[j],
                  (int)pts_x[j + 1], (int)pts_y[j + 1], bolt_color );

    /* Glow pass - slightly offset, half alpha */
    aColor_t glow_color = { 0xff, 0xff, 0x64, alpha / 2 };
    seed = z->seed + 9999;
    float g_pts_x[ZAP_SEGMENTS + 1];
    float g_pts_y[ZAP_SEGMENTS + 1];
    g_pts_x[0] = s_sx;  g_pts_y[0] = s_sy;
    g_pts_x[ZAP_SEGMENTS] = s_ex;  g_pts_y[ZAP_SEGMENTS] = s_ey;
    for ( int j = 1; j < ZAP_SEGMENTS; j++ )
    {
      float t = (float)j / ZAP_SEGMENTS;
      seed = lcg_next( seed );
      float offset = ( ( seed % 7 ) - 3 ) * 2.0f;
      g_pts_x[j] = s_sx + dx * t + px * offset;
      g_pts_y[j] = s_sy + dy * t + py * offset;
    }
    for ( int j = 0; j < ZAP_SEGMENTS; j++ )
      a_DrawLine( (int)g_pts_x[j], (int)g_pts_y[j],
                  (int)g_pts_x[j + 1], (int)g_pts_y[j + 1], glow_color );
  }

  /* 3. Projectiles (frost diamond, fireball) */
  for ( int i = 0; i < MAX_PROJS; i++ )
  {
    if ( !projs[i].active ) continue;
    SpellProj_t* p = &projs[i];

    float wx = p->start_wx + ( p->end_wx - p->start_wx ) * p->progress;
    float wy = p->start_wy + ( p->end_wy - p->start_wy ) * p->progress;

    float sx, sy;
    GV_WorldToScreen( vp_rect, cam, wx, wy, &sx, &sy );

    if ( p->shape == 0 )
    {
      /* Diamond (frost): 2 filled triangles */
      int r = 5;
      a_DrawFilledTriangle( (int)sx, (int)sy - r,
                            (int)sx + r - 1, (int)sy,
                            (int)sx, (int)sy + r, p->color );
      a_DrawFilledTriangle( (int)sx, (int)sy - r,
                            (int)sx - r + 1, (int)sy,
                            (int)sx, (int)sy + r, p->color );
    }
    else if ( p->shape == 1 )
    {
      /* Fireball: core rect + outer glow + trail */

      /* Trail: 2 fading rects behind */
      for ( int t = 2; t >= 1; t-- )
      {
        float tp = p->progress - t * 0.08f;
        if ( tp < 0 ) continue;
        float twx = p->start_wx + ( p->end_wx - p->start_wx ) * tp;
        float twy = p->start_wy + ( p->end_wy - p->start_wy ) * tp;
        float tsx, tsy;
        GV_WorldToScreen( vp_rect, cam, twx, twy, &tsx, &tsy );

        int trail_a = ( t == 2 ) ? 60 : 100;
        int trail_sz = ( t == 2 ) ? 2 : 3;
        aColor_t tc = p->color;
        tc.a = trail_a;
        a_DrawFilledRect( (aRectf_t){ tsx - trail_sz, tsy - trail_sz,
                                      trail_sz * 2, trail_sz * 2 }, tc );
      }

      /* Outer glow */
      aColor_t glow = p->color;
      glow.a = 100;
      a_DrawFilledRect( (aRectf_t){ sx - 4, sy - 4, 8, 8 }, glow );

      /* Core */
      a_DrawFilledRect( (aRectf_t){ sx - 2, sy - 2, 5, 5 }, p->color );
    }
  }
}

int SpellVFXActive( void )
{
  for ( int i = 0; i < MAX_PROJS; i++ )
    if ( projs[i].active ) return 1;
  for ( int i = 0; i < MAX_ZAPS; i++ )
    if ( zaps[i].active ) return 1;
  for ( int i = 0; i < MAX_FLASHES; i++ )
    if ( flashes[i].active ) return 1;
  if ( screen_flash_alpha > 1.0f ) return 1;
  return 0;
}

float    SpellVFXShakeOX( void ) { return shake_x; }
float    SpellVFXShakeOY( void ) { return shake_y; }
float    SpellVFXFlashAlpha( void ) { return screen_flash_alpha; }
aColor_t SpellVFXFlashColor( void ) { return screen_flash_color; }
