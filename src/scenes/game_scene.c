#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "draw_utils.h"
#include "console.h"
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

/* Panel colors */
#define PANEL_BG  (aColor_t){ 0, 0, 0, 200 }
#define PANEL_FG  (aColor_t){ 255, 255, 255, 255 }

static Console_t console;

void GameSceneInit( void )
{
  app.delegate.logic = gs_Logic;
  app.delegate.draw  = gs_Draw;

  app.options.scale_factor = 1;

  a_WidgetsInit( "resources/widgets/game_scene.auf" );
  app.active_widget = a_GetWidget( "inv_panel" );

  ConsoleInit( &console );
  ConsolePush( &console, "Welcome, adventurer.", white );
}

static void gs_Logic( float dt )
{
  a_DoInput();

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_WidgetCacheFree();
    MainMenuInit();
    return;
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/game_scene.auf" );
  }

  a_DoWidget();
}

static void gs_DrawTopBar( void )
{
  aContainerWidget_t* tb = a_GetContainerFromWidget( "top_bar" );
  aRectf_t r = tb->rect;

  /* Background + border */
  a_DrawFilledRect( r, PANEL_BG );
  a_DrawRect( r, PANEL_FG );

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
  char buf[32];

  /* HP — color based on percentage */
  float hp_pct = ( player.max_hp > 0 ) ? (float)player.hp / player.max_hp : 0;
  if ( hp_pct > 0.5f )       ts.fg = white;
  else if ( hp_pct > 0.25f ) ts.fg = yellow;
  else                        ts.fg = (aColor_t){ 255, 60, 60, 255 };

  snprintf( buf, sizeof( buf ), "HP: %d/%d", player.hp, player.max_hp );
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DMG */
  snprintf( buf, sizeof( buf ), "DMG: %d", player.base_damage );
  ts.fg = white;
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DEF */
  snprintf( buf, sizeof( buf ), "DEF: %d", player.defense );
  ts.fg = white;
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );

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
    a_DrawRect( vr, PANEL_FG );
  }

  /* Inventory panel */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    a_DrawFilledRect( ip->rect, PANEL_BG );
    a_DrawRect( ip->rect, PANEL_FG );
  }

  /* Key items panel */
  {
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    a_DrawFilledRect( kp->rect, PANEL_BG );
    a_DrawRect( kp->rect, PANEL_FG );
  }

  /* Console panel */
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    a_DrawFilledRect( cp->rect, PANEL_BG );
    a_DrawRect( cp->rect, PANEL_FG );
    ConsoleDraw( &console, cp->rect );
  }

  a_DrawWidgets();

  /* Player character centered in viewport */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float size = 64.0f;
    float cx = vp->rect.x + ( vp->rect.w - size ) / 2.0f;
    float cy = vp->rect.y + ( vp->rect.h - size ) / 2.0f;
    DrawImageOrGlyph( player.image, "?", white, cx, cy, size );
  }
}
