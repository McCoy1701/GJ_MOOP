#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "draw_utils.h"
#include "console.h"
#include "game_events.h"
#include "inventory_ui.h"
#include "transitions.h"
#include "sound_manager.h"
#include "world.h"
#include "game_viewport.h"
#include "game_scene.h"
#include "main_menu.h"
#include "enemies.h"
#include "combat.h"
#include "movement.h"
#include "tile_actions.h"
#include "look_mode.h"
#include "hud.h"

static void gs_Logic( float );
static void gs_Draw( float );

/* Viewport zoom limits (half-height in world units) */
#define ZOOM_MIN_H       20.0f
#define ZOOM_MAX_H      100.0f

/* Panel colors */
#define PANEL_BG  (aColor_t){ 0, 0, 0, 200 }
#define PANEL_FG  (aColor_t){ 255, 255, 255, 255 }
#define GOLD      (aColor_t){ 218, 175, 32, 255 }

static aTileset_t*  tileset = NULL;
static World_t*     world   = NULL;
static GameCamera_t camera;

static int prev_mx = -1;
static int prev_my = -1;
static int mouse_moved = 0;

static aSoundEffect_t sfx_move;
static aSoundEffect_t sfx_click;

static Console_t console;

static int hover_row = -1, hover_col = -1;

/* Enemies */
static Enemy_t  enemies[MAX_ENEMIES];
static int      num_enemies = 0;
static int      was_moving = 0;  /* tracks previous frame's moving state */

void GameSceneInit( void )
{
  app.delegate.logic = gs_Logic;
  app.delegate.draw  = gs_Draw;

  app.options.scale_factor = 1;

  a_WidgetsInit( "resources/widgets/game_scene.auf" );
  app.active_widget = a_GetWidget( "inv_panel" );

  /* Load tileset and create 8x8 world */
  tileset = a_TilesetCreate( "resources/assets/tiles/level01tilemap.png", 16, 16 );
  world   = WorldCreate( 8, 8, 16, 16 );

  /* Walls on edges, floor (default) inside */
  for ( int r = 0; r < world->height; r++ )
  {
    for ( int c = 0; c < world->width; c++ )
    {
      if ( r == 0 || r == world->height - 1 || c == 0 || c == world->width - 1 )
      {
        int idx = r * world->width + c;
        world->background[idx].tile     = 1;
        world->background[idx].glyph    = "#";
        world->background[idx].glyph_fg = (aColor_t){ 160, 160, 160, 255 };
        world->background[idx].solid    = 1;
      }
    }
  }

  /* Doors on midground — one per wall, centered.
     Clear the background wall so floor shows through. */
  {
    int w = world->width;
    int mid = w / 2;

    struct { int idx; uint32_t tile; aColor_t color; } doors[] = {
      { 0 * w + mid,                2, { 100, 180, 255, 255 } }, /* top: blue   */
      { (world->height - 1) * w + mid, 3, {  60, 255,  60, 255 } }, /* bot: green  */
      { mid * w + 0,                4, { 255,  60,  60, 255 } }, /* left: red   */
      { mid * w + (w - 1),          5, { 240, 240, 240, 255 } }, /* right: white */
    };

    for ( int i = 0; i < 4; i++ )
    {
      int idx = doors[i].idx;

      /* Clear wall from background */
      world->background[idx].tile     = TILE_EMPTY;
      world->background[idx].glyph    = ".";
      world->background[idx].glyph_fg = (aColor_t){ 80, 80, 80, 255 };
      world->background[idx].solid    = 0;

      /* Place door on midground */
      world->midground[idx].tile     = doors[i].tile;
      world->midground[idx].glyph    = "+";
      world->midground[idx].glyph_fg = doors[i].color;
      world->midground[idx].solid    = 1;
    }
  }

  /* Spawn player at tile (3,3) center */
  player.world_x = 56.0f;
  player.world_y = 56.0f;

  /* Initialize game camera centered on player */
  camera = (GameCamera_t){ player.world_x, player.world_y, 64.0f };
  app.g_viewport = (aRectf_t){ 0, 0, 0, 0 };

  hover_row = hover_col = -1;

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  /* Init subsystems */
  MovementInit( world );
  TileActionsInit( world, &camera, &console, &sfx_move, &sfx_click );
  LookModeInit( world, &console, &sfx_move, &sfx_click );
  InventoryUIInit( &sfx_move, &sfx_click );

  ConsoleInit( &console );
  GameEventsInit( &console );
  ConsolePush( &console, "Welcome, adventurer.", white );

  /* Enemies & combat */
  EnemiesLoadTypes();
  EnemiesInit( enemies, &num_enemies );
  CombatInit( &console );
  EnemiesSetWorld( world );
  EnemySpawn( enemies, &num_enemies, EnemyTypeByKey( "rat" ),
              5, 5, world->tile_w, world->tile_h );
  was_moving = 0;

  SoundManagerPlayGame();
  TransitionIntroStart();
}

