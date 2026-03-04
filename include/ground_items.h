#ifndef __GROUND_ITEMS_H__
#define __GROUND_ITEMS_H__

#include <Archimedes.h>
#include "world.h"
#include "game_viewport.h"

#define MAX_GROUND_ITEMS 64

#define GROUND_CONSUMABLE 0
#define GROUND_MAP        1
#define GROUND_EQUIPMENT  2

typedef struct
{
  int   item_type;         /* GROUND_CONSUMABLE or GROUND_MAP */
  int   item_idx;          /* index into g_consumables[] or g_maps[] */
  int   row, col;
  float world_x, world_y;
  int   alive;             /* 1 = on ground, 0 = picked up */
} GroundItem_t;

void          GroundItemsInit( GroundItem_t* list, int* count );
GroundItem_t* GroundItemSpawn( GroundItem_t* list, int* count,
                               int consumable_idx, int row, int col,
                               int tile_w, int tile_h );
GroundItem_t* GroundItemSpawnMap( GroundItem_t* list, int* count,
                                  int map_idx, int row, int col,
                                  int tile_w, int tile_h );
GroundItem_t* GroundItemSpawnEquipment( GroundItem_t* list, int* count,
                                        int equip_idx, int row, int col,
                                        int tile_w, int tile_h );
GroundItem_t* GroundItemAt( GroundItem_t* list, int count, int row, int col );

void GroundItemsDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                          GroundItem_t* list, int count,
                          World_t* world, int gfx_mode );

#endif
