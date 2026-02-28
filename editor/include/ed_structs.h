/*
 * ed_structs.h:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#ifndef __EDITOR_STRUCTS_H__
#define __EDITOR_STRUCTS_H__

#include <Archimedes.h>

typedef enum
{
  OBJ_DOOR,
  OBJ_CHEST,
  OBJ_TRAP
} ObjectType_t;

typedef struct
{
  ObjectType_t type;
  int x, y;
  int state;
  uint8_t key_id;
} ActiveObject_t;

typedef struct
{
  uint16_t id;
  uint8_t visitor_mask;
  ActiveObject_t* room_obj;
  int num_obj;
} Room_t;

typedef struct
{
  SDL_Rect rects[MAX_GLYPHS];
  SDL_Texture* texture;
  int count;
} GlyphArray_t;

typedef struct
{
  aSpriteSheet_t* sheet;
} TileArray_t;

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

  uint16_t* room_ids;

  int tile_count;
  int tile_w, tile_h;
  int rows, cols;
} World_t;

enum
{
  APOLLO_PALETE = 0
};

enum
{
  APOLLO_WHITE,
  APOLLO_BLACK,
  APOLLO_DARK_GRAY_0,
  APOLLO_DARK_GRAY_1,
  APOLLO_DARK_GRAY_2,
  APOLLO_DARK_GRAY_3,
  APOLLO_LIGHT_GRAY_0,
  APOLLO_LIGHT_GRAY_1,
  APOLLO_LIGHT_GRAY_2,
  APOLLO_LIGHT_GRAY_3,
  APOLLO_LIGHT_GRAY_4,
  APOLLO_LIGHT_GRAY_5,
  APOLLO_BLUE_0,
  APOLLO_BLUE_1,
  APOLLO_BLUE_2,
  APOLLO_BLUE_3,
  APOLLO_BLUE_4,
  APOLLO_BLUE_5,
  APOLLO_GREEN_0,
  APOLLO_GREEN_1,
  APOLLO_GREEN_2,
  APOLLO_GREEN_3,
  APOLLO_GREEN_4,
  APOLLO_GREEN_5,
  APOLLO_TAN_0,
  APOLLO_TAN_1,
  APOLLO_TAN_2,
  APOLLO_TAN_3,
  APOLLO_TAN_4,
  APOLLO_TAN_5,
  APOLLO_ORANGE_0,
  APOLLO_ORANGE_1,
  APOLLO_ORANGE_2,
  APOLLO_ORANGE_3,
  APOLLO_ORANGE_4,
  APOLLO_ORANGE_5,
  APOLLO_RED_0,
  APOLLO_RED_1,
  APOLLO_RED_2,
  APOLLO_RED_3,
  APOLLO_RED_4,
  APOLLO_RED_5,
  APOLLO_PURPLE_0,
  APOLLO_PURPLE_1,
  APOLLO_PURPLE_2,
  APOLLO_PURPLE_3,
  APOLLO_PURPLE_4,
  APOLLO_PURPLE_5,
  APOLLO_MAX
};

#endif

