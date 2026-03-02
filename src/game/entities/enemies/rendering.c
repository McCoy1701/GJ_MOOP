#include <string.h>
#include <math.h>
#include <Archimedes.h>

#include "defines.h"
#include "enemies.h"
#include "combat_vfx.h"
#include "game_viewport.h"
#include "world.h"
#include "tween.h"

/* ---- Enemy sprite drawing ---- */

void EnemiesDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                     Enemy_t* list, int count,
                     World_t* world, int gfx_mode )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;
    EnemyType_t* et = &g_enemy_types[list[i].type_idx];

    float shadow_oy = ( strcmp( et->ai, "ranged_telegraph" ) == 0 ) ? 8.0f :
                       ( gfx_mode == GFX_IMAGE ) ? 6.0f : 7.0f;
    GV_DrawFilledRect( vp_rect, cam,
                       list[i].world_x, list[i].world_y + shadow_oy,
                       10.0f, 3.0f,
                       (aColor_t){ 0, 0, 0, 80 } );

    if ( et->image && gfx_mode == GFX_IMAGE )
    {
      if ( list[i].facing_left )
        GV_DrawSpriteFlipped( vp_rect, cam, et->image,
                              list[i].world_x, list[i].world_y,
                              (float)world->tile_w, (float)world->tile_h, 'x' );
      else
        GV_DrawSprite( vp_rect, cam, et->image,
                       list[i].world_x, list[i].world_y,
                       (float)world->tile_w, (float)world->tile_h );
    }
    else
    {
      /* Glyph fallback - stretch to fill tile */
      float sx, sy;
      GV_WorldToScreen( vp_rect, cam,
                        list[i].world_x - world->tile_w / 2.0f,
                        list[i].world_y - world->tile_h / 2.0f,
                        &sx, &sy );
      float half_w = cam->half_h * ( vp_rect.w / vp_rect.h );
      int dw = (int)( world->tile_w * ( vp_rect.w / ( half_w * 2.0f ) ) );
      int dh = (int)( world->tile_h * ( vp_rect.h / ( cam->half_h * 2.0f ) ) );

      a_DrawGlyph( et->glyph, (int)sx, (int)sy, dw, dh,
                   et->color, (aColor_t){ 0, 0, 0, 0 }, FONT_CODE_PAGE_437 );
    }
  }
}

/* ---- Telegraph dashed line ---- */

void EnemiesDrawTelegraph( aRectf_t vp_rect, GameCamera_t* cam,
                           Enemy_t* list, int count,
                           World_t* world )
{
  aColor_t col = { 0xcf, 0x57, 0x3c, 160 };
  int dash = 4, gap = 3;

  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;
    EnemyType_t* et = &g_enemy_types[list[i].type_idx];
    if ( strcmp( et->ai, "ranged_telegraph" ) != 0 ) continue;
    if ( list[i].ai_state != 1 ) continue;

    int tw = world->tile_w;
    int th = world->tile_h;

    /* Start: center of skeleton tile */
    float swx = list[i].row * tw + tw / 2.0f;
    float swy = list[i].col * th + th / 2.0f;

    /* Walk firing line to find end tile */
    int cr = list[i].row, cc = list[i].col;
    int end_r = cr, end_c = cc;
    for ( int step = 0; step < et->range; step++ )
    {
      cr += list[i].ai_dir_row;
      cc += list[i].ai_dir_col;
      if ( cr < 0 || cr >= world->width || cc < 0 || cc >= world->height )
        break;
      int idx = cc * world->width + cr;
      if ( world->background[idx].solid || world->midground[idx].solid )
        break;
      end_r = cr;
      end_c = cc;
    }

    if ( end_r == list[i].row && end_c == list[i].col )
      continue;

    float ewx = end_r * tw + tw / 2.0f;
    float ewy = end_c * th + th / 2.0f;

    /* Convert to screen */
    float sx1, sy1, sx2, sy2;
    GV_WorldToScreen( vp_rect, cam, swx, swy, &sx1, &sy1 );
    GV_WorldToScreen( vp_rect, cam, ewx, ewy, &sx2, &sy2 );

    /* Draw dashed line */
    float dx = sx2 - sx1;
    float dy = sy2 - sy1;
    float len = sqrtf( dx * dx + dy * dy );
    if ( len < 1.0f ) continue;

    float nx = dx / len;
    float ny = dy / len;
    float pos = 0;

    while ( pos < len )
    {
      float seg_end = pos + dash;
      if ( seg_end > len ) seg_end = len;

      a_DrawLine( (int)( sx1 + nx * pos ),     (int)( sy1 + ny * pos ),
                  (int)( sx1 + nx * seg_end ), (int)( sy1 + ny * seg_end ),
                  col );
      pos += dash + gap;
    }
  }
}

