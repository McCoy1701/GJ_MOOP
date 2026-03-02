#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "pause_menu.h"
#include "defines.h"
#include "draw_utils.h"
#include "persist.h"
#include "sound_manager.h"

/* State machine */
enum { PM_CLOSED, PM_MAIN, PM_SETTINGS };

static int state = PM_CLOSED;
static int cursor = 0;
static int prev_mouse_down = 0;
static int requested_exit = 0;

/* Sound effects */
static aSoundEffect_t sfx_move;
static aSoundEffect_t sfx_click;
static int sfx_loaded = 0;

/* Layout constants */
#define PM_PANEL_W    280.0f
#define PM_BTN_H       36.0f
#define PM_BTN_SPACING  8.0f
#define PM_PADDING     16.0f
#define PM_ITEM_H      32.0f
#define PM_VOL_STEP     5

/* Palette - matches settings/main menu */
#define BG_NORM   (aColor_t){ 0x10, 0x14, 0x1f, 255 }
#define BG_HOVER  (aColor_t){ 0x20, 0x2e, 0x37, 255 }
#define FG_NORM   (aColor_t){ 0x81, 0x97, 0x96, 255 }
#define FG_HOVER  (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }
#define GOLD      (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define DIM       (aColor_t){ 0x81, 0x97, 0x96, 120 }
#define PANEL_BG  (aColor_t){ 0x08, 0x0a, 0x10, 230 }
#define OVERLAY   (aColor_t){ 0, 0, 0, 140 }

/* --- Main menu items --- */
#define PM_NUM_BTNS  3
enum { PM_RESUME, PM_SETTINGS_BTN, PM_MAINMENU };
static const char* pm_labels[] = { "Resume", "Settings", "Main Menu" };

/* --- Settings items --- */
#define PM_NUM_SETS  3
#define PM_SET_TOTAL 4  /* 3 settings + back button */
enum { PM_GFX, PM_MUSIC, PM_SFX, PM_BACK };
static const char* pm_set_labels[] = { "Graphics", "Music Volume", "SFX Volume" };

static void load_sfx( void )
{
  if ( sfx_loaded ) return;
  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );
  sfx_loaded = 1;
}

/* --- Settings helpers (mirror settings.c) --- */

static const char* pm_GetValueStr( int row, char* buf, int buflen )
{
  switch ( row )
  {
    case PM_GFX:   return settings.gfx_mode == GFX_IMAGE ? "Image" : "ASCII";
    case PM_MUSIC: snprintf( buf, buflen, "%d%%", settings.music_vol ); return buf;
    case PM_SFX:   snprintf( buf, buflen, "%d%%", settings.sfx_vol );  return buf;
  }
  return "";
}

static void pm_Adjust( int row, int dir )
{
  switch ( row )
  {
    case PM_GFX:
      settings.gfx_mode = !settings.gfx_mode;
      break;
    case PM_MUSIC:
      settings.music_vol += dir * PM_VOL_STEP;
      if ( settings.music_vol < 0 )   settings.music_vol = 0;
      if ( settings.music_vol > 100 ) settings.music_vol = 100;
      SoundManagerSetMusicVolume( settings.music_vol );
      break;
    case PM_SFX:
      settings.sfx_vol += dir * PM_VOL_STEP;
      if ( settings.sfx_vol < 0 )   settings.sfx_vol = 0;
      if ( settings.sfx_vol > 100 ) settings.sfx_vol = 100;
      SoundManagerSetSfxVolume( settings.sfx_vol );
      break;
  }
}

static void pm_SaveSettings( void )
{
  char buf[64];
  snprintf( buf, sizeof( buf ), "%d\n%d\n%d",
            settings.gfx_mode, settings.music_vol, settings.sfx_vol );
  PersistSave( "settings", buf );
}

/* --- Public API --- */

void PauseMenuOpen( void )
{
  load_sfx();
  state = PM_MAIN;
  cursor = 0;
  prev_mouse_down = 0;
  requested_exit = 0;
}

void PauseMenuClose( void )
{
  state = PM_CLOSED;
}

int PauseMenuActive( void )
{
  return state != PM_CLOSED;
}

/* --- Logic --- */

