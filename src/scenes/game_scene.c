#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "draw_utils.h"
#include "context_menu.h"
#include "console.h"
#include "game_events.h"
#include "inventory_ui.h"
#include "transitions.h"
#include "tween.h"
#include "sound_manager.h"
#include "world.h"
#include "game_viewport.h"
#include "game_scene.h"
#include "main_menu.h"

static void gs_Logic( float );
static void gs_Draw( float );

/* Top bar layout */
#define TB_PAD_X        12.0f
#define TB_PAD_Y         8.0f
#define TB_NAME_SCALE    1.6f
#define TB_STAT_SCALE    1.2f
#define TB_STAT_GAP     12.0f   /* gap between each stat token */
#define TB_SECTION_GAP  32.0f   /* gap between name and stats */
#define TB_ESC_SCALE     1.0f

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

/* Tile interaction */
#define TILE_ACTION_COUNT  2


static TweenManager_t gs_tweens;
static int moving = 0;
static int facing_left = 0;  /* 1 = player sprite flipped horizontally */
static float bounce_oy = 0;  /* vertical bounce offset in world units */

static int hover_row = -1, hover_col = -1;

static aTimer_t* rapid_move_timer = NULL;  /* 500ms window for click-to-move */
static int rapid_move_active = 0;

static int tile_action_open   = 0;
static int tile_action_cursor = 0;
static int tile_action_row, tile_action_col;
static int tile_action_on_self = 0;  /* 1 = menu opened on player's tile */

static int look_mode = 0;
static int look_row, look_col;

/* Helper: get player's current tile grid coords */
static void player_tile( int* row, int* col )
{
  *row = (int)( player.world_x / world->tile_w );
  *col = (int)( player.world_y / world->tile_h );
}

/* Helper: check if (r,c) is adjacent to player (Manhattan distance == 1) */
static int tile_adjacent( int r, int c )
{
  int pr, pc;
  player_tile( &pr, &pc );
  return ( abs( r - pr ) + abs( c - pc ) ) == 1;
}

/* Helper: check if tile at (r,c) is in bounds and not solid */
static int tile_walkable( int r, int c )
{
  if ( r < 0 || r >= world->width || c < 0 || c >= world->height ) return 0;
  int idx = c * world->width + r;
  return !world->background[idx].solid;
}

static void bounce_down( void* data )
{
  (void)data;
  TweenFloat( &gs_tweens, &bounce_oy, 0.0f, 0.08f, TWEEN_EASE_IN_QUAD );
}