static void gs_Logic( float dt )
{
  a_DoInput();
  SoundManagerUpdate( dt );

  /* Track whether the mouse actually moved this frame */
  mouse_moved = ( app.mouse.x != prev_mx || app.mouse.y != prev_my );
  prev_mx = app.mouse.x;
  prev_my = app.mouse.y;

  /* Intro cinematic — blocks all input until done */
  if ( TransitionIntroActive() )
  {
    if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
      TransitionIntroSkip();
    }
    else
    {
      TransitionIntroUpdate( dt );
    }

    /* Override camera during intro */
    camera.half_h = TransitionGetViewportH();
    camera.x = player.world_x;
    camera.y = player.world_y;

    a_DoWidget();
    return;
  }

  /* ---- Tile action menu (highest priority after intro) ---- */
  if ( TileActionsLogic( mouse_moved, enemies, num_enemies ) )
    goto gs_logic_end;

  /* ---- ESC: close look mode, then inventory menus, then main menu ---- */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    if ( LookModeActive() )
      LookModeExit();
    else if ( !InventoryUICloseMenus() )
    {
      a_WidgetCacheFree();
      MainMenuInit();
      return;
    }
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/game_scene.auf" );
  }

  InventoryUILogic( mouse_moved );

  /* When inventory is focused, consume WASD/arrows */
  if ( InventoryUIFocused() )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
  }

  /* ---- Movement update (always runs, even in look mode) ---- */
  MovementUpdate( dt );
  EnemiesUpdate( dt );

  /* Enemy turn: fires once when player finishes a move */
  if ( was_moving && !PlayerIsMoving() && !EnemiesTurning() )
  {
    int pr, pc;
    PlayerGetTile( &pr, &pc );
    EnemiesStartTurn( enemies, num_enemies, pr, pc, TileWalkable );
  }
  was_moving = PlayerIsMoving();

  /* ---- Look mode ---- */
  if ( LookModeLogic( mouse_moved ) )
  {
    hover_row = -1;
    hover_col = -1;
    goto gs_logic_end;
  }

  /* ---- L key: enter look mode ---- */
  if ( app.keyboard[SDL_SCANCODE_L] == 1 )
  {
    app.keyboard[SDL_SCANCODE_L] = 0;
    int pr, pc;
    PlayerGetTile( &pr, &pc );
    LookModeEnter( pr, pc );
  }

  /* ---- Arrow key / WASD movement (when not moving, not inventory focused) ---- */
  if ( !PlayerIsMoving() && !EnemiesTurning() && !InventoryUIFocused() )
  {
    int dr = 0, dc = 0;
    if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
    { app.keyboard[SDL_SCANCODE_UP] = 0; app.keyboard[SDL_SCANCODE_W] = 0; dc = -1; }
    else if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
    { app.keyboard[SDL_SCANCODE_DOWN] = 0; app.keyboard[SDL_SCANCODE_S] = 0; dc =  1; }
    else if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
    { app.keyboard[SDL_SCANCODE_LEFT] = 0; app.keyboard[SDL_SCANCODE_A] = 0; dr = -1; }
    else if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
    { app.keyboard[SDL_SCANCODE_RIGHT] = 0; app.keyboard[SDL_SCANCODE_D] = 0; dr =  1; }

    if ( dr != 0 || dc != 0 )
    {
      int pr, pc;
      PlayerGetTile( &pr, &pc );
      int tr = pr + dr;
      int tc = pc + dc;

      /* Bump-to-attack: enemy on target tile */
      Enemy_t* bump = EnemyAt( enemies, num_enemies, tr, tc );
      if ( bump )
      {
        PlayerLunge( dr, dc );
        CombatAttack( bump );
        EnemiesStartTurn( enemies, num_enemies, pr, pc, TileWalkable );
      }
      else if ( TileWalkable( tr, tc ) )
        PlayerStartMove( tr, tc );
      else
        PlayerWallBump( dr, dc );
    }
  }

  /* ---- Mouse hover over viewport tiles ---- */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    int mx = app.mouse.x;
    int my = app.mouse.y;
    hover_row = -1;
    hover_col = -1;

    if ( PointInRect( mx, my, vp->rect.x, vp->rect.y, vp->rect.w, vp->rect.h ) )
    {
      float wx, wy;
      GV_ScreenToWorld( vp->rect, &camera, mx, my, &wx, &wy );
      int r = (int)( wx / world->tile_w );
      int c = (int)( wy / world->tile_h );
      if ( r >= 0 && r < world->width && c >= 0 && c < world->height )
      {
        hover_row = r;
        hover_col = c;
      }

      /* Mouse is over a tile — clear inventory focus/highlights */
      if ( mouse_moved && hover_row >= 0 )
        InventoryUIUnfocus();

      /* Click on tile (blocked while moving / enemies turning) */
      if ( app.mouse.pressed && hover_row >= 0
           && !PlayerIsMoving() && !EnemiesTurning() )
      {
        /* Rapid-move: click adjacent walkable tile within window */
        if ( RapidMoveActive()
             && TileAdjacent( hover_row, hover_col )
             && TileWalkable( hover_row, hover_col )
             && !EnemyAt( enemies, num_enemies, hover_row, hover_col ) )
        {
          app.mouse.pressed = 0;
          PlayerStartMove( hover_row, hover_col );
        }
        /* Normal click — open action menu */
        else
        {
          app.mouse.pressed = 0;
          int pr, pc;
          PlayerGetTile( &pr, &pc );
          int on_self = ( hover_row == pr && hover_col == pc );
          TileActionsOpen( hover_row, hover_col, on_self );
        }
      }
    }
  }

  /* ---- Mouse wheel: console scroll or viewport zoom ---- */
  if ( app.mouse.wheel != 0 )
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    if ( PointInRect( app.mouse.x, app.mouse.y,
                      cp->rect.x, cp->rect.y, cp->rect.w, cp->rect.h ) )
    {
      ConsoleScroll( &console, app.mouse.wheel );
      app.mouse.wheel = 0;
    }
  }
  if ( app.mouse.wheel != 0 )
  {
    GV_Zoom( &camera, app.mouse.wheel, ZOOM_MIN_H, ZOOM_MAX_H );
    app.mouse.wheel = 0;
  }

