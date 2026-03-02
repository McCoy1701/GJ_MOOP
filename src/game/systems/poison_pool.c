#include <string.h>
#include <Archimedes.h>

#include "poison_pool.h"
#include "player.h"
#include "combat_vfx.h"
#include "visibility.h"

extern Player_t player;

static PoisonPool_t pools[MAX_POISON_POOLS];
static int          num_pools = 0;
static Console_t*   console   = NULL;

/* Default green (used when no color specified) */
#define POOL_DEFAULT_COLOR (aColor_t){ 50, 220, 50, 255 }

void PoisonPoolInit( Console_t* con )
{
  console = con;
  memset( pools, 0, sizeof( pools ) );
  num_pools = 0;
}

void PoisonPoolSpawn( int row, int col, int duration, int damage,
                      aColor_t color )
{
  /* Reuse an inactive slot or append */
  int slot = -1;
  for ( int i = 0; i < num_pools; i++ )
  {
    if ( !pools[i].active ) { slot = i; break; }
  }
  if ( slot < 0 )
  {
    if ( num_pools >= MAX_POISON_POOLS ) return;
    slot = num_pools++;
  }

  pools[slot].row             = row;
  pools[slot].col             = col;
  pools[slot].turns_remaining = duration;
  pools[slot].damage          = damage;
  pools[slot].active          = 1;
  pools[slot].color           = color;
}

PoisonPool_t* PoisonPoolAt( int row, int col )
{
  for ( int i = 0; i < num_pools; i++ )
  {
    if ( pools[i].active && pools[i].row == row && pools[i].col == col )
      return &pools[i];
  }
  return NULL;
}

void PoisonPoolTick( int player_row, int player_col )
{
  for ( int i = 0; i < num_pools; i++ )
  {
    if ( !pools[i].active ) continue;

    /* Damage player if standing on pool */
    if ( pools[i].row == player_row && pools[i].col == player_col )
    {
      aColor_t c = pools[i].color;
      PlayerTakeDamage( pools[i].damage );
      CombatVFXSpawnNumber( player.world_x, player.world_y,
                            pools[i].damage, c );
      ConsolePushF( console, c,
                    "The poison pool burns you for %d damage!",
                    pools[i].damage );
      if ( player.hp <= 0 )
      {
        ConsolePush( console, "You have fallen...",
                     (aColor_t){ 0xa5, 0x30, 0x30, 255 } );
      }
    }

    /* Decrement turn counter */
    pools[i].turns_remaining--;
    if ( pools[i].turns_remaining <= 0 )
      pools[i].active = 0;
  }
}

void PoisonPoolDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                        World_t* world, int gfx_mode )
{
  (void)gfx_mode;

  for ( int i = 0; i < num_pools; i++ )
  {
    if ( !pools[i].active ) continue;
    if ( VisibilityGet( pools[i].row, pools[i].col ) < 0.01f ) continue;

    float wx = pools[i].row * world->tile_w + world->tile_w / 2.0f;
    float wy = pools[i].col * world->tile_h + world->tile_h / 2.0f;

    aColor_t fill  = { pools[i].color.r, pools[i].color.g,
                        pools[i].color.b, 100 };
    aColor_t glyph = { pools[i].color.r, pools[i].color.g,
                        pools[i].color.b, 180 };

    GV_DrawFilledRect( vp_rect, cam, wx, wy,
                       (float)world->tile_w, (float)world->tile_h,
                       fill );

    /* Draw '~' glyph over the pool */
    float sx, sy;
    GV_WorldToScreen( vp_rect, cam,
                      wx - world->tile_w / 2.0f,
                      wy - world->tile_h / 2.0f,
                      &sx, &sy );
    float half_w = cam->half_h * ( vp_rect.w / vp_rect.h );
    int dw = (int)( world->tile_w * ( vp_rect.w / ( half_w * 2.0f ) ) );
    int dh = (int)( world->tile_h * ( vp_rect.h / ( cam->half_h * 2.0f ) ) );
    a_DrawGlyph( "~", (int)sx, (int)sy, dw, dh,
                 glyph, (aColor_t){ 0, 0, 0, 0 },
                 FONT_CODE_PAGE_437 );
  }
}