/* Start a tween-based move to tile (r,c) center */
static void start_move( int r, int c )
{
  int pr, pc;
  player_tile( &pr, &pc );
  if ( r < pr ) facing_left = 1;
  else if ( r > pr ) facing_left = 0;

  float tx = r * world->tile_w + world->tile_w / 2.0f;
  float ty = c * world->tile_h + world->tile_h / 2.0f;
  TweenFloat( &gs_tweens, &player.world_x, tx, 0.15f, TWEEN_EASE_OUT_CUBIC );
  TweenFloat( &gs_tweens, &player.world_y, ty, 0.15f, TWEEN_EASE_OUT_CUBIC );
  moving = 1;
  SoundManagerPlayFootstep();

  /* Little bounce — hop up 2px then back down */
  bounce_oy = 0;
  TweenFloatWithCallback( &gs_tweens, &bounce_oy, -2.0f, 0.07f,
                          TWEEN_EASE_OUT_QUAD, bounce_down, NULL );

  /* Start rapid-move window */
  a_TimerStart( rapid_move_timer );
  rapid_move_active = 1;
}

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

  /* Spawn player at tile (3,3) center */
  player.world_x = 56.0f;
  player.world_y = 56.0f;

  /* Initialize game camera centered on player */
  camera = (GameCamera_t){ player.world_x, player.world_y, 64.0f };
  app.g_viewport = (aRectf_t){ 0, 0, 0, 0 };

  InitTweenManager( &gs_tweens );
  moving = 0;
  tile_action_open = 0;
  look_mode = 0;
  hover_row = hover_col = -1;
  if ( !rapid_move_timer ) rapid_move_timer = a_TimerCreate();
  rapid_move_active = 0;

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  InventoryUIInit( &sfx_move, &sfx_click );

  ConsoleInit( &console );
  GameEventsInit( &console );
  ConsolePush( &console, "Welcome, adventurer.", white );

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
  if ( tile_action_open )
  {
    /* Build label list: on player's tile → "Look" only, else "Move" + "Look" */
    const char* ta_labels[TILE_ACTION_COUNT];
    int ta_count;
    if ( tile_action_on_self )
    {
      ta_labels[0] = "Look";
      ta_count = 1;
    }
    else
    {
      ta_labels[0] = "Move";
      ta_labels[1] = "Look";
      ta_count = TILE_ACTION_COUNT;
    }

    /* ESC closes menu */
    if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
      tile_action_open = 0;
      goto gs_logic_end;
    }

    /* W/S or Up/Down — navigate actions */
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      tile_action_cursor--;
      if ( tile_action_cursor < 0 ) tile_action_cursor = ta_count - 1;
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      tile_action_cursor++;
      if ( tile_action_cursor >= ta_count ) tile_action_cursor = 0;
      a_AudioPlaySound( &sfx_move, NULL );
    }

    /* Mouse hover on action menu rows */
    if ( mouse_moved )
    {
      aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
      float sx, sy, cl, ct;
      float aspect = vp->rect.w / vp->rect.h;
      float cam_w = camera.half_h * aspect;
      sx = vp->rect.w / ( cam_w * 2.0f );
      sy = vp->rect.h / ( camera.half_h * 2.0f );
      cl = camera.x - cam_w;
      ct = camera.y - camera.half_h;
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
            a_AudioPlaySound( &sfx_move, NULL );
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
      exec = 1;
    }

    if ( exec )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      const char* action = ta_labels[tile_action_cursor];

      if ( strcmp( action, "Move" ) == 0 )
      {
        if ( tile_adjacent( tile_action_row, tile_action_col )
             && tile_walkable( tile_action_row, tile_action_col ) )
        {
          start_move( tile_action_row, tile_action_col );
        }
        else
        {
          ConsolePush( &console, "Can't move there.", (aColor_t){ 160, 160, 160, 255 } );
        }
      }
      else /* Look */
      {
        if ( tile_action_on_self )
        {
          ConsolePush( &console, "You see yourself.",
                       (aColor_t){ 160, 160, 160, 255 } );
        }
        else
        {
          int idx = tile_action_col * world->width + tile_action_row;
          Tile_t* t = &world->background[idx];
          if ( t->solid )
            ConsolePushF( &console, (aColor_t){ 160, 160, 160, 255 },
                          "You see a stone wall." );
          else
            ConsolePushF( &console, (aColor_t){ 160, 160, 160, 255 },
                          "You see stone floor." );
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
    goto gs_logic_end;
  }

  /* ---- ESC: close look mode, then inventory menus, then main menu ---- */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    if ( look_mode )
      look_mode = 0;
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

  /* ---- Movement tween (always runs, even in look mode) ---- */
  if ( moving )
  {
    UpdateTweens( &gs_tweens, dt );
    if ( GetActiveTweenCount( &gs_tweens ) == 0 )
      moving = 0;
  }

  /* ---- Look mode ---- */
  if ( look_mode )
  {
    hover_row = -1;
    hover_col = -1;

    /* L toggles off, or click anywhere on map exits */
    if ( app.keyboard[SDL_SCANCODE_L] == 1 )
    {
      app.keyboard[SDL_SCANCODE_L] = 0;
      look_mode = 0;
      goto gs_logic_end;
    }
    if ( app.mouse.pressed )
    {
      app.mouse.pressed = 0;
      look_mode = 0;
      goto gs_logic_end;
    }

    /* Arrow keys / WASD move look cursor */
    if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
    {
      app.keyboard[SDL_SCANCODE_UP] = 0;
      app.keyboard[SDL_SCANCODE_W] = 0;
      if ( look_col > 0 ) { look_col--; a_AudioPlaySound( &sfx_move, NULL ); }
    }
    if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
    {
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      app.keyboard[SDL_SCANCODE_S] = 0;
      if ( look_col < world->height - 1 ) { look_col++; a_AudioPlaySound( &sfx_move, NULL ); }
    }
    if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
    {
      app.keyboard[SDL_SCANCODE_LEFT] = 0;
      app.keyboard[SDL_SCANCODE_A] = 0;
      if ( look_row > 0 ) { look_row--; a_AudioPlaySound( &sfx_move, NULL ); }
    }
    if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
    {
      app.keyboard[SDL_SCANCODE_RIGHT] = 0;
      app.keyboard[SDL_SCANCODE_D] = 0;
      if ( look_row < world->width - 1 ) { look_row++; a_AudioPlaySound( &sfx_move, NULL ); }
    }

    /* Space/Enter opens tile action menu at cursor */
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      tile_action_row = look_row;
      tile_action_col = look_col;
      tile_action_cursor = 0;
      int pr, pc; player_tile( &pr, &pc );
      tile_action_on_self = ( look_row == pr && look_col == pc );
      tile_action_open = 1;
      a_AudioPlaySound( &sfx_click, NULL );
    }

    goto gs_logic_end;
  }

  /* ---- L key: enter look mode ---- */
  if ( app.keyboard[SDL_SCANCODE_L] == 1 )
  {
    app.keyboard[SDL_SCANCODE_L] = 0;
    look_mode = 1;
    player_tile( &look_row, &look_col );
    ConsolePush( &console, "Look mode. WASD/Arrows to move cursor, Enter to inspect, L/ESC to exit.",
                 (aColor_t){ 218, 175, 32, 255 } );
  }

  /* ---- Arrow key / WASD movement (when not moving, not inventory focused) ---- */
  if ( !moving && !InventoryUIFocused() )
  {
    int dr = 0, dc = 0;
    if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
    { app.keyboard[SDL_SCANCODE_UP] = 0; app.keyboard[SDL_SCANCODE_W] = 0; dc = -1; }
    if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
    { app.keyboard[SDL_SCANCODE_DOWN] = 0; app.keyboard[SDL_SCANCODE_S] = 0; dc =  1; }
    if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
    { app.keyboard[SDL_SCANCODE_LEFT] = 0; app.keyboard[SDL_SCANCODE_A] = 0; dr = -1; }
    if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
    { app.keyboard[SDL_SCANCODE_RIGHT] = 0; app.keyboard[SDL_SCANCODE_D] = 0; dr =  1; }

    if ( dr != 0 || dc != 0 )
    {
      int pr, pc;
      player_tile( &pr, &pc );
      int tr = pr + dr;
      int tc = pc + dc;
      if ( tile_walkable( tr, tc ) )
        start_move( tr, tc );
    }
  }

  /* Expire rapid-move window */
  if ( rapid_move_active
       && a_TimerStarted( rapid_move_timer )
       && a_TimerGetTicks( rapid_move_timer ) > 700 )
    rapid_move_active = 0;

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

      /* Mouse is over the viewport — clear inventory focus/highlights */
      if ( mouse_moved )
        InventoryUIUnfocus();

      /* Click on tile */
      if ( app.mouse.pressed && hover_row >= 0 )
      {
        /* Rapid-move: click adjacent walkable tile within window → move immediately */
        if ( rapid_move_active
             && tile_adjacent( hover_row, hover_col )
             && tile_walkable( hover_row, hover_col ) )
        {
          app.mouse.pressed = 0;
          start_move( hover_row, hover_col );
        }
        /* Normal click — open action menu */
        else
        {
          app.mouse.pressed = 0;
          tile_action_row = hover_row;
          tile_action_col = hover_col;
          tile_action_cursor = 0;
          int pr, pc; player_tile( &pr, &pc );
          tile_action_on_self = ( hover_row == pr && hover_col == pc );
          tile_action_open = 1;
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
  /* Always center camera on player */
  camera.x = player.world_x;
  camera.y = player.world_y;

  a_DoWidget();
}

static void gs_DrawTopBar( void )
{
  aContainerWidget_t* tb = a_GetContainerFromWidget( "top_bar" );
  aRectf_t r = tb->rect;
  r.y += TransitionGetTopBarOY();
  float tb_alpha = TransitionGetUIAlpha();

  /* Background + border */
  aColor_t tb_bg = PANEL_BG;
  tb_bg.a = (int)( tb_bg.a * tb_alpha );
  aColor_t tb_fg = PANEL_FG;
  tb_fg.a = (int)( tb_fg.a * tb_alpha );
  a_DrawFilledRect( r, tb_bg );
  a_DrawRect( r, tb_fg );

  aTextStyle_t ts = a_default_text_style;
  ts.bg = (aColor_t){ 0, 0, 0, 0 };

  float tx = r.x + TB_PAD_X;
  float ty = r.y + TB_PAD_Y;

  /* Player name — left side, larger */
  ts.fg = white;
  ts.scale = TB_NAME_SCALE;
  ts.align = TEXT_ALIGN_LEFT;
  a_DrawText( player.name, (int)tx, (int)ty, ts );

  /* Measure name width to place stats right after it */
  float name_w = strlen( player.name ) * 8.0f * TB_NAME_SCALE;
  float sx = tx + name_w + TB_SECTION_GAP;

  /* Stats — flexed right of name, smaller */
  ts.scale = TB_STAT_SCALE;
  ts.align = TEXT_ALIGN_LEFT;
  char buf[48];

  /* HP — label white, numbers colored by percentage */
  ts.fg = white;
  a_DrawText( "HP: ", (int)sx, (int)( ty + 4 ), ts );
  float hp_label_w = 4 * 8.0f * TB_STAT_SCALE;

  float hp_pct = ( player.max_hp > 0 ) ? (float)player.hp / player.max_hp : 0;
  if ( hp_pct > 0.5f )       ts.fg = (aColor_t){ 255, 60, 60, 255 };
  else if ( hp_pct > 0.25f ) ts.fg = yellow;
  else                        ts.fg = (aColor_t){ 200, 0, 0, 255 };

  snprintf( buf, sizeof( buf ), "%d/%d", player.hp, player.max_hp );
  a_DrawText( buf, (int)( sx + hp_label_w ), (int)( ty + 4 ), ts );
  sx += hp_label_w + strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DMG */
  snprintf( buf, sizeof( buf ), "DMG: %d", PlayerStat( "damage" ) );
  ts.fg = white;
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DEF */
  snprintf( buf, sizeof( buf ), "DEF: %d", PlayerStat( "defense" ) );
  ts.fg = white;
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* Equipment effects — yellow */
  ts.fg = yellow;
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    if ( player.equipment[i] < 0 ) continue;
    EquipmentInfo_t* e = &g_equipment[ player.equipment[i] ];
    if ( strcmp( e->effect, "none" ) == 0 ) continue;
    snprintf( buf, sizeof( buf ), "%s(%d)", e->effect, e->effect_value );
    a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
    sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;
  }

  /* Settings[ESC] — far right */
  ts.scale = TB_ESC_SCALE;
  ts.fg = (aColor_t){ 160, 160, 160, 255 };
  ts.align = TEXT_ALIGN_RIGHT;
  a_DrawText( "Settings[ESC]", (int)( r.x + r.w - TB_PAD_X ), (int)( ty + 6 ), ts );
}

