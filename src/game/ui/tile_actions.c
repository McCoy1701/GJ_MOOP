#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "draw_utils.h"
#include "context_menu.h"
#include "movement.h"
#include "combat.h"
#include "tile_actions.h"

extern Player_t player;

#define TILE_ACTION_COUNT 3

static World_t*       world;
static GameCamera_t*  camera;
static Console_t*     console;
static aSoundEffect_t* sfx_move;
static aSoundEffect_t* sfx_click;

static int tile_action_open    = 0;
static int tile_action_cursor  = 0;
static int tile_action_row, tile_action_col;
static int tile_action_on_self = 0;

/* Check if midground has a door at (r,c) */
static int tile_has_door( int r, int c )
{
  int idx = c * world->width + r;
  return world->midground[idx].tile != TILE_EMPTY;
}

/* Build the label list for the current tile action target */
static int build_labels( const char** labels, Enemy_t* enemies, int num_enemies )
{
  int adjacent = TileAdjacent( tile_action_row, tile_action_col );

  Enemy_t* e = EnemyAt( enemies, num_enemies,
                         tile_action_row, tile_action_col );

  if ( e && adjacent )
  {
    labels[0] = "Attack";
    labels[1] = "Look";
    return 2;
  }

  if ( tile_action_on_self || !adjacent )
  {
    labels[0] = "Look";
    return 1;
  }

  /* Adjacent door */
  if ( tile_has_door( tile_action_row, tile_action_col ) )
  {
    labels[0] = "Open";
    labels[1] = "Look";
    return 2;
  }

  labels[0] = "Move";
  labels[1] = "Look";
  return 2;
}

void TileActionsInit( World_t* w, GameCamera_t* cam, Console_t* con,
                      aSoundEffect_t* move, aSoundEffect_t* click )
{
  world     = w;
  camera    = cam;
  console   = con;
  sfx_move  = move;
  sfx_click = click;
  tile_action_open = 0;
}

void TileActionsOpen( int row, int col, int on_self )
{
  tile_action_row     = row;
  tile_action_col     = col;
  tile_action_cursor  = 0;
  tile_action_on_self = on_self;
  tile_action_open    = 1;
}

int  TileActionsIsOpen( void )  { return tile_action_open; }
void TileActionsClose( void )   { tile_action_open = 0; }

