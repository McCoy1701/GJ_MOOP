#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "draw_utils.h"
#include "persist.h"
#include "settings.h"
#include "main_menu.h"
#include "sound_manager.h"
#include "lore.h"

static void st_Logic( float );
static void st_Draw( float );

#define NUM_SETTINGS 4
#define ITEM_H       32.0f
#define ITEM_SPACING  6.0f
#define VOL_STEP      5

enum { SET_GFX, SET_MUSIC, SET_SFX, SET_DELETE };

/* Confirmation state for delete */
enum { ST_NORMAL, ST_CONFIRM_1, ST_CONFIRM_2 };
static int confirm_state  = ST_NORMAL;
static int confirm_cursor = 1; /* 0=Yes, 1=No (default No) */

static int cursor;
static int back_hovered;
static int prev_mouse_down;

static const char* setting_labels[] = {
  "Graphics", "Music Volume", "SFX Volume", "Delete Save Data"
};

static aSoundEffect_t sfx_move;
static aSoundEffect_t sfx_click;

/* --- helpers --- */

static const char* st_GetValueStr( int row, char* buf, int buflen )
{
  switch ( row )
  {
    case SET_GFX:
      return settings.gfx_mode == GFX_IMAGE ? "Image" : "ASCII";
    case SET_MUSIC:
      snprintf( buf, buflen, "%d%%", settings.music_vol );
      return buf;
    case SET_SFX:
      snprintf( buf, buflen, "%d%%", settings.sfx_vol );
      return buf;
  }
  return "";
}

static void st_Adjust( int row, int dir )
{
  switch ( row )
  {
    case SET_GFX:
      settings.gfx_mode = !settings.gfx_mode;
      break;
    case SET_MUSIC:
      settings.music_vol += dir * VOL_STEP;
      if ( settings.music_vol < 0 )   settings.music_vol = 0;
      if ( settings.music_vol > 100 ) settings.music_vol = 100;
      SoundManagerSetMusicVolume( settings.music_vol );
      break;
    case SET_SFX:
      settings.sfx_vol += dir * VOL_STEP;
      if ( settings.sfx_vol < 0 )   settings.sfx_vol = 0;
      if ( settings.sfx_vol > 100 ) settings.sfx_vol = 100;
      SoundManagerSetSfxVolume( settings.sfx_vol );
      break;
  }
}

static void st_Save( void )
{
  char buf[64];
  snprintf( buf, sizeof( buf ), "%d\n%d\n%d",
            settings.gfx_mode, settings.music_vol, settings.sfx_vol );
  PersistSave( "settings", buf );
}

static void st_Leave( void )
{
  st_Save();
  a_WidgetCacheFree();
  MainMenuInit();
}

/* --- init --- */

void SettingsInit( void )
{
  app.delegate.logic = st_Logic;
  app.delegate.draw  = st_Draw;

  app.options.scale_factor = 1;

  cursor          = 0;
  back_hovered    = 0;
  prev_mouse_down = 0;
  confirm_state   = ST_NORMAL;
  confirm_cursor  = 1;

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  a_WidgetsInit( "resources/widgets/settings.auf" );
}

/* --- delete save data --- */

static void st_DoDelete( void )
{
  PersistDelete( "settings" );
  PersistDelete( "lore" );
  LoreResetAll();

  /* Reset to defaults */
  settings.gfx_mode  = GFX_IMAGE;
  settings.music_vol = 100;
  settings.sfx_vol   = 100;
  SoundManagerSetMusicVolume( settings.music_vol );
  SoundManagerSetSfxVolume( settings.sfx_vol );
}

/* --- confirmation modal logic --- */

