#include <Archimedes.h>

#include "game_over.h"
#include "defines.h"
#include "player.h"
#include "draw_utils.h"

extern Player_t player;

/* States */
enum { GO_INACTIVE, GO_DELAY, GO_ACTIVE };

static int   state       = GO_INACTIVE;
static float delay_timer = 0.0f;
static float fade_alpha  = 0.0f;
static int   btn_hovered = 0;

/* Timing */
#define GO_DELAY_SECS  0.8f
#define GO_FADE_SECS   0.3f

/* Layout */
#define GO_PANEL_W     280.0f
#define GO_BTN_H        36.0f
#define GO_PADDING      16.0f

/* Palette */
#define BG_NORM   (aColor_t){ 0x10, 0x14, 0x1f, 255 }
#define BG_HOVER  (aColor_t){ 0x20, 0x2e, 0x37, 255 }
#define FG_NORM   (aColor_t){ 0x81, 0x97, 0x96, 255 }
#define FG_HOVER  (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }
#define GOLD      (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define PANEL_BG  (aColor_t){ 0x08, 0x0a, 0x10, 230 }
#define DEATH_RED (aColor_t){ 0xa5, 0x30, 0x30, 255 }

void GameOverReset( void )
{
  state       = GO_INACTIVE;
  delay_timer = 0.0f;
  fade_alpha  = 0.0f;
  btn_hovered = 0;
}

void GameOverCheck( float dt )
{
  (void)dt;
  if ( state != GO_INACTIVE ) return;

  if ( player.hp <= 0 )
  {
    state       = GO_DELAY;
    delay_timer = GO_DELAY_SECS;
    fade_alpha  = 0.0f;
  }
}

int GameOverActive( void )
{
  return state != GO_INACTIVE;
}

int GameOverLogic( float dt )
{
  if ( state == GO_INACTIVE ) return 0;

  /* Delay phase - block input, wait for VFX to finish */
  if ( state == GO_DELAY )
  {
    delay_timer -= dt;
    if ( delay_timer <= 0.0f )
    {
      state       = GO_ACTIVE;
      fade_alpha  = 0.0f;
      btn_hovered = 0;
    }
    return 1;
  }

  /* Fade in */
  if ( fade_alpha < 1.0f )
  {
    fade_alpha += dt / GO_FADE_SECS;
    if ( fade_alpha > 1.0f ) fade_alpha = 1.0f;
  }

  /* Keyboard: Enter / Space → main menu */
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 ||
       app.keyboard[SDL_SCANCODE_SPACE]  == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE]  = 0;
    return 2;
  }

  /* Consume ESC so pause menu can't open */
  app.keyboard[SDL_SCANCODE_ESCAPE] = 0;

  /* Mouse hit test on button */
  float panel_h = GO_BTN_H + GO_PADDING * 2;
  float px = ( SCREEN_WIDTH  - GO_PANEL_W ) / 2.0f;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f + 20.0f;
  float bx = px + 8;
  float by = py + GO_PADDING;
  float bw = GO_PANEL_W - 16;

  btn_hovered = PointInRect( app.mouse.x, app.mouse.y,
                             bx, by, bw, GO_BTN_H );

  if ( btn_hovered && app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    return 2;

  return 1;
}

void GameOverDraw( void )
{
  if ( state != GO_ACTIVE ) return;

  /* Dark overlay */
  int ov_alpha = (int)( 160.0f * fade_alpha );
  a_DrawFilledRect(
    (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
    (aColor_t){ 0, 0, 0, ov_alpha }
  );

  if ( fade_alpha < 0.1f ) return;

  /* Title */
  aTextStyle_t ts = a_default_text_style;
  ts.bg    = (aColor_t){ 0, 0, 0, 0 };
  ts.fg    = DEATH_RED;
  ts.scale = 2.2f;
  ts.align = TEXT_ALIGN_CENTER;

  float panel_h = GO_BTN_H + GO_PADDING * 2;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f + 20.0f;

  a_DrawText( "You Have Fallen",
              (int)( SCREEN_WIDTH / 2.0f ), (int)( py - 36 ), ts );

  /* Panel with button */
  float px = ( SCREEN_WIDTH - GO_PANEL_W ) / 2.0f;
  aRectf_t pr = { px, py, GO_PANEL_W, panel_h };
  a_DrawFilledRect( pr, PANEL_BG );
  a_DrawRect( pr, GOLD );

  DrawButton( px + 8, py + GO_PADDING, GO_PANEL_W - 16, GO_BTN_H,
              "Main Menu", 1.3f, btn_hovered,
              BG_NORM, BG_HOVER, FG_NORM, FG_HOVER );
}
