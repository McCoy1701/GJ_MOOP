#include <Archimedes.h>

#include "world.h"
#include "game_viewport.h"

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
    int has_mg = ( mg.tile != TILE_EMPTY );
    int has_fg = ( fg.tile != TILE_EMPTY );

    if ( draw_ascii )
    {
      aRectf_t gs = a_GetGlyphSize();
      float scale = dh / gs.h;
      if ( scale < 0.1f ) scale = 0.1f;

      aTextStyle_t ts = {
        .type      = FONT_CODE_PAGE_437,
        .align     = TEXT_ALIGN_CENTER,
        .wrap_width = 0,
        .scale     = scale,
        .padding   = 0
      };

      ts.fg = bg.glyph_fg;
      ts.bg = bg.glyph_bg;
      a_DrawText( bg.glyph, (int)dx, (int)dy, ts );

      if ( has_mg && mg.glyph[0] != '\0' )
      {
        ts.fg = mg.glyph_fg;
        ts.bg = mg.glyph_bg;
        a_DrawText( mg.glyph, (int)dx, (int)dy, ts );
      }

      if ( has_fg && fg.glyph[0] != '\0' )
      {
        ts.fg = fg.glyph_fg;
        ts.bg = fg.glyph_bg;
        a_DrawText( fg.glyph, (int)dx, (int)dy, ts );
      }
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
