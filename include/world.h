#ifndef __WORLD_H__
#define __WORLD_H__

#include <Archimedes.h>

#define TILE_EMPTY ((uint32_t)-1)

typedef struct
{
  uint32_t tile;
  char* glyph;
  aColor_t glyph_fg;
  aColor_t glyph_bg;
  uint8_t solid;
  Ground_t base;
} Tile_t;

typedef struct
{
  Tile_t* background;
  Tile_t* midground;
  Tile_t* foreground;
  int tile_count;
  int tile_w, tile_h;
  int width, height;
} World_t;

World_t* WorldCreate( int width, int height, int tile_w, int tile_h );
void WorldDraw( int x_off, int y_off,
                World_t* world, aTileset_t* tile_set,
                uint8_t draw_ascii );

#endif

