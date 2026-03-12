#include <string.h>
#include <Archimedes.h>

#include "placed_traps.h"
#include "visibility.h"
#include "defines.h"
#include "items.h"
#include "player.h"

extern Player_t player;

static PlacedTrap_t traps[MAX_PLACED_TRAPS];
static int          num_traps = 0;
static Console_t*   console   = NULL;
#define TRAP_COLOR       (aColor_t){ 160, 160, 160, 80 }
#define TRAP_GLYPH_COLOR (aColor_t){ 160, 160, 160, 200 }
#define TRAP_PLACED_DOT  (aColor_t){ 0x40, 0xc0, 0x40, 200 }

void PlacedTrapsInit( Console_t* con )
{
  console = con;
  memset( traps, 0, sizeof( traps ) );
  num_traps = 0;
}

void PlacedTrapSpawn( int row, int col, int damage, int stun_turns,
                      int cons_idx, aImage_t* image )
{
  int slot = -1;
  for ( int i = 0; i < num_traps; i++ )
  {
    if ( !traps[i].active ) { slot = i; break; }
  }
  if ( slot < 0 )
  {
    if ( num_traps >= MAX_PLACED_TRAPS ) return;
    slot = num_traps++;
  }

  traps[slot].row         = row;
  traps[slot].col         = col;
  traps[slot].damage      = damage;
  traps[slot].stun_turns  = stun_turns;
  traps[slot].active      = 1;
  traps[slot].cons_idx    = cons_idx;
  traps[slot].image       = image;
}

PlacedTrap_t* PlacedTrapAt( int row, int col )
{
  for ( int i = 0; i < num_traps; i++ )
  {
    if ( traps[i].active && traps[i].row == row && traps[i].col == col )
      return &traps[i];
  }
  return NULL;
}

void PlacedTrapRemove( PlacedTrap_t* trap )
{
  trap->active = 0;
}

int PlacedTrapPickup( int row, int col )
{
  PlacedTrap_t* trap = PlacedTrapAt( row, col );
  if ( !trap ) return 0;

  int slot = InventoryAdd( INV_CONSUMABLE, trap->cons_idx );
  if ( slot < 0 ) return 0;

  ConsolePushF( console, (aColor_t){ 0x75, 0xa7, 0x43, 255 },
                "You pick up your %s.", g_consumables[trap->cons_idx].name );
  PlacedTrapRemove( trap );
  return 1;
}

void PlacedTrapsDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                         World_t* world, int gfx_mode )
{
  for ( int i = 0; i < num_traps; i++ )
  {
    if ( !traps[i].active ) continue;
    if ( VisibilityGet( traps[i].row, traps[i].col ) < 0.01f ) continue;

    float wx = traps[i].row * world->tile_w + world->tile_w / 2.0f;
    float wy = traps[i].col * world->tile_h + world->tile_h / 2.0f;

    /* Convert tile top-left to screen coords for pixel overlays */
    float sx, sy;
    GV_WorldToScreen( vp_rect, cam,
                      wx - world->tile_w / 2.0f,
                      wy - world->tile_h / 2.0f,
                      &sx, &sy );
    float half_w = cam->half_h * ( vp_rect.w / vp_rect.h );
    float nw = world->tile_w * ( vp_rect.w / ( half_w * 2.0f ) );
    float nh = world->tile_h * ( vp_rect.h / ( cam->half_h * 2.0f ) );

    if ( traps[i].image && gfx_mode == GFX_IMAGE )
    {
      GV_DrawSprite( vp_rect, cam, traps[i].image,
                     wx, wy,
                     (float)world->tile_w, (float)world->tile_h );
    }
    else
    {
      GV_DrawFilledRect( vp_rect, cam, wx, wy,
                         (float)world->tile_w, (float)world->tile_h,
                         TRAP_COLOR );

      a_DrawGlyph( "^", (int)sx, (int)sy, (int)nw, (int)nh,
                   TRAP_GLYPH_COLOR, (aColor_t){ 0, 0, 0, 0 },
                   FONT_CODE_PAGE_437 );
    }

    /* Green dot overlay to distinguish placed traps from pickups */
    float gs = nw * 0.12f;
    float gp = nw * 0.1f;
    a_DrawFilledRect( (aRectf_t){ sx + gp,            sy + gp,            gs, gs }, TRAP_PLACED_DOT );
    a_DrawFilledRect( (aRectf_t){ sx + nw - gp - gs,  sy + gp,            gs, gs }, TRAP_PLACED_DOT );
    a_DrawFilledRect( (aRectf_t){ sx + gp,            sy + nh - gp - gs,  gs, gs }, TRAP_PLACED_DOT );
    a_DrawFilledRect( (aRectf_t){ sx + nw - gp - gs,  sy + nh - gp - gs,  gs, gs }, TRAP_PLACED_DOT );
  }
}