static void st_ConfirmLogic( void )
{
  /* ESC - cancel */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_AudioPlaySound( &sfx_click, NULL );
    confirm_state = ST_NORMAL;
    return;
  }

  /* Left / Right - switch Yes/No */
  if ( app.keyboard[A_A] == 1 || app.keyboard[A_LEFT] == 1 )
  {
    app.keyboard[A_A] = 0;
    app.keyboard[A_LEFT] = 0;
    confirm_cursor = 0;
    a_AudioPlaySound( &sfx_move, NULL );
  }
  if ( app.keyboard[A_D] == 1 || app.keyboard[A_RIGHT] == 1 )
  {
    app.keyboard[A_D] = 0;
    app.keyboard[A_RIGHT] = 0;
    confirm_cursor = 1;
    a_AudioPlaySound( &sfx_move, NULL );
  }

  /* Enter / Space */
  int activate = 0;
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 ||
       app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    activate = 1;
  }

  /* Mouse */
  int mx = app.mouse.x;
  int my = app.mouse.y;
  int mouse_down = app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT;
  int clicked = mouse_down && !prev_mouse_down;
  prev_mouse_down = mouse_down;

  /* Hit test on Yes/No buttons */
  float mw = 240.0f, mh = 100.0f;
  float mpx = ( SCREEN_WIDTH - mw ) / 2.0f;
  float mpy = ( SCREEN_HEIGHT - mh ) / 2.0f;
  float btn_w = 80.0f, btn_h = 32.0f;
  float gap = 20.0f;
  float total_w = btn_w * 2 + gap;
  float bx = mpx + ( mw - total_w ) / 2.0f;
  float by = mpy + mh - btn_h - 12;

  if ( PointInRect( mx, my, bx, by, btn_w, btn_h ) )
  {
    confirm_cursor = 0;
    if ( clicked ) activate = 1;
  }
  if ( PointInRect( mx, my, bx + btn_w + gap, by, btn_w, btn_h ) )
  {
    confirm_cursor = 1;
    if ( clicked ) activate = 1;
  }

  if ( !activate ) return;

  a_AudioPlaySound( &sfx_click, NULL );

  if ( confirm_cursor == 1 ) /* No */
  {
    confirm_state = ST_NORMAL;
    return;
  }

  /* Yes */
  if ( confirm_state == ST_CONFIRM_1 )
  {
    confirm_state = ST_CONFIRM_2;
    confirm_cursor = 1; /* default No again */
  }
  else /* ST_CONFIRM_2 - actually delete */
  {
    st_DoDelete();
    confirm_state = ST_NORMAL;
  }
}

/* --- logic --- */