static void gs_Draw( float dt )
{
  /* Draw manual backgrounds for boxed:0 panels */

  /* Top bar */
  gs_DrawTopBar();

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

  /* World + Player — clipped to game_viewport panel (draw before UI overlays) */
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
    if ( look_mode )
      GV_DrawTileOutline( vp_rect, &camera, look_row, look_col,
                          world->tile_w, world->tile_h, GOLD );

    /* Tile action menu target highlight */
    if ( tile_action_open )
      GV_DrawTileOutline( vp_rect, &camera, tile_action_row, tile_action_col,
                          world->tile_w, world->tile_h, GOLD );

    float py = player.world_y + bounce_oy;
    if ( player.image && settings.gfx_mode == GFX_IMAGE )
    {
      if ( facing_left )
        GV_DrawSpriteFlipped( vp_rect, &camera, player.image,
                              player.world_x, py, 16.0f, 16.0f, 'x' );
      else
        GV_DrawSprite( vp_rect, &camera, player.image,
                       player.world_x, py, 16.0f, 16.0f );
    }
    else
      GV_DrawFilledRect( vp_rect, &camera,
                         player.world_x, py, 16.0f, 16.0f, white );

    a_DisableClipRect();
  }

  /* Tile action menu (drawn outside clip rect, over everything) */
  if ( tile_action_open && world )
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float sx, sy, cl, ct;
    float aspect = vp->rect.w / vp->rect.h;
    float cam_w = camera.half_h * aspect;
    sx = vp->rect.w / ( cam_w * 2.0f );
    sy = vp->rect.h / ( camera.half_h * 2.0f );
    cl = camera.x - cam_w;
    ct = camera.y - camera.half_h;

    float twx = tile_action_row * world->tile_w;
    float twy = tile_action_col * world->tile_h;
    float menu_x = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
    float menu_y = ( twy - ct ) * sy + vp->rect.y;

    const char* ta_labels[TILE_ACTION_COUNT];
    int ta_count;
    if ( tile_action_on_self )
    {
      ta_labels[0] = "Look";
      ta_count = 1;
    }
    else
    {
      ta_labels[0] = "Move";
      ta_labels[1] = "Look";
      ta_count = TILE_ACTION_COUNT;
    }

    DrawContextMenu( menu_x, menu_y, ta_labels, ta_count,
                     tile_action_cursor );
  }

  if ( TransitionShowLabels() )
    a_DrawWidgets();

  InventoryUISetIntroOffset( TransitionGetRightOX(), TransitionGetUIAlpha() );
  InventoryUIDraw();
}