gs_logic_end:
  /* Always center camera on player + shake offset */
  camera.x = player.world_x + PlayerShakeOX();
  camera.y = player.world_y + PlayerShakeOY();

  a_DoWidget();
}

static void gs_Draw( float dt )
{
  (void)dt;

  /* Top bar */
  HUDDrawTopBar();

  /* Game viewport — shrink 1px on right so it doesn't overlap right panels */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    aRectf_t vr = { vp->rect.x, vp->rect.y, vp->rect.w - 1, vp->rect.h };
    a_DrawFilledRect( vr, (aColor_t){ 0, 0, 0, 255 } );
    a_DrawRect( vr, (aColor_t){ 255, 255, 255, 255 } );
  }

  /* Inventory panel background */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aRectf_t ir = ip->rect;
    ir.x += TransitionGetRightOX();
    a_DrawFilledRect( ir, PANEL_BG );
  }

  /* Equipment panel background */
  {
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    aRectf_t kr = kp->rect;
    kr.x += TransitionGetRightOX();
    a_DrawFilledRect( kr, PANEL_BG );
  }

  /* Console panel */
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    aRectf_t cr = cp->rect;
    cr.y += TransitionGetConsoleOY();
    aColor_t con_bg = PANEL_BG;
    con_bg.a = (int)( con_bg.a * TransitionGetUIAlpha() );
    aColor_t con_fg = PANEL_FG;
    con_fg.a = (int)( con_fg.a * TransitionGetUIAlpha() );
    a_DrawFilledRect( cr, con_bg );
    a_DrawRect( cr, con_fg );
    ConsoleDraw( &console, cr );
  }

  /* World + Player — clipped to game_viewport panel */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    aRectf_t clip = { vp->rect.x + 1, vp->rect.y + 1, vp->rect.w - 3, vp->rect.h - 2 };
    a_SetClipRect( clip );

    aRectf_t vp_rect = vp->rect;

    if ( world && tileset )
      GV_DrawWorld( vp_rect, &camera, world, tileset, settings.gfx_mode == GFX_ASCII );

    /* Fade overlay on map only — drawn BEFORE player so player stays visible */
    float fade = 1.0f - TransitionGetViewportAlpha();
    if ( fade > 0.01f )
      a_DrawFilledRect( clip, (aColor_t){ 0, 0, 0, (int)( 255 * fade ) } );

    /* Hover highlight */
    if ( hover_row >= 0 && hover_col >= 0 )
      GV_DrawTileOutline( vp_rect, &camera, hover_row, hover_col,
                          world->tile_w, world->tile_h,
                          (aColor_t){ 255, 255, 255, 80 } );

    /* Look mode cursor */
    LookModeDraw( vp_rect, &camera );

    /* Tile action menu target highlight */
    if ( TileActionsIsOpen() )
      GV_DrawTileOutline( vp_rect, &camera,
                          TileActionsGetRow(), TileActionsGetCol(),
                          world->tile_w, world->tile_h, GOLD );

    /* Draw enemies */
    EnemiesDrawAll( vp_rect, &camera, enemies, num_enemies,
                    world, settings.gfx_mode );

    /* Player sprite */
    PlayerDraw( vp_rect, &camera, settings.gfx_mode );

    a_DisableClipRect();
  }

  /* Tile action menu (drawn outside clip rect, over everything) */
  TileActionsDraw( enemies, num_enemies );

  if ( TransitionShowLabels() )
    a_DrawWidgets();

  InventoryUISetIntroOffset( TransitionGetRightOX(), TransitionGetUIAlpha() );
  InventoryUIDraw();
}