static void st_Logic( float dt )
{
  a_DoInput();

  /* Confirmation modal takes priority */
  if ( confirm_state != ST_NORMAL )
  {
    st_ConfirmLogic();
    return;
  }

  /* ESC - save and leave */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_AudioPlaySound( &sfx_click, NULL );
    st_Leave();
    return;
  }

  /* Hot reload */
  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/settings.auf" );
  }

  /* Up / Down - move cursor */
  if ( app.keyboard[A_W] == 1 || app.keyboard[A_UP] == 1 )
  {
    app.keyboard[A_W] = 0;
    app.keyboard[A_UP] = 0;
    cursor = ( cursor - 1 + NUM_SETTINGS ) % NUM_SETTINGS;
    a_AudioPlaySound( &sfx_move, NULL );
  }

  if ( app.keyboard[A_S] == 1 || app.keyboard[A_DOWN] == 1 )
  {
    app.keyboard[A_S] = 0;
    app.keyboard[A_DOWN] = 0;
    cursor = ( cursor + 1 ) % NUM_SETTINGS;
    a_AudioPlaySound( &sfx_move, NULL );
  }

  /* Left / Right - adjust value (skip delete row) */
  if ( cursor != SET_DELETE )
  {
    if ( app.keyboard[A_A] == 1 || app.keyboard[A_LEFT] == 1 )
    {
      app.keyboard[A_A] = 0;
      app.keyboard[A_LEFT] = 0;
      st_Adjust( cursor, -1 );
      a_AudioPlaySound( &sfx_click, NULL );
    }

    if ( app.keyboard[A_D] == 1 || app.keyboard[A_RIGHT] == 1 )
    {
      app.keyboard[A_D] = 0;
      app.keyboard[A_RIGHT] = 0;
      st_Adjust( cursor, 1 );
      a_AudioPlaySound( &sfx_click, NULL );
    }
  }

  /* Enter / Space - toggle/activate */
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 ||
       app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;

    if ( cursor == SET_GFX )
    {
      st_Adjust( cursor, 1 );
      a_AudioPlaySound( &sfx_click, NULL );
    }
    else if ( cursor == SET_DELETE )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      confirm_state = ST_CONFIRM_1;
      confirm_cursor = 1;
    }
  }

  /* ---- Mouse ---- */
  int mx = app.mouse.x;
  int my = app.mouse.y;
  int mouse_down = app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT;
  int clicked = mouse_down && !prev_mouse_down;
  prev_mouse_down = mouse_down;

  /* Back button */
  {
    aContainerWidget_t* bc = a_GetContainerFromWidget( "settings_back" );
    aRectf_t br = bc->rect;
    int hit = PointInRect( mx, my, br.x, br.y, br.w, br.h );

    if ( hit && !back_hovered )
      a_AudioPlaySound( &sfx_move, NULL );
    back_hovered = hit;

    if ( hit && clicked )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      st_Leave();
      return;
    }
  }

  /* Setting rows */
  {
    aContainerWidget_t* pc = a_GetContainerFromWidget( "settings_panel" );
    aRectf_t r = pc->rect;
    float by = r.y + 16;

    for ( int i = 0; i < NUM_SETTINGS; i++ )
    {
      float bx  = r.x + 4;
      float byi = by + i * ( ITEM_H + ITEM_SPACING );

      int hit = PointInRect( mx, my, bx, byi, r.w - 8, ITEM_H );
      if ( hit )
      {
        if ( cursor != i )
        {
          cursor = i;
          a_AudioPlaySound( &sfx_move, NULL );
        }

        if ( clicked )
        {
          if ( i == SET_DELETE )
          {
            a_AudioPlaySound( &sfx_click, NULL );
            confirm_state = ST_CONFIRM_1;
            confirm_cursor = 1;
          }
          else
          {
            float mid = bx + ( r.w - 8 ) / 2.0f;
            int dir = ( mx < mid ) ? -1 : 1;
            st_Adjust( i, dir );
            a_AudioPlaySound( &sfx_click, NULL );
          }
        }
      }
    }
  }
}

/* --- draw --- */

