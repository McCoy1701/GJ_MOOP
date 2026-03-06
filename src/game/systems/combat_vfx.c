#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "combat_vfx.h"
#include "tween.h"

/* ---- World-to-screen helper (same math as game_viewport.c) ---- */

static void vfx_transform( aRectf_t rect, GameCamera_t* cam,
                            float* sx, float* sy,
                            float* cam_left, float* cam_top )
{
  float aspect = rect.w / rect.h;
  float cam_w  = cam->half_h * aspect;
  *sx = rect.w / ( cam_w * 2.0f );
  *sy = rect.h / ( cam->half_h * 2.0f );
  *cam_left = cam->x - cam_w;
  *cam_top  = cam->y - cam->half_h;
}

/* ---- Floating damage numbers ---- */

#define MAX_MARKERS 16

typedef struct
{
  float    wx, wy;     /* world position (base) */
  float    oy;         /* vertical offset (tweened upward) */
  float    alpha;      /* 255 → 0 fade */
  int      amount;
  aColor_t color;
  float    scale;      /* text scale (default 1.5) */
  int      active;
} HitMarker_t;

static HitMarker_t markers[MAX_MARKERS];
static TweenManager_t vfx_tweens;

/* ---- Floating text bubbles ---- */

#define MAX_TEXT_MARKERS 4

typedef struct
{
  float    wx, wy;
  float    oy;
  float    alpha;
  char     text[64];
  aColor_t color;
  int      active;
} TextMarker_t;

static TextMarker_t text_markers[MAX_TEXT_MARKERS];

void CombatVFXInit( void )
{
  InitTweenManager( &vfx_tweens );
  for ( int i = 0; i < MAX_MARKERS; i++ )
    markers[i].active = 0;
  for ( int i = 0; i < MAX_TEXT_MARKERS; i++ )
    text_markers[i].active = 0;
}

void CombatVFXUpdate( float dt )
{
  UpdateTweens( &vfx_tweens, dt );

  /* Deactivate markers that have fully faded */
  for ( int i = 0; i < MAX_MARKERS; i++ )
  {
    if ( markers[i].active && markers[i].alpha < 1.0f )
      markers[i].active = 0;
  }
  for ( int i = 0; i < MAX_TEXT_MARKERS; i++ )
  {
    if ( text_markers[i].active && text_markers[i].alpha < 1.0f )
      text_markers[i].active = 0;
  }
}

static void spawn_number( float wx, float wy, int amount, aColor_t color, float scale )
{
  int slot = -1;
  for ( int i = 0; i < MAX_MARKERS; i++ )
  {
    if ( !markers[i].active ) { slot = i; break; }
  }
  if ( slot < 0 ) return;

  HitMarker_t* m = &markers[slot];
  m->wx     = wx;
  m->wy     = wy;
  m->oy     = 0;
  m->alpha  = 255.0f;
  m->amount = amount;
  m->color  = color;
  m->scale  = scale;
  m->active = 1;

  TweenFloat( &vfx_tweens, &m->oy, -10.0f, 0.6f, TWEEN_EASE_OUT_CUBIC );
  TweenFloat( &vfx_tweens, &m->alpha, 0.0f, 0.6f, TWEEN_EASE_IN_QUAD );
}

void CombatVFXSpawnNumber( float wx, float wy, int amount, aColor_t color )
{
  spawn_number( wx, wy, amount, color, 1.5f );
}

void CombatVFXSpawnNumberScaled( float wx, float wy, int amount, aColor_t color, float scale )
{
  spawn_number( wx, wy, amount, color, scale );
}

