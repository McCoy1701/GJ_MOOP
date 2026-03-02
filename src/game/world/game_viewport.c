#include <stdlib.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "world.h"
#include "game_viewport.h"
#include "visibility.h"

#define GV_ZOOM_STEP 0.9f  /* multiplier per scroll tick (< 1 = zoom in) */

/* Compute scale factors and camera edges for world→screen transform */
static void gv_transform( aRectf_t rect, GameCamera_t* cam,
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

void GV_DrawWorld( aRectf_t rect, GameCamera_t* cam,
                   World_t* world, aTileset_t* tileset,
                   uint8_t draw_ascii )
{
  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  for ( int i = 0; i < world->tile_count; i++ )
  {
    int x = i % world->width;
    int y = i / world->width;

    /* Tile top-left in world coords */
    float wx = x * world->tile_w;
    float wy = y * world->tile_h;

    /* Map to screen */
    float dx = ( wx - cl ) * sx + rect.x;
    float dy = ( wy - ct ) * sy + rect.y;
    float dw = world->tile_w * sx;
    float dh = world->tile_h * sy;

    Tile_t bg = world->background[i];
    Tile_t mg = world->midground[i];
    Tile_t fg = world->foreground[i];

    if ( bg.tile == TILE_EMPTY )
    {
      d_LogFatalF( "[GV_DrawWorld] background tile %d has TILE_EMPTY - "
                   "background must always have a valid tile index", i );
      exit( 1 );
    }

    int has_mg = ( mg.tile != TILE_EMPTY );
    int has_fg = ( fg.tile != TILE_EMPTY );

    if ( draw_ascii )
    {
      /* Snap to pixel grid like image mode */
      int nx = (int)dx;
      int ny = (int)dy;
      int nw = (int)( dx + dw + 0.5f ) - (int)dx;
      int nh = (int)( dy + dh + 0.5f ) - (int)dy;

      a_DrawGlyph( bg.glyph, nx, ny, nw, nh,
                   bg.glyph_fg, bg.glyph_bg, FONT_CODE_PAGE_437 );

      if ( has_mg && mg.glyph[0] != '\0' )
        a_DrawGlyph( mg.glyph, nx, ny, nw, nh,
                     mg.glyph_fg, mg.glyph_bg, FONT_CODE_PAGE_437 );

      if ( has_fg && fg.glyph[0] != '\0' )
        a_DrawGlyph( fg.glyph, nx, ny, nw, nh,
                     fg.glyph_fg, fg.glyph_bg, FONT_CODE_PAGE_437 );
    }
    else
    {
      /* Snap to pixel grid to prevent sub-pixel gaps between tiles */
      float nx = (int)dx;
      float ny = (int)dy;
      float nw = (int)( dx + dw + 0.5f ) - (int)dx;
      float nh = (int)( dy + dh + 0.5f ) - (int)dy;
      aRectf_t dst = { nx, ny, nw, nh };
      a_BlitRect( tileset[bg.tile].img, NULL, &dst, 1.0f );
      if ( has_mg )
        a_BlitRect( tileset[mg.tile].img, NULL, &dst, 1.0f );
      if ( has_fg )
        a_BlitRect( tileset[fg.tile].img, NULL, &dst, 1.0f );
    }
  }
}

void GV_DrawDarkness( aRectf_t rect, GameCamera_t* cam, World_t* world,
                      float fade )
{
  /* fade: 0 = everything black (intro start), 1 = normal darkness only */
  int floor_a = (int)( ( 1.0f - fade ) * 255 );

  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  for ( int i = 0; i < world->tile_count; i++ )
  {
    int x = i % world->width;
    int y = i / world->width;

    float v = VisibilityGet( x, y );
    int vis_a = (int)( ( 1.0f - v ) * 255 );
    int alpha = vis_a > floor_a ? vis_a : floor_a;
    if ( alpha < 1 ) continue;
    if ( alpha > 255 ) alpha = 255;

    float wx = x * world->tile_w;
    float wy = y * world->tile_h;

    float dx = ( wx - cl ) * sx + rect.x;
    float dy = ( wy - ct ) * sy + rect.y;
    float dw = world->tile_w * sx;
    float dh = world->tile_h * sy;

    /* Snap to pixel grid like GV_DrawWorld */
    float nx = (int)dx;
    float ny = (int)dy;
    float nw = (int)( dx + dw + 0.5f ) - (int)dx;
    float nh = (int)( dy + dh + 0.5f ) - (int)dy;

    a_DrawFilledRect( (aRectf_t){ nx, ny, nw, nh },
                      (aColor_t){ 0, 0, 0, alpha } );
  }
}

void GV_DrawSprite( aRectf_t rect, GameCamera_t* cam,
                    aImage_t* img,
                    float wx, float wy, float ww, float wh )
{
  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  /* Center sprite on (wx, wy) */
  float dx = ( wx - ww / 2.0f - cl ) * sx + rect.x;
  float dy = ( wy - wh / 2.0f - ct ) * sy + rect.y;
  float dw = ww * sx;
  float dh = wh * sy;

  aRectf_t dst = { dx, dy, dw, dh };
  a_BlitRect( img, NULL, &dst, 1.0f );
}

void GV_DrawSpriteFlipped( aRectf_t rect, GameCamera_t* cam,
                           aImage_t* img,
                           float wx, float wy, float ww, float wh,
                           char axis )
{
  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  float dx = ( wx - ww / 2.0f - cl ) * sx + rect.x;
  float dy = ( wy - wh / 2.0f - ct ) * sy + rect.y;
  float dw = ww * sx;

  float scale = dw / img->rect.w;
  a_BlitRectFlipped( img, NULL,
                     &(aRectf_t){ dx, dy, img->rect.w, img->rect.h },
                     scale, axis );
}

void GV_DrawFilledRect( aRectf_t rect, GameCamera_t* cam,
                        float wx, float wy, float ww, float wh,
                        aColor_t color )
{
  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  float dx = ( wx - ww / 2.0f - cl ) * sx + rect.x;
  float dy = ( wy - wh / 2.0f - ct ) * sy + rect.y;
  float dw = ww * sx;
  float dh = wh * sy;

  a_DrawFilledRect( (aRectf_t){ dx, dy, dw, dh }, color );
}

void GV_ScreenToWorld( aRectf_t rect, GameCamera_t* cam,
                       int screen_x, int screen_y,
                       float* world_x, float* world_y )
{
  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  *world_x = ( screen_x - rect.x ) / sx + cl;
  *world_y = ( screen_y - rect.y ) / sy + ct;
}

void GV_WorldToScreen( aRectf_t rect, GameCamera_t* cam,
                       float wx, float wy,
                       float* screen_x, float* screen_y )
{
  float s_x, s_y, cl, ct;
  gv_transform( rect, cam, &s_x, &s_y, &cl, &ct );
  *screen_x = ( wx - cl ) * s_x + rect.x;
  *screen_y = ( wy - ct ) * s_y + rect.y;
}

void GV_DrawTileOutline( aRectf_t rect, GameCamera_t* cam,
                         int tile_x, int tile_y,
                         int tile_w, int tile_h, aColor_t color )
{
  float sx, sy, cl, ct;
  gv_transform( rect, cam, &sx, &sy, &cl, &ct );

  float wx = tile_x * tile_w;
  float wy = tile_y * tile_h;

  float dx = ( wx - cl ) * sx + rect.x;
  float dy = ( wy - ct ) * sy + rect.y;
  float dw = tile_w * sx;
  float dh = tile_h * sy;

  a_DrawRect( (aRectf_t){ dx, dy, dw, dh }, color );
}

void GV_Zoom( GameCamera_t* cam, int direction,
              float min_h, float max_h )
{
  if ( direction > 0 )
    cam->half_h *= GV_ZOOM_STEP;
  else if ( direction < 0 )
    cam->half_h /= GV_ZOOM_STEP;

  if ( cam->half_h < min_h ) cam->half_h = min_h;
  if ( cam->half_h > max_h ) cam->half_h = max_h;
}
