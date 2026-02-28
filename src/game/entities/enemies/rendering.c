#include <Archimedes.h>

#include "defines.h"
#include "enemies.h"
#include "game_viewport.h"
#include "world.h"

void EnemiesDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                     Enemy_t* list, int count,
                     World_t* world, int gfx_mode )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( !list[i].alive ) continue;
    EnemyType_t* et = &g_enemy_types[list[i].type_idx];

    /* Shadow */
    GV_DrawFilledRect( vp_rect, cam,
                       list[i].world_x, list[i].world_y + 6.0f,
                       10.0f, 3.0f,
                       (aColor_t){ 0, 0, 0, 80 } );

    if ( et->image && gfx_mode == GFX_IMAGE )
      GV_DrawSprite( vp_rect, cam, et->image,
                     list[i].world_x, list[i].world_y,
                     (float)world->tile_w, (float)world->tile_h );
    else
      GV_DrawFilledRect( vp_rect, cam,
                         list[i].world_x, list[i].world_y,
                         (float)world->tile_w, (float)world->tile_h,
                         et->color );
  }
}