static int pm_MainLogic( void )
{
  /* ESC - close pause menu */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_AudioPlaySound( &sfx_click, NULL );
    PauseMenuClose();
    return 1;
  }

  /* Up / Down */
  if ( app.keyboard[A_W] == 1 || app.keyboard[A_UP] == 1 )
  {
    app.keyboard[A_W] = 0;
    app.keyboard[A_UP] = 0;
    cursor = ( cursor - 1 + PM_NUM_BTNS ) % PM_NUM_BTNS;
    a_AudioPlaySound( &sfx_move, NULL );
  }
  if ( app.keyboard[A_S] == 1 || app.keyboard[A_DOWN] == 1 )
  {
    app.keyboard[A_S] = 0;
    app.keyboard[A_DOWN] = 0;
    cursor = ( cursor + 1 ) % PM_NUM_BTNS;
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

  float total_h = PM_NUM_BTNS * PM_BTN_H + ( PM_NUM_BTNS - 1 ) * PM_BTN_SPACING;
  float panel_h = total_h + PM_PADDING * 2;
  float px = ( SCREEN_WIDTH  - PM_PANEL_W ) / 2.0f;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f;
  float by = py + PM_PADDING;

  for ( int i = 0; i < PM_NUM_BTNS; i++ )
  {
    float byi = by + i * ( PM_BTN_H + PM_BTN_SPACING );
    int hit = PointInRect( mx, my, px + 8, byi, PM_PANEL_W - 16, PM_BTN_H );
    if ( hit )
    {
      if ( cursor != i )
      {
        cursor = i;
        a_AudioPlaySound( &sfx_move, NULL );
      }
      if ( clicked ) activate = 1;
    }
  }

  if ( activate )
  {
    a_AudioPlaySound( &sfx_click, NULL );
    switch ( cursor )
    {
      case PM_RESUME:
        PauseMenuClose();
        break;
      case PM_SETTINGS_BTN:
        state = PM_SETTINGS;
        cursor = 0;
        break;
      case PM_MAINMENU:
        PauseMenuClose();
        return 2; /* signal: exit to main menu */
    }
  }
  return 1;
}

static int pm_SettingsLogic( void )
{
  /* ESC - back to main */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_AudioPlaySound( &sfx_click, NULL );
    pm_SaveSettings();
    state = PM_MAIN;
    cursor = 0;
    return 1;
  }

  /* Up / Down */
  if ( app.keyboard[A_W] == 1 || app.keyboard[A_UP] == 1 )
  {
    app.keyboard[A_W] = 0;
    app.keyboard[A_UP] = 0;
    cursor = ( cursor - 1 + PM_SET_TOTAL ) % PM_SET_TOTAL;
    a_AudioPlaySound( &sfx_move, NULL );
  }
  if ( app.keyboard[A_S] == 1 || app.keyboard[A_DOWN] == 1 )
  {
    app.keyboard[A_S] = 0;
    app.keyboard[A_DOWN] = 0;
    cursor = ( cursor + 1 ) % PM_SET_TOTAL;
    a_AudioPlaySound( &sfx_move, NULL );
  }

  /* Left / Right - adjust (only on setting rows) */
  if ( cursor < PM_NUM_SETS )
  {
    if ( app.keyboard[A_A] == 1 || app.keyboard[A_LEFT] == 1 )
    {
      app.keyboard[A_A] = 0;
      app.keyboard[A_LEFT] = 0;
      pm_Adjust( cursor, -1 );
      a_AudioPlaySound( &sfx_click, NULL );
    }
    if ( app.keyboard[A_D] == 1 || app.keyboard[A_RIGHT] == 1 )
    {
      app.keyboard[A_D] = 0;
      app.keyboard[A_RIGHT] = 0;
      pm_Adjust( cursor, 1 );
      a_AudioPlaySound( &sfx_click, NULL );
    }
  }

  /* Enter / Space - toggle (gfx) or activate (back) */
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 ||
       app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    if ( cursor == PM_GFX )
    {
      pm_Adjust( cursor, 1 );
      a_AudioPlaySound( &sfx_click, NULL );
    }
    else if ( cursor == PM_BACK )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      pm_SaveSettings();
      state = PM_MAIN;
      cursor = 0;
      return 1;
    }
  }

  /* Mouse */
  int mx = app.mouse.x;
  int my = app.mouse.y;
  int mouse_down = app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT;
  int clicked = mouse_down && !prev_mouse_down;
  prev_mouse_down = mouse_down;

  /* Settings rows */
  float total_h = PM_NUM_SETS * PM_ITEM_H + ( PM_NUM_SETS - 1 ) * PM_BTN_SPACING
                  + PM_BTN_H + PM_BTN_SPACING; /* +back button */
  float panel_h = total_h + PM_PADDING * 2;
  float px = ( SCREEN_WIDTH  - PM_PANEL_W ) / 2.0f;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f;
  float ry = py + PM_PADDING;

  for ( int i = 0; i < PM_NUM_SETS; i++ )
  {
    float ryi = ry + i * ( PM_ITEM_H + PM_BTN_SPACING );
    int hit = PointInRect( mx, my, px + 8, ryi, PM_PANEL_W - 16, PM_ITEM_H );
    if ( hit )
    {
      if ( cursor != i )
      {
        cursor = i;
        a_AudioPlaySound( &sfx_move, NULL );
      }
      if ( clicked )
      {
        float mid = px + 8 + ( PM_PANEL_W - 16 ) / 2.0f;
        int dir = ( mx < mid ) ? -1 : 1;
        pm_Adjust( i, dir );
        a_AudioPlaySound( &sfx_click, NULL );
      }
    }
  }

  /* Back button hover + click */
  float back_y = ry + PM_NUM_SETS * ( PM_ITEM_H + PM_BTN_SPACING );
  if ( PointInRect( mx, my, px + 8, back_y, PM_PANEL_W - 16, PM_BTN_H ) )
  {
    if ( cursor != PM_BACK )
    {
      cursor = PM_BACK;
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( clicked )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      pm_SaveSettings();
      state = PM_MAIN;
      cursor = 0;
    }
  }

  return 1;
}