int TileActionsLogic( int mouse_moved, Enemy_t* enemies, int num_enemies )
{
  if ( !tile_action_open ) return 0;

  const char* ta_labels[TILE_ACTION_COUNT];
  int ta_count = build_labels( ta_labels, enemies, num_enemies );

  /* ESC closes menu */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    tile_action_open = 0;
    return 1;
  }

  /* W/S or Up/Down — navigate */
  if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    tile_action_cursor--;
    if ( tile_action_cursor < 0 ) tile_action_cursor = ta_count - 1;
    a_AudioPlaySound( sfx_move, NULL );
  }
  if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    tile_action_cursor++;
    if ( tile_action_cursor >= ta_count ) tile_action_cursor = 0;
    a_AudioPlaySound( sfx_move, NULL );
  }

  /* Scroll wheel */
  if ( app.mouse.wheel != 0 )
  {
    tile_action_cursor += ( app.mouse.wheel < 0 ) ? 1 : -1;
    if ( tile_action_cursor < 0 ) tile_action_cursor = ta_count - 1;
    if ( tile_action_cursor >= ta_count ) tile_action_cursor = 0;
    a_AudioPlaySound( sfx_move, NULL );
    app.mouse.wheel = 0;
  }

  /* Mouse hover on action menu rows */
  if ( mouse_moved )
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float sx, sy, cl, ct;
    float aspect = vp->rect.w / vp->rect.h;
    float cam_w = camera->half_h * aspect;
    sx = vp->rect.w / ( cam_w * 2.0f );
    sy = vp->rect.h / ( camera->half_h * 2.0f );
    cl = camera->x - cam_w;
    ct = camera->y - camera->half_h;
    float twx = tile_action_row * world->tile_w;
    float twy = tile_action_col * world->tile_h;
    float menu_sx = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
    float menu_sy = ( twy - ct ) * sy + vp->rect.y;

    for ( int i = 0; i < ta_count; i++ )
    {
      float ry = menu_sy + i * ( CTX_MENU_ROW_H + CTX_MENU_PAD );
      if ( PointInRect( app.mouse.x, app.mouse.y, menu_sx, ry,
                        CTX_MENU_W, CTX_MENU_ROW_H ) )
      {
        if ( i != tile_action_cursor )
          a_AudioPlaySound( sfx_move, NULL );
        tile_action_cursor = i;
        break;
      }
    }
  }

  /* Space/Enter or click — execute action */
  int exec = 0;
  if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    exec = 1;
  }
  if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
  {
    app.mouse.pressed = 0;
    /* Only execute if click is on the menu, otherwise close */
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float sx, sy, cl, ct;
    float aspect = vp->rect.w / vp->rect.h;
    float cam_w = camera->half_h * aspect;
    sx = vp->rect.w / ( cam_w * 2.0f );
    sy = vp->rect.h / ( camera->half_h * 2.0f );
    cl = camera->x - cam_w;
    ct = camera->y - camera->half_h;
    float twx = tile_action_row * world->tile_w;
    float twy = tile_action_col * world->tile_h;
    float mx = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
    float my = ( twy - ct ) * sy + vp->rect.y;
    float mh = ta_count * ( CTX_MENU_ROW_H + CTX_MENU_PAD ) - CTX_MENU_PAD;

    if ( PointInRect( app.mouse.x, app.mouse.y, mx, my, CTX_MENU_W, mh ) )
    {
      exec = 1;
    }
    else
    {
      /* Click on the same tile = execute first action (double-click shortcut) */
      float cwx, cwy;
      GV_ScreenToWorld( vp->rect, camera, app.mouse.x, app.mouse.y, &cwx, &cwy );
      int cr = (int)( cwx / world->tile_w );
      int cc = (int)( cwy / world->tile_h );
      if ( cr == tile_action_row && cc == tile_action_col )
      {
        tile_action_cursor = 0;
        exec = 1;
      }
      else
        tile_action_open = 0;
    }
  }

  if ( exec )
  {
    a_AudioPlaySound( sfx_click, NULL );
    const char* action = ta_labels[tile_action_cursor];

    if ( strcmp( action, "Move" ) == 0 )
    {
      if ( TileWalkable( tile_action_row, tile_action_col ) )
        PlayerStartMove( tile_action_row, tile_action_col );
      else
      {
        int pr, pc;
        PlayerGetTile( &pr, &pc );
        PlayerWallBump( tile_action_row - pr, tile_action_col - pc );
      }
    }
    else if ( strcmp( action, "Open" ) == 0 )
    {
      int idx = tile_action_col * world->width + tile_action_row;
      Tile_t* door = &world->midground[idx];
      ConsolePushF( console, door->glyph_fg,
                    "The door is locked." );
    }
    else if ( strcmp( action, "Attack" ) == 0 )
    {
      Enemy_t* ae = EnemyAt( enemies, num_enemies,
                              tile_action_row, tile_action_col );
      if ( ae )
      {
        int pr, pc;
        PlayerGetTile( &pr, &pc );
        PlayerLunge( tile_action_row - pr, tile_action_col - pc );
        CombatAttack( ae );
        EnemiesStartTurn( enemies, num_enemies, pr, pc, TileWalkable );
      }
    }
    else /* Look */
    {
      if ( tile_action_on_self )
      {
        ConsolePush( console, "You see yourself.",
                     (aColor_t){ 160, 160, 160, 255 } );
      }
      else
      {
        Enemy_t* le = EnemyAt( enemies, num_enemies,
                                tile_action_row, tile_action_col );
        if ( le )
        {
          EnemyType_t* lt = &g_enemy_types[le->type_idx];
          ConsolePushF( console, lt->color,
                        "%s - %s", lt->name, lt->description );
        }
        else
        {
          int idx = tile_action_col * world->width + tile_action_row;
          Tile_t* mg = &world->midground[idx];
          if ( mg->tile != TILE_EMPTY )
          {
            ConsolePushF( console, mg->glyph_fg,
                          "A locked door." );
          }
          else
          {
            Tile_t* t = &world->background[idx];
            if ( t->solid )
              ConsolePushF( console, (aColor_t){ 160, 160, 160, 255 },
                            "You see a stone wall." );
            else
              ConsolePushF( console, (aColor_t){ 160, 160, 160, 255 },
                            "You see stone floor." );
          }
        }
      }
    }
    tile_action_open = 0;
  }

  /* Consume remaining input */
  app.keyboard[SDL_SCANCODE_W] = 0;
  app.keyboard[SDL_SCANCODE_S] = 0;
  app.keyboard[SDL_SCANCODE_A] = 0;
  app.keyboard[SDL_SCANCODE_D] = 0;
  app.keyboard[SDL_SCANCODE_UP] = 0;
  app.keyboard[SDL_SCANCODE_DOWN] = 0;
  app.keyboard[SDL_SCANCODE_LEFT] = 0;
  app.keyboard[SDL_SCANCODE_RIGHT] = 0;
  app.keyboard[SDL_SCANCODE_SPACE] = 0;
  app.keyboard[SDL_SCANCODE_RETURN] = 0;
  return 1;
}

void TileActionsDraw( Enemy_t* enemies, int num_enemies )
{
  if ( !tile_action_open || !world ) return;

  aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
  float sx, sy, cl, ct;
  float aspect = vp->rect.w / vp->rect.h;
  float cam_w = camera->half_h * aspect;
  sx = vp->rect.w / ( cam_w * 2.0f );
  sy = vp->rect.h / ( camera->half_h * 2.0f );
  cl = camera->x - cam_w;
  ct = camera->y - camera->half_h;

  float twx = tile_action_row * world->tile_w;
  float twy = tile_action_col * world->tile_h;
  float menu_x = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
  float menu_y = ( twy - ct ) * sy + vp->rect.y;

  const char* ta_labels[TILE_ACTION_COUNT];
  int ta_count = build_labels( ta_labels, enemies, num_enemies );

  DrawContextMenu( menu_x, menu_y, ta_labels, ta_count, tile_action_cursor );
}

int TileActionsGetRow( void ) { return tile_action_row; }
int TileActionsGetCol( void ) { return tile_action_col; }
