#include <Archimedes.h>

#include "victory.h"
#include "defines.h"
#include "draw_utils.h"

/* States */
enum { V_INACTIVE, V_DELAY, V_ACTIVE };

static int   state       = V_INACTIVE;
static float delay_timer = 0.0f;
static float fade_alpha  = 0.0f;
static int   btn_hovered = 0;

/* Timing */
#define V_DELAY_SECS  0.4f
#define V_FADE_SECS   0.4f

/* Layout */
#define V_PANEL_W     320.0f
#define V_BTN_H        36.0f
#define V_PADDING      16.0f

/* Palette */
#define BG_NORM   (aColor_t){ 0x10, 0x14, 0x1f, 255 }
#define BG_HOVER  (aColor_t){ 0x20, 0x2e, 0x37, 255 }
#define FG_NORM   (aColor_t){ 0x81, 0x97, 0x96, 255 }
#define FG_HOVER  (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }
#define GOLD      (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define PANEL_BG  (aColor_t){ 0x08, 0x0a, 0x10, 230 }
#define VIC_GOLD  (aColor_t){ 0xde, 0x9e, 0x41, 255 }

void VictoryReset( void )
{
  state       = V_INACTIVE;
  delay_timer = 0.0f;
  fade_alpha  = 0.0f;
  btn_hovered = 0;
}

void VictoryTrigger( void )
{
  if ( state != V_INACTIVE ) return;
  state       = V_DELAY;
  delay_timer = V_DELAY_SECS;
  fade_alpha  = 0.0f;
}

int VictoryActive( void )
{
  return state != V_INACTIVE;
}

int VictoryLogic( float dt )
{
  if ( state == V_INACTIVE ) return 0;

  if ( state == V_DELAY )
  {
    delay_timer -= dt;
    if ( delay_timer <= 0.0f )
    {
      state       = V_ACTIVE;
      fade_alpha  = 0.0f;
      btn_hovered = 0;
    }
    return 1;
  }

  /* Fade in */
  if ( fade_alpha < 1.0f )
  {
    fade_alpha += dt / V_FADE_SECS;
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

  /* Consume ESC */
  app.keyboard[SDL_SCANCODE_ESCAPE] = 0;

  /* Mouse hit test on button */
  float panel_h = V_BTN_H + V_PADDING * 2;
  float px = ( SCREEN_WIDTH  - V_PANEL_W ) / 2.0f;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f + 40.0f;
  float bx = px + 8;
  float by = py + V_PADDING;
  float bw = V_PANEL_W - 16;

  btn_hovered = PointInRect( app.mouse.x, app.mouse.y,
                             bx, by, bw, V_BTN_H );

  if ( btn_hovered && app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    return 2;

  return 1;
}

void VictoryDraw( void )
{
  if ( state != V_ACTIVE ) return;

  /* Dark overlay */
  int ov_alpha = (int)( 180.0f * fade_alpha );
  a_DrawFilledRect(
    (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
    (aColor_t){ 0, 0, 0, ov_alpha }
  );

  if ( fade_alpha < 0.1f ) return;

  /* Title */
  aTextStyle_t ts = a_default_text_style;
  ts.bg    = (aColor_t){ 0, 0, 0, 0 };
  ts.fg    = VIC_GOLD;
  ts.scale = 2.4f;
  ts.align = TEXT_ALIGN_CENTER;

  float panel_h = V_BTN_H + V_PADDING * 2;
  float py = ( SCREEN_HEIGHT - panel_h ) / 2.0f + 40.0f;

  a_DrawText( "Victory!",
              (int)( SCREEN_WIDTH / 2.0f ), (int)( py - 56 ), ts );

  /* Subtitle */
  ts.fg    = FG_HOVER;
  ts.scale = 1.4f;
  a_DrawText( "Thank you for playing!",
              (int)( SCREEN_WIDTH / 2.0f ), (int)( py - 24 ), ts );

  /* Panel with button */
  float px = ( SCREEN_WIDTH - V_PANEL_W ) / 2.0f;
  aRectf_t pr = { px, py, V_PANEL_W, panel_h };
  a_DrawFilledRect( pr, PANEL_BG );
  a_DrawRect( pr, GOLD );

  DrawButton( px + 8, py + V_PADDING, V_PANEL_W - 16, V_BTN_H,
              "Main Menu", 1.3f, btn_hovered,
              BG_NORM, BG_HOVER, FG_NORM, FG_HOVER );
}
