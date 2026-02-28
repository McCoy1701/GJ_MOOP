#ifndef __GAME_VIEWPORT_H__
#define __GAME_VIEWPORT_H__

#include <Archimedes.h>
#include "world.h"

typedef struct
{
  float x, y;     /* camera center in world coords */
  float half_h;   /* half-height in world units (zoom level) */
} GameCamera_t;

/* Draw all world layers into the widget rect */
void GV_DrawWorld( aRectf_t rect, GameCamera_t* cam,
                   World_t* world, aTileset_t* tileset,
                   uint8_t draw_ascii );

/* Draw a sprite centered at (wx, wy) with given world size */
void GV_DrawSprite( aRectf_t rect, GameCamera_t* cam,
                    aImage_t* img,
                    float wx, float wy, float ww, float wh );

/* Draw a sprite flipped on the given axis ('x' for horizontal) */
void GV_DrawSpriteFlipped( aRectf_t rect, GameCamera_t* cam,
                           aImage_t* img,
                           float wx, float wy, float ww, float wh,
                           char axis );

/* Draw a filled rect centered at (wx, wy) with given world size */
void GV_DrawFilledRect( aRectf_t rect, GameCamera_t* cam,
                        float wx, float wy, float ww, float wh,
                        aColor_t color );

/* Convert screen coordinates to world coordinates */
void GV_ScreenToWorld( aRectf_t rect, GameCamera_t* cam,
                       int screen_x, int screen_y,
                       float* world_x, float* world_y );

/* Draw an outline around a tile at grid position (x, y) */
void GV_DrawTileOutline( aRectf_t rect, GameCamera_t* cam,
                         int tile_x, int tile_y,
                         int tile_w, int tile_h, aColor_t color );

/* Adjust zoom: direction > 0 = zoom in, < 0 = zoom out */
void GV_Zoom( GameCamera_t* cam, int direction,
              float min_h, float max_h );

#endif