/* ---- Arrow projectile VFX ---- */

#define MAX_PROJECTILES 8

typedef struct
{
  float start_wx, start_wy;
  float end_wx, end_wy;
  float progress;            /* 0 → 1 tweened */
  int   dir_row, dir_col;
  int   active;
} Projectile_t;

static Projectile_t projectiles[MAX_PROJECTILES];
static TweenManager_t proj_tweens;

void EnemyProjectileInit( void )
{
  InitTweenManager( &proj_tweens );
  for ( int i = 0; i < MAX_PROJECTILES; i++ )
    projectiles[i].active = 0;
}

void EnemyProjectileUpdate( float dt )
{
  UpdateTweens( &proj_tweens, dt );

  for ( int i = 0; i < MAX_PROJECTILES; i++ )
  {
    if ( projectiles[i].active && projectiles[i].progress >= 1.0f )
      projectiles[i].active = 0;
  }
}

void EnemyProjectileSpawn( float sx, float sy, float ex, float ey,
                           int dir_row, int dir_col )
{
  int slot = -1;
  for ( int i = 0; i < MAX_PROJECTILES; i++ )
    if ( !projectiles[i].active ) { slot = i; break; }
  if ( slot < 0 ) return;

  Projectile_t* p = &projectiles[slot];
  p->start_wx = sx;
  p->start_wy = sy;
  p->end_wx   = ex;
  p->end_wy   = ey;
  p->dir_row  = dir_row;
  p->dir_col  = dir_col;
  p->progress = 0.0f;
  p->active   = 1;

  TweenFloat( &proj_tweens, &p->progress, 1.0f, 0.25f, TWEEN_LINEAR );
}

int EnemyProjectileInFlight( void )
{
  for ( int i = 0; i < MAX_PROJECTILES; i++ )
    if ( projectiles[i].active ) return 1;
  return 0;
}

void EnemyProjectileDraw( aRectf_t vp_rect, GameCamera_t* cam )
{
  for ( int i = 0; i < MAX_PROJECTILES; i++ )
  {
    if ( !projectiles[i].active ) continue;
    Projectile_t* p = &projectiles[i];

    float t = p->progress;
    float wx = p->start_wx + ( p->end_wx - p->start_wx ) * t;
    float wy = p->start_wy + ( p->end_wy - p->start_wy ) * t;

    float sx, sy;
    GV_WorldToScreen( vp_rect, cam, wx, wy, &sx, &sy );

    /* Arrowhead triangle pointing in firing direction */
    int sz = 4;
    int x0, y0, x1, y1, x2, y2;

    if ( p->dir_row == 1 ) /* right */
    {
      x0 = (int)sx + sz; y0 = (int)sy;
      x1 = (int)sx - sz; y1 = (int)sy - sz;
      x2 = (int)sx - sz; y2 = (int)sy + sz;
    }
    else if ( p->dir_row == -1 ) /* left */
    {
      x0 = (int)sx - sz; y0 = (int)sy;
      x1 = (int)sx + sz; y1 = (int)sy - sz;
      x2 = (int)sx + sz; y2 = (int)sy + sz;
    }
    else if ( p->dir_col == 1 ) /* down */
    {
      x0 = (int)sx;      y0 = (int)sy + sz;
      x1 = (int)sx - sz; y1 = (int)sy - sz;
      x2 = (int)sx + sz; y2 = (int)sy - sz;
    }
    else /* up */
    {
      x0 = (int)sx;      y0 = (int)sy - sz;
      x1 = (int)sx - sz; y1 = (int)sy + sz;
      x2 = (int)sx + sz; y2 = (int)sy + sz;
    }

    a_DrawFilledTriangle( x0, y0, x1, y1, x2, y2,
                          (aColor_t){ 0xcf, 0x57, 0x3c, 220 } );
  }
}
