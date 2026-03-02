#include <string.h>
#include <stdlib.h>
#include <Archimedes.h>

#include "defines.h"
#include "target_mode.h"
#include "player.h"
#include "items.h"
#include "movement.h"
#include "visibility.h"
#include "draw_utils.h"

extern Player_t player;

/* Targeting styles */
#define TGT_CARDINAL  0   /* shoot/poison - cursor on cardinal lines */
#define TGT_FREE      1   /* magic_bolt/freeze/aoe/swap - free move in range */
#define TGT_ADJACENT  2   /* trap_stun - adjacent tiles only */
#define TGT_SELF      3   /* smoke - instant at player's feet */

#define RANGE_COLOR     (aColor_t){ 0xde, 0x9e, 0x41, 40 }
#define AOE_COLOR       (aColor_t){ 0xcf, 0x57, 0x3c, 50 }
#define CURSOR_COLOR    (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define INVALID_COLOR   (aColor_t){ 0xcf, 0x57, 0x3c, 255 }

static World_t*        world;
static Console_t*      console;
static GameCamera_t*   cam;
static aSoundEffect_t* sfx_move;
static aSoundEffect_t* sfx_click;

static int active       = 0;
static int confirmed    = 0;
static int cons_idx     = -1;
static int slot_idx     = -1;
static int cursor_row, cursor_col;
static int player_row, player_col;

/* Derived from consumable on enter */
static int tgt_style;
static int tgt_range;
static int tgt_los;
static int tgt_aoe_radius;

void TargetModeInit( World_t* w, Console_t* con, GameCamera_t* c,
                     aSoundEffect_t* move, aSoundEffect_t* click )
{
  world     = w;
  console   = con;
  cam       = c;
  sfx_move  = move;
  sfx_click = click;
  active    = 0;
  confirmed = 0;
}

/* Determine targeting style from consumable data */
static int style_from_consumable( ConsumableInfo_t* c )
{
  if ( strcmp( c->effect, "smoke" ) == 0 )
    return TGT_SELF;
  if ( c->place_range > 0 )
    return TGT_ADJACENT;
  if ( strcmp( c->effect, "shoot" ) == 0 || strcmp( c->effect, "poison" ) == 0 )
    return TGT_CARDINAL;
  if ( strcmp( c->effect, "reach" ) == 0 )
    return TGT_CARDINAL;
  if ( strcmp( c->effect, "fire_cone" ) == 0 )
    return TGT_CARDINAL;
  if ( strcmp( c->effect, "cleave" ) == 0 )
    return TGT_ADJACENT;
  return TGT_FREE;
}

void TargetModeEnter( int consumable_idx, int inv_slot )
{
  ConsumableInfo_t* c = &g_consumables[consumable_idx];

  cons_idx  = consumable_idx;
  slot_idx  = inv_slot;
  active    = 1;
  confirmed = 0;

  PlayerGetTile( &player_row, &player_col );
  cursor_row = player_row;
  cursor_col = player_col;

  tgt_style      = style_from_consumable( c );
  tgt_range      = ( c->place_range > 0 ) ? c->place_range : c->range;
  tgt_los        = c->requires_los;
  tgt_aoe_radius = c->aoe_radius;

  /* TGT_SELF: auto-confirm at player's feet (smoke bomb) */
  if ( tgt_style == TGT_SELF )
  {
    confirmed = 1;
    active    = 0;
    return;
  }

  const char* hint = "";
  switch ( tgt_style )
  {
    case TGT_CARDINAL: hint = "Aim with WASD or mouse. Click/Space to fire, ESC to cancel."; break;
    case TGT_FREE:     hint = "Aim with WASD or mouse. Click/Space to confirm, ESC to cancel."; break;
    case TGT_ADJACENT: hint = "Pick adjacent tile. Click/Space to confirm, ESC to cancel."; break;
    default: break;
  }
  ConsolePushF( console, (aColor_t){ 0xde, 0x9e, 0x41, 255 },
                "Targeting: %s. %s", c->name, hint );
}

void TargetModeExit( void )
{
  active    = 0;
  confirmed = 0;
}

