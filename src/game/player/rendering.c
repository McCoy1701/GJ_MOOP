#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "movement.h"
#include "game_viewport.h"

extern Player_t player;

void PlayerDraw( aRectf_t vp_rect, GameCamera_t* cam, int gfx_mode )
{
  float py = player.world_y + PlayerBounceOY();

  /* Shadow — pinned to bottom of tile, doesn't bounce with player */
  GV_DrawFilledRect( vp_rect, cam,
                     player.world_x, player.world_y + 8.0f,
                     10.0f, 3.0f,
                     (aColor_t){ 0, 0, 0, 80 } );

  if ( player.image && gfx_mode == GFX_IMAGE )
  {
    if ( PlayerFacingLeft() )
      GV_DrawSpriteFlipped( vp_rect, cam, player.image,
                            player.world_x, py, 16.0f, 16.0f, 'x' );
    else
      GV_DrawSprite( vp_rect, cam, player.image,
                     player.world_x, py, 16.0f, 16.0f );
  }
  else
    GV_DrawFilledRect( vp_rect, cam,
                       player.world_x, py, 16.0f, 16.0f, white );
}