static void st_Draw( float dt )
{
  aColor_t bg_norm  = { 0x10, 0x14, 0x1f, 255 };
  aColor_t bg_hover = { 0x20, 0x2e, 0x37, 255 };
  aColor_t fg_norm  = { 0x81, 0x97, 0x96, 255 };
  aColor_t fg_hover = { 0xc7, 0xcf, 0xcc, 255 };
  aColor_t gold     = { 0xde, 0x9e, 0x41, 255 };
  aColor_t dim      = { 0x81, 0x97, 0x96, 120 };

  /* Title */
  {
    aContainerWidget_t* tc = a_GetContainerFromWidget( "settings_title" );
    aRectf_t tr = tc->rect;

    aTextStyle_t ts = a_default_text_style;
    ts.fg    = gold;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 2.0f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( "Settings", (int)( tr.x + tr.w / 2.0f ),
                (int)( tr.y + tr.h / 2.0f ), ts );
  }

  /* Panel */
  {
    aContainerWidget_t* pc = a_GetContainerFromWidget( "settings_panel" );
    aRectf_t r = pc->rect;

    a_DrawFilledRect( r, (aColor_t){ 0x08, 0x0a, 0x10, 200 } );

    float by = r.y + 16;
    for ( int i = 0; i < NUM_SETTINGS; i++ )
    {
      int sel = ( cursor == i );
      float bx  = r.x + 4;
      float byi = by + i * ( ITEM_H + ITEM_SPACING );
      float bw  = r.w - 8;

      /* Row background */
      aRectf_t row_r = { bx, byi, bw, ITEM_H };
      a_DrawFilledRect( row_r, sel ? bg_hover : bg_norm );

      aTextStyle_t ts = a_default_text_style;
      ts.bg    = (aColor_t){ 0, 0, 0, 0 };
      ts.scale = 1.2f;

      if ( i == SET_DELETE )
      {
        /* Delete row - red label, centered, no value arrows */
        aColor_t red = { 0xa5, 0x30, 0x30, 255 };
        aColor_t red_bright = { 0xcf, 0x57, 0x3c, 255 };
        ts.fg    = sel ? red_bright : red;
        ts.align = TEXT_ALIGN_CENTER;
        a_DrawText( setting_labels[i],
                    (int)( bx + bw / 2 ),
                    (int)( byi + ITEM_H / 2.0f - 6 ), ts );
      }
      else
      {
        /* Label - left aligned */
        ts.fg    = sel ? fg_hover : fg_norm;
        ts.align = TEXT_ALIGN_LEFT;
        a_DrawText( setting_labels[i],
                    (int)( bx + 8 ),
                    (int)( byi + ITEM_H / 2.0f - 6 ), ts );

        /* Value - centered in right half with arrows */
        char vbuf[16];
        const char* val = st_GetValueStr( i, vbuf, sizeof( vbuf ) );
        char display[32];
        snprintf( display, sizeof( display ), "< %s >", val );

        ts.fg    = sel ? gold : dim;
        ts.align = TEXT_ALIGN_CENTER;
        a_DrawText( display,
                    (int)( bx + bw * 5 / 8 - 76 ),
                    (int)( byi + ITEM_H / 2.0f - 6 ), ts );
      }
    }

    /* Panel border */
    a_DrawRect( r, gold );
  }

  /* Back button */
  {
    aContainerWidget_t* bc = a_GetContainerFromWidget( "settings_back" );
    aRectf_t br = bc->rect;
    DrawButton( br.x, br.y, br.w, br.h, "Back [ESC]", 1.4f, back_hovered,
                bg_norm, bg_hover, fg_norm, fg_hover );
  }

  /* Hint */
  {
    aContainerWidget_t* hc = a_GetContainerFromWidget( "settings_hint" );
    aRectf_t hr = hc->rect;
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = dim;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( "[LEFT/RIGHT] Adjust  [ESC] Back",
                (int)( hr.x + hr.w / 2.0f ),
                (int)( hr.y + hr.h / 2.0f ), ts );
  }

  /* Confirmation modal overlay */
  if ( confirm_state != ST_NORMAL )
  {
    /* Darken background */
    a_DrawFilledRect( (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                      (aColor_t){ 0, 0, 0, 160 } );

    float mw = 240.0f, mh = 100.0f;
    float mpx = ( SCREEN_WIDTH - mw ) / 2.0f;
    float mpy = ( SCREEN_HEIGHT - mh ) / 2.0f;

    /* Modal panel */
    aRectf_t mr = { mpx, mpy, mw, mh };
    a_DrawFilledRect( mr, (aColor_t){ 0x08, 0x0a, 0x10, 240 } );
    a_DrawRect( mr, gold );

    /* Question text */
    const char* question = ( confirm_state == ST_CONFIRM_1 )
                           ? "Are you sure?"
                           : "Like, actually?";
    aTextStyle_t ts = a_default_text_style;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.fg    = fg_hover;
    ts.scale = 1.4f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( question,
                (int)( mpx + mw / 2.0f ),
                (int)( mpy + 20 ), ts );

    /* Yes / No buttons */
    float btn_w = 80.0f, btn_h = 32.0f;
    float gap = 20.0f;
    float total_w = btn_w * 2 + gap;
    float bx = mpx + ( mw - total_w ) / 2.0f;
    float by = mpy + mh - btn_h - 12;

    aColor_t red       = { 0xa5, 0x30, 0x30, 255 };
    aColor_t red_hover = { 0xcf, 0x57, 0x3c, 255 };

    DrawButton( bx, by, btn_w, btn_h, "Yes", 1.3f,
                confirm_cursor == 0,
                bg_norm, bg_hover, red, red_hover );
    DrawButton( bx + btn_w + gap, by, btn_w, btn_h, "No", 1.3f,
                confirm_cursor == 1,
                bg_norm, bg_hover, fg_norm, fg_hover );
  }
}