int TargetModeActive( void ) { return active; }

/* Check if a tile is a valid cursor position for the current style */
static int valid_tile( int r, int c )
{
  if ( r < 0 || r >= world->width || c < 0 || c >= world->height )
    return 0;
  if ( VisibilityGet( r, c ) < 0.01f )
    return 0;

  int dr = abs( r - player_row );
  int dc = abs( c - player_col );
  int dist = dr + dc;

  switch ( tgt_style )
  {
    case TGT_CARDINAL:
      /* Must be on a cardinal line (same row or same col) and within range */
      if ( r != player_row && c != player_col ) return 0;
      if ( dist > tgt_range || dist == 0 ) return 0;
      if ( tgt_los && !los_clear( player_row, player_col, r, c ) ) return 0;
      return 1;

    case TGT_FREE:
      if ( dist > tgt_range || dist == 0 ) return 0;
      if ( tgt_los && !los_clear( player_row, player_col, r, c ) ) return 0;
      return 1;

    case TGT_ADJACENT:
    {
      int maxd = ( dr > dc ) ? dr : dc;
      if ( maxd != 1 ) return 0;
      return TileWalkable( r, c );
    }
  }
  return 0;
}

int TargetModeLogic( Enemy_t* enemies, int num_enemies )
{
  (void)enemies;
  (void)num_enemies;

  if ( !active ) return 0;

  /* ESC or right-click cancels */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1
       || ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_RIGHT ) )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    app.mouse.pressed = 0;
    ConsolePush( console, "Targeting cancelled.",
                 (aColor_t){ 0x81, 0x97, 0x96, 255 } );
    active = 0;
    return 1;
  }

  /* Mouse hover - move cursor to hovered tile if valid */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    int mx = app.mouse.x;
    int my = app.mouse.y;
    if ( PointInRect( mx, my, vp->rect.x, vp->rect.y, vp->rect.w, vp->rect.h ) )
    {
      float wx, wy;
      GV_ScreenToWorld( vp->rect, cam, mx, my, &wx, &wy );
      int mr = (int)( wx / world->tile_w );
      int mc = (int)( wy / world->tile_h );

      if ( valid_tile( mr, mc ) && ( mr != cursor_row || mc != cursor_col ) )
      {
        cursor_row = mr;
        cursor_col = mc;
      }

      /* Mouse click - confirm if on valid tile, cancel otherwise */
      if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
      {
        app.mouse.pressed = 0;
        if ( valid_tile( mr, mc ) )
        {
          cursor_row = mr;
          cursor_col = mc;
          goto tgt_confirm;
        }
        else
        {
          ConsolePush( console, "Targeting cancelled.",
                       (aColor_t){ 0x81, 0x97, 0x96, 255 } );
          active = 0;
          return 1;
        }
      }
    }
    else if ( app.mouse.pressed )
    {
      /* Click outside viewport cancels */
      app.mouse.pressed = 0;
      ConsolePush( console, "Targeting cancelled.",
                   (aColor_t){ 0x81, 0x97, 0x96, 255 } );
      active = 0;
      return 1;
    }
  }

  /* Keyboard movement */
  int dr = 0, dc = 0;
  if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
  { app.keyboard[SDL_SCANCODE_UP] = 0; app.keyboard[SDL_SCANCODE_W] = 0; dc = -1; }
  else if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
  { app.keyboard[SDL_SCANCODE_DOWN] = 0; app.keyboard[SDL_SCANCODE_S] = 0; dc = 1; }
  else if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
  { app.keyboard[SDL_SCANCODE_LEFT] = 0; app.keyboard[SDL_SCANCODE_A] = 0; dr = -1; }
  else if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
  { app.keyboard[SDL_SCANCODE_RIGHT] = 0; app.keyboard[SDL_SCANCODE_D] = 0; dr = 1; }

  if ( dr != 0 || dc != 0 )
  {
    int nr = cursor_row + dr;
    int nc = cursor_col + dc;

    if ( tgt_style == TGT_CARDINAL )
    {
      /* For cardinal: if cursor is at player, jump to first valid tile in direction.
         If cursor is on a ray, move along that ray or switch direction. */
      if ( cursor_row == player_row && cursor_col == player_col )
      {
        /* Starting from player - jump in pressed direction */
        nr = player_row + dr;
        nc = player_col + dc;
        if ( valid_tile( nr, nc ) )
        {
          cursor_row = nr;
          cursor_col = nc;
          a_AudioPlaySound( sfx_move, NULL );
        }
      }
      else if ( valid_tile( nr, nc ) )
      {
        cursor_row = nr;
        cursor_col = nc;
        a_AudioPlaySound( sfx_move, NULL );
      }
      else
      {
        /* Try switching to a different cardinal direction from player */
        nr = player_row + dr;
        nc = player_col + dc;
        if ( valid_tile( nr, nc ) )
        {
          cursor_row = nr;
          cursor_col = nc;
          a_AudioPlaySound( sfx_move, NULL );
        }
      }
    }
    else
    {
      /* Free or adjacent: just move if valid */
      if ( valid_tile( nr, nc ) )
      {
        cursor_row = nr;
        cursor_col = nc;
        a_AudioPlaySound( sfx_move, NULL );
      }
    }
  }

  /* Confirm (keyboard) */
  if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    app.keyboard[SDL_SCANCODE_RETURN] = 0;

  tgt_confirm:
    if ( cursor_row == player_row && cursor_col == player_col )
    {
      /* Can't confirm on self (except TGT_SELF which is handled elsewhere) */
      return 1;
    }

    /* For non-AoE free targeting, require an enemy at cursor */
    if ( tgt_style == TGT_FREE && tgt_aoe_radius == 0 )
    {
      if ( !EnemyAt( enemies, num_enemies, cursor_row, cursor_col ) )
      {
        ConsolePush( console, "No target there.",
                     (aColor_t){ 0xcf, 0x57, 0x3c, 255 } );
        return 1;
      }
    }



    a_AudioPlaySound( sfx_click, NULL );
    confirmed = 1;
    active    = 0;
    return 1;
  }

  return 1;
}

