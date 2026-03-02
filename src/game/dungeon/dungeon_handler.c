#include <Archimedes.h>

#include "defines.h"
#include "dungeon.h"
#include "visibility.h"

#define EASEL_COL 22
#define EASEL_ROW  4

#define CHAIR_COL 12
#define CHAIR_ROW  5

static aImage_t* easel_image = NULL;
static float easel_wx = 0;
static float easel_wy = 0;

static aImage_t* chair_image = NULL;
static float chair_wx = 0;
static float chair_wy = 0;

void DungeonHandlerInit( World_t* world )
{
  easel_image = a_ImageLoad( "resources/assets/objects/jonathon-easel.png" );
  easel_wx = EASEL_COL * world->tile_w + world->tile_w / 2.0f;
  easel_wy = EASEL_ROW * world->tile_h + world->tile_h / 2.0f;

  chair_image = a_ImageLoad( "resources/assets/objects/grishnak-chair.png" );
  chair_wx = CHAIR_COL * world->tile_w + world->tile_w / 2.0f;
  chair_wy = CHAIR_ROW * world->tile_h + world->tile_h / 2.0f;
}

void DungeonDrawProps( aRectf_t vp_rect, GameCamera_t* cam,
                       World_t* world, int gfx_mode )
{
  /* Easel - floor 1 only */
  if ( g_current_floor == 1 )
  {
    if ( VisibilityGet( EASEL_COL, EASEL_ROW ) >= 0.01f
         && easel_image && gfx_mode == GFX_IMAGE )
    {
      GV_DrawSprite( vp_rect, cam, easel_image,
                     easel_wx, easel_wy,
                     (float)world->tile_w, (float)world->tile_h );
    }
  }

  /* Chair - floor 2 only */
  if ( g_current_floor == 2 )
  {
    if ( VisibilityGet( CHAIR_COL, CHAIR_ROW ) >= 0.01f
         && chair_image && gfx_mode == GFX_IMAGE )
    {
      GV_DrawSprite( vp_rect, cam, chair_image,
                     chair_wx, chair_wy,
                     (float)world->tile_w, (float)world->tile_h );
    }
  }
}
