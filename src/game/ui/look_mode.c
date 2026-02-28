#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "movement.h"
#include "tile_actions.h"
#include "look_mode.h"

#define GOLD (aColor_t){ 218, 175, 32, 255 }

extern Player_t player;

static World_t*       world;
static Console_t*     console;
static aSoundEffect_t* sfx_move;
static aSoundEffect_t* sfx_click;

static int look_mode = 0;
static int look_row, look_col;

void LookModeInit( World_t* w, Console_t* con,
                   aSoundEffect_t* move, aSoundEffect_t* click )
{
  world     = w;
  console   = con;
  sfx_move  = move;
  sfx_click = click;
  look_mode = 0;
}

int  LookModeActive( void ) { return look_mode; }

void LookModeEnter( int start_row, int start_col )
{
  look_mode = 1;
  look_row  = start_row;
  look_col  = start_col;
  ConsolePush( console,
    "Look mode. WASD/Arrows to move cursor, Enter/Space to inspect, L/ESC to exit.",
    GOLD );
}

void LookModeExit( void ) { look_mode = 0; }

void LookModeGetCursor( int* row, int* col )
{
  *row = look_row;
  *col = look_col;
}

int LookModeLogic( int mouse_moved )
{
  (void)mouse_moved;
  if ( !look_mode ) return 0;

  /* L toggles off, or click anywhere exits */
  if ( app.keyboard[SDL_SCANCODE_L] == 1 )
  {
    app.keyboard[SDL_SCANCODE_L] = 0;
    look_mode = 0;
    return 1;
  }
  if ( app.mouse.pressed )
  {
    app.mouse.pressed = 0;
    look_mode = 0;
    return 1;
  }

  /* Arrow keys / WASD move look cursor */
  if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
  {
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_W] = 0;
    if ( look_col > 0 ) { look_col--; a_AudioPlaySound( sfx_move, NULL ); }
  }
  if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
  {
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    if ( look_col < world->height - 1 ) { look_col++; a_AudioPlaySound( sfx_move, NULL ); }
  }
  if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
  {
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    if ( look_row > 0 ) { look_row--; a_AudioPlaySound( sfx_move, NULL ); }
  }
  if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    if ( look_row < world->width - 1 ) { look_row++; a_AudioPlaySound( sfx_move, NULL ); }
  }

  /* Space/Enter opens tile action menu at cursor */
  if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    int pr, pc;
    PlayerGetTile( &pr, &pc );
    int on_self = ( look_row == pr && look_col == pc );
    TileActionsOpen( look_row, look_col, on_self );
    a_AudioPlaySound( sfx_click, NULL );
  }

  return 1;
}

void LookModeDraw( aRectf_t vp_rect, GameCamera_t* cam )
{
  if ( !look_mode ) return;
  GV_DrawTileOutline( vp_rect, cam, look_row, look_col,
                      world->tile_w, world->tile_h, GOLD );
}