int TargetModeConfirmed( int* row, int* col, int* consumable_idx, int* inv_slot )
{
  if ( !confirmed ) return 0;
  *row           = cursor_row;
  *col           = cursor_col;
  *consumable_idx = cons_idx;
  *inv_slot      = slot_idx;
  confirmed      = 0;
  return 1;
}

void TargetModeDraw( aRectf_t vp_rect, GameCamera_t* cam, World_t* w )
{
  if ( !active ) return;

  int tw = w->tile_w;
  int th = w->tile_h;

  /* Draw valid range tiles */
  if ( tgt_style == TGT_CARDINAL )
  {
    /* Highlight the 4 cardinal rays */
    static const int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for ( int d = 0; d < 4; d++ )
    {
      for ( int step = 1; step <= tgt_range; step++ )
      {
        int r = player_row + dirs[d][0] * step;
        int c = player_col + dirs[d][1] * step;
        if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) break;
        if ( !TileWalkable( r, c ) ) break;
        if ( VisibilityGet( r, c ) < 0.01f ) break;

        float wx = r * tw + tw / 2.0f;
        float wy = c * th + th / 2.0f;
        GV_DrawFilledRect( vp_rect, cam, wx, wy, (float)tw, (float)th, RANGE_COLOR );
      }
    }
  }
  else if ( tgt_style == TGT_FREE )
  {
    /* Highlight all tiles in Manhattan range */
    for ( int r = player_row - tgt_range; r <= player_row + tgt_range; r++ )
    {
      for ( int c = player_col - tgt_range; c <= player_col + tgt_range; c++ )
      {
        if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) continue;
        int dist = abs( r - player_row ) + abs( c - player_col );
        if ( dist == 0 || dist > tgt_range ) continue;
        if ( VisibilityGet( r, c ) < 0.01f ) continue;
        if ( tgt_los && !los_clear( player_row, player_col, r, c ) ) continue;

        float wx = r * tw + tw / 2.0f;
        float wy = c * th + th / 2.0f;
        GV_DrawFilledRect( vp_rect, cam, wx, wy, (float)tw, (float)th, RANGE_COLOR );
      }
    }
  }
  else if ( tgt_style == TGT_ADJACENT )
  {
    /* Highlight all 8 surrounding tiles */
    static const int dirs[8][2] = {
      {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };
    for ( int d = 0; d < 8; d++ )
    {
      int r = player_row + dirs[d][0];
      int c = player_col + dirs[d][1];
      if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) continue;
      if ( !TileWalkable( r, c ) ) continue;

      float wx = r * tw + tw / 2.0f;
      float wy = c * th + th / 2.0f;
      GV_DrawFilledRect( vp_rect, cam, wx, wy, (float)tw, (float)th, RANGE_COLOR );
    }
  }

  /* AoE preview around cursor */
  if ( tgt_aoe_radius > 0 && !( cursor_row == player_row && cursor_col == player_col ) )
  {
    for ( int r = cursor_row - tgt_aoe_radius; r <= cursor_row + tgt_aoe_radius; r++ )
    {
      for ( int c = cursor_col - tgt_aoe_radius; c <= cursor_col + tgt_aoe_radius; c++ )
      {
        if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) continue;
        int dist = abs( r - cursor_row ) + abs( c - cursor_col );
        if ( dist > tgt_aoe_radius ) continue;
        if ( r == cursor_row && c == cursor_col ) continue; /* cursor tile drawn separately */

        float wx = r * tw + tw / 2.0f;
        float wy = c * th + th / 2.0f;
        GV_DrawFilledRect( vp_rect, cam, wx, wy, (float)tw, (float)th, AOE_COLOR );
      }
    }
  }

  /* Cleave splash preview - highlight all adjacent tiles that will be hit */
  if ( cons_idx >= 0 && strcmp( g_consumables[cons_idx].effect, "cleave" ) == 0
       && ( cursor_row != player_row || cursor_col != player_col ) )
  {
    static const int dirs[8][2] = {
      {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };
    for ( int d = 0; d < 8; d++ )
    {
      int r = player_row + dirs[d][0];
      int c = player_col + dirs[d][1];
      if ( r == cursor_row && c == cursor_col ) continue;
      if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) continue;
      if ( !TileWalkable( r, c ) ) continue;

      float wx = r * tw + tw / 2.0f;
      float wy = c * th + th / 2.0f;
      GV_DrawFilledRect( vp_rect, cam, wx, wy, (float)tw, (float)th, AOE_COLOR );
    }
  }

  /* Fire cone preview - highlight expanding cone in the aimed direction */
  if ( cons_idx >= 0 && strcmp( g_consumables[cons_idx].effect, "fire_cone" ) == 0
       && ( cursor_row != player_row || cursor_col != player_col ) )
  {
    int dr = ( cursor_row > player_row ) ? 1 : ( cursor_row < player_row ) ? -1 : 0;
    int dc = ( cursor_col > player_col ) ? 1 : ( cursor_col < player_col ) ? -1 : 0;
    int perp_r = -dc;
    int perp_c =  dr;

    for ( int step = 1; step <= tgt_range; step++ )
    {
      int spread = step - 1;
      for ( int s = -spread; s <= spread; s++ )
      {
        int r = player_row + step * dr + s * perp_r;
        int c = player_col + step * dc + s * perp_c;
        if ( r < 0 || r >= w->width || c < 0 || c >= w->height ) continue;
        if ( !TileWalkable( r, c ) ) continue;
        if ( r == cursor_row && c == cursor_col ) continue;

        float wx = r * tw + tw / 2.0f;
        float wy = c * th + th / 2.0f;
        GV_DrawFilledRect( vp_rect, cam, wx, wy, (float)tw, (float)th, AOE_COLOR );
      }
    }
  }

  /* Draw cursor outline (only when not at player position) */
  if ( cursor_row != player_row || cursor_col != player_col )
  {
    GV_DrawTileOutline( vp_rect, cam, cursor_row, cursor_col, tw, th, CURSOR_COLOR );
  }
}