void CombatVFXSpawnText( float wx, float wy, const char* text, aColor_t color )
{
  int slot = -1;
  for ( int i = 0; i < MAX_TEXT_MARKERS; i++ )
    if ( !text_markers[i].active ) { slot = i; break; }
  if ( slot < 0 ) return;

  TextMarker_t* m = &text_markers[slot];
  m->wx    = wx;
  m->wy    = wy - 10.0f;   /* start above the sprite's head */
  m->oy    = 0;
  m->alpha = 255.0f;
  strncpy( m->text, text, 63 );
  m->text[63] = '\0';
  m->color  = color;
  m->active = 1;

  /* Gentle drift up, then fade out after a readable pause */
  TweenFloat( &vfx_tweens, &m->oy, -5.0f, 1.2f, TWEEN_EASE_OUT_QUAD );
  TweenFloat( &vfx_tweens, &m->alpha, 0.0f, 1.2f, TWEEN_EASE_IN_CUBIC );
}

void CombatVFXDraw( aRectf_t vp_rect, GameCamera_t* cam )
{
  float sx, sy, cl, ct;
  vfx_transform( vp_rect, cam, &sx, &sy, &cl, &ct );

  for ( int i = 0; i < MAX_MARKERS; i++ )
  {
    if ( !markers[i].active ) continue;
    HitMarker_t* m = &markers[i];

    float screen_x = ( m->wx - cl ) * sx + vp_rect.x;
    float screen_y = ( m->wy + m->oy - ct ) * sy + vp_rect.y;

    char buf[16];
    snprintf( buf, sizeof( buf ), "%d", m->amount );

    aColor_t c = m->color;
    c.a = (int)m->alpha;

    aTextStyle_t ts = a_default_text_style;
    ts.fg    = c;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = m->scale;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( buf, (int)screen_x, (int)screen_y, ts );
  }

  /* Floating text bubbles */
  for ( int i = 0; i < MAX_TEXT_MARKERS; i++ )
  {
    if ( !text_markers[i].active ) continue;
    TextMarker_t* m = &text_markers[i];

    float screen_x = ( m->wx - cl ) * sx + vp_rect.x;
    float screen_y = ( m->wy + m->oy - ct ) * sy + vp_rect.y;

    aColor_t c = m->color;
    c.a = (int)m->alpha;

    aTextStyle_t ts = a_default_text_style;
    ts.fg    = c;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.2f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( m->text, (int)screen_x, (int)screen_y, ts );
  }
}

/* ---- Health bars ---- */

void CombatVFXDrawHealthBar( aRectf_t vp_rect, GameCamera_t* cam,
                              float wx, float wy,
                              int hp, int max_hp )
{
  if ( hp >= max_hp ) return; /* full health - no bar */
  if ( hp < 0 ) hp = 0;

  float sx, sy, cl, ct;
  vfx_transform( vp_rect, cam, &sx, &sy, &cl, &ct );

  /* Bar dimensions in world units */
  float bar_w_world = 10.0f;
  float bar_h_world = 1.5f;
  float bar_offset  = -8.0f; /* above the sprite */

  /* Convert to screen */
  float bx = ( wx - bar_w_world / 2.0f - cl ) * sx + vp_rect.x;
  float by = ( wy + bar_offset - bar_h_world / 2.0f - ct ) * sy + vp_rect.y;
  float bw = bar_w_world * sx;
  float bh = bar_h_world * sy;

  /* Background */
  a_DrawFilledRect( (aRectf_t){ bx, by, bw, bh },
                    (aColor_t){ 0x09, 0x0a, 0x14, 200 } );

  /* Fill */
  float pct = (float)hp / (float)max_hp;
  if ( pct < 0 ) pct = 0;
  if ( pct > 1 ) pct = 1;

  aColor_t fill;
  if ( pct > 0.5f )
    fill = (aColor_t){ 0x75, 0xa7, 0x43, 220 };  /* palette green */
  else if ( pct > 0.25f )
    fill = (aColor_t){ 0xe8, 0xc1, 0x70, 220 };  /* palette gold */
  else
    fill = (aColor_t){ 0xa5, 0x30, 0x30, 220 };  /* palette red */

  /* Bar outline */
  a_DrawRect( (aRectf_t){ bx, by, bw, bh },
              (aColor_t){ 0x39, 0x4a, 0x50, 200 } );

  a_DrawFilledRect( (aRectf_t){ bx + 1, by + 1, ( bw - 2 ) * pct, bh - 2 },
                    fill );
}
