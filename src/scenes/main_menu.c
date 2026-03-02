#include <stdio.h>
#include <Archimedes.h>

#include "defines.h"
#include "draw_utils.h"
#include "class_select.h"
#include "lore_scene.h"
#include "settings.h"
#include "sound_manager.h"

static void mm_Logic( float );
static void mm_Draw( float );

#define NUM_BUTTONS   4
#define BTN_H         42.0f
#define BTN_SPACING   14.0f

enum { BTN_PLAY, BTN_LORE, BTN_SETTINGS, BTN_QUIT };

static const char* btn_labels[NUM_BUTTONS] = { "Play", "Lore", "Settings", "Quit" };

static int cursor = 0;
static int hovered[NUM_BUTTONS] = { 0 };

static aSoundEffect_t sfx_hover;
static aSoundEffect_t sfx_click;

void MainMenuInit( void )
{
  app.delegate.logic = mm_Logic;
  app.delegate.draw  = mm_Draw;

  app.options.scale_factor = 1;

  cursor = 0;
  for ( int i = 0; i < NUM_BUTTONS; i++ )
    hovered[i] = 0;

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_hover );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  a_WidgetsInit( "resources/widgets/main_menu.auf" );
  app.active_widget = a_GetWidget( "mm_buttons" );

  SoundManagerPlayMenu();
}

static void mm_Execute( int index )
{
  a_AudioPlaySound( &sfx_click, NULL );

  switch ( index )
  {
    case BTN_PLAY:
      a_WidgetCacheFree();
      ClassSelectInit();
      break;
    case BTN_LORE:
      a_WidgetCacheFree();
      LoreSceneInit();
      break;
    case BTN_SETTINGS:
      a_WidgetCacheFree();
      SettingsInit();
      break;
    case BTN_QUIT:
      app.running = 0;
      break;
  }
}

static void mm_Logic( float dt )
{
  a_DoInput();

  /* ESC - quit */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    app.running = 0;
    return;
  }

  /* Hot reload AUF */
  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/main_menu.auf" );
  }

  /* Keyboard nav */
  if ( app.keyboard[A_W] == 1 || app.keyboard[A_UP] == 1 )
  {
    app.keyboard[A_W] = 0;
    app.keyboard[A_UP] = 0;
    cursor = ( cursor - 1 + NUM_BUTTONS ) % NUM_BUTTONS;
    a_AudioPlaySound( &sfx_hover, NULL );
  }

  if ( app.keyboard[A_S] == 1 || app.keyboard[A_DOWN] == 1 )
  {
    app.keyboard[A_S] = 0;
    app.keyboard[A_DOWN] = 0;
    cursor = ( cursor + 1 ) % NUM_BUTTONS;
    a_AudioPlaySound( &sfx_hover, NULL );
  }

  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 || app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    mm_Execute( cursor );
    return;
  }

  /* Mouse - hit test each button rect */
  aContainerWidget_t* bc = a_GetContainerFromWidget( "mm_buttons" );
  aRectf_t r = bc->rect;
  float btn_w = r.w;
  float total_h = NUM_BUTTONS * BTN_H + ( NUM_BUTTONS - 1 ) * BTN_SPACING;
  float by = r.y + ( r.h - total_h ) / 2.0f;

  for ( int i = 0; i < NUM_BUTTONS; i++ )
  {
    float bx = r.x;
    float byi = by + i * ( BTN_H + BTN_SPACING );

    int hit = PointInRect( app.mouse.x, app.mouse.y, bx, byi, btn_w, BTN_H );

    if ( hit && !hovered[i] )
    {
      cursor = i;
      a_AudioPlaySound( &sfx_hover, NULL );
    }
    hovered[i] = hit;

    if ( hit && app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    {
      mm_Execute( i );
      return;
    }
  }
}

static void mm_Draw( float dt )
{
  /* bg handled by app.background set in init */

  /* Title */
  {
    aContainerWidget_t* tc = a_GetContainerFromWidget( "mm_title" );
    aRectf_t tr = tc->rect;

    aTextStyle_t ts = a_default_text_style;
    ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
    ts.bg = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 2.5f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( "Open Doors Dungeon", (int)( tr.x + tr.w / 2.0f ),
                (int)( tr.y + tr.h / 2.0f ), ts );
  }

  /* Buttons */
  {
    aContainerWidget_t* bc = a_GetContainerFromWidget( "mm_buttons" );
    aRectf_t r = bc->rect;
    float btn_w = r.w;
    float total_h = NUM_BUTTONS * BTN_H + ( NUM_BUTTONS - 1 ) * BTN_SPACING;
    float by = r.y + ( r.h - total_h ) / 2.0f;

    aColor_t bg_norm  = { 0x10, 0x14, 0x1f, 255 };
    aColor_t bg_hover = { 0x20, 0x2e, 0x37, 255 };
    aColor_t fg_norm  = { 0x81, 0x97, 0x96, 255 };
    aColor_t fg_hover = { 0xc7, 0xcf, 0xcc, 255 };

    for ( int i = 0; i < NUM_BUTTONS; i++ )
    {
      float bx = r.x;
      float byi = by + i * ( BTN_H + BTN_SPACING );
      int sel = ( cursor == i );

      DrawButton( bx, byi, btn_w, BTN_H, btn_labels[i], 1.5f, sel,
                  bg_norm, bg_hover, fg_norm, fg_hover );
    }
  }

  /* Version hint */
  {
    aContainerWidget_t* vc = a_GetContainerFromWidget( "mm_version" );
    aRectf_t vr = vc->rect;

    aTextStyle_t ts = a_default_text_style;
    ts.fg = (aColor_t){ 0x81, 0x97, 0x96, 120 };
    ts.bg = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( "v0.1", (int)( vr.x + vr.w / 2.0f ),
                (int)( vr.y + vr.h / 2.0f ), ts );
  }
}
