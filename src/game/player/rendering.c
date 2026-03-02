#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "movement.h"
#include "combat_vfx.h"
#include "game_viewport.h"
#include "floor_cutscene.h"

extern Player_t player;

void PlayerDraw( aRectf_t vp_rect, GameCamera_t* cam, int gfx_mode )
{
  float py = player.world_y + PlayerBounceOY() + FloorCutscenePlayerOY();

  /* Shadow - pinned to bottom of tile, doesn't bounce with player */
  float shadow_oy = ( gfx_mode == GFX_IMAGE ) ? 8.0f : 7.0f;
  GV_DrawFilledRect( vp_rect, cam,
                     player.world_x, player.world_y + shadow_oy,
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
  {
    float sx, sy;
    GV_WorldToScreen( vp_rect, cam,
                      player.world_x - 8.0f, py - 8.0f,
                      &sx, &sy );
    float half_w = cam->half_h * ( vp_rect.w / vp_rect.h );
    int dw = (int)( 16.0f * ( vp_rect.w / ( half_w * 2.0f ) ) );
    int dh = (int)( 16.0f * ( vp_rect.h / ( cam->half_h * 2.0f ) ) );

    a_DrawGlyph( player.glyph, (int)sx, (int)sy, dw, dh,
                 player.color, (aColor_t){ 0, 0, 0, 0 }, FONT_CODE_PAGE_437 );
  }
}