int PauseMenuLogic( void )
{
  if ( state == PM_CLOSED ) return 0;
  a_DoInput();
  if ( state == PM_MAIN )     return pm_MainLogic();
  if ( state == PM_SETTINGS ) return pm_SettingsLogic();
  return 1;
}

/* --- Draw --- */

static void pm_DrawMain( void )
{
  float total_h = PM_NUM_BTNS * PM_BTN_H + ( PM_NUM_BTNS - 1 ) * PM_BTN_SPACING;
  float panel_h = total_h + PM_PADDING * 2;
  float px = ( SCREEN_WIDTH  - PM_PANEL_W ) / 2.0f;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f;

  /* Panel */
  aRectf_t pr = { px, py, PM_PANEL_W, panel_h };
  a_DrawFilledRect( pr, PANEL_BG );
  a_DrawRect( pr, GOLD );

  /* Buttons */
  float by = py + PM_PADDING;
  for ( int i = 0; i < PM_NUM_BTNS; i++ )
  {
    float byi = by + i * ( PM_BTN_H + PM_BTN_SPACING );
    int sel = ( cursor == i );
    DrawButton( px + 8, byi, PM_PANEL_W - 16, PM_BTN_H,
                pm_labels[i], 1.3f, sel,
                BG_NORM, BG_HOVER, FG_NORM, FG_HOVER );
  }
}

static void pm_DrawSettings( void )
{
  float total_h = PM_NUM_SETS * PM_ITEM_H + ( PM_NUM_SETS - 1 ) * PM_BTN_SPACING
                  + PM_BTN_H + PM_BTN_SPACING;
  float panel_h = total_h + PM_PADDING * 2;
  float px = ( SCREEN_WIDTH  - PM_PANEL_W ) / 2.0f;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f;

  /* Panel */
  aRectf_t pr = { px, py, PM_PANEL_W, panel_h };
  a_DrawFilledRect( pr, PANEL_BG );
  a_DrawRect( pr, GOLD );

  /* Setting rows */
  float ry = py + PM_PADDING;
  float rw = PM_PANEL_W - 16;

  for ( int i = 0; i < PM_NUM_SETS; i++ )
  {
    int sel = ( cursor == i );
    float rx = px + 8;
    float ryi = ry + i * ( PM_ITEM_H + PM_BTN_SPACING );

    aRectf_t row_r = { rx, ryi, rw, PM_ITEM_H };
    a_DrawFilledRect( row_r, sel ? BG_HOVER : BG_NORM );

    aTextStyle_t ts = a_default_text_style;
    ts.bg = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.1f;

    /* Label - left */
    ts.fg = sel ? FG_HOVER : FG_NORM;
    ts.align = TEXT_ALIGN_LEFT;
    a_DrawText( pm_set_labels[i], (int)( rx + 6 ), (int)( ryi + PM_ITEM_H / 2.0f ), ts );

    /* Value - centered right half */
    char vbuf[16];
    const char* val = pm_GetValueStr( i, vbuf, sizeof( vbuf ) );
    char display[32];
    snprintf( display, sizeof( display ), "< %s >", val );
    ts.fg = sel ? GOLD : DIM;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( display, (int)( rx + rw * 5 / 8 ), (int)( ryi + PM_ITEM_H / 2.0f ), ts );
  }

  /* Back button */
  float back_y = ry + PM_NUM_SETS * ( PM_ITEM_H + PM_BTN_SPACING );
  DrawButton( px + 8, back_y, rw, PM_BTN_H,
              "Back [ESC]", 1.2f, cursor == PM_BACK,
              BG_NORM, BG_HOVER, FG_NORM, FG_HOVER );
}

void PauseMenuDraw( void )
{
  if ( state == PM_CLOSED ) return;

  /* Dark overlay */
  a_DrawFilledRect( (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT }, OVERLAY );

  /* Title */
  aTextStyle_t ts = a_default_text_style;
  ts.bg = (aColor_t){ 0, 0, 0, 0 };
  ts.fg = GOLD;
  ts.scale = 2.0f;
  ts.align = TEXT_ALIGN_CENTER;

  if ( state == PM_MAIN )
  {
    float total_h = PM_NUM_BTNS * PM_BTN_H + ( PM_NUM_BTNS - 1 ) * PM_BTN_SPACING;
    float panel_h = total_h + PM_PADDING * 2;
    float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f;
    a_DrawText( "Paused", (int)( SCREEN_WIDTH / 2.0f ), (int)( py - 32 ), ts );
    pm_DrawMain();
  }
  else if ( state == PM_SETTINGS )
  {
    float total_h = PM_NUM_SETS * PM_ITEM_H + ( PM_NUM_SETS - 1 ) * PM_BTN_SPACING
                    + PM_BTN_H + PM_BTN_SPACING;
    float panel_h = total_h + PM_PADDING * 2;
    float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f;
    a_DrawText( "Settings", (int)( SCREEN_WIDTH / 2.0f ), (int)( py - 32 ), ts );
    pm_DrawSettings();
  }
}
