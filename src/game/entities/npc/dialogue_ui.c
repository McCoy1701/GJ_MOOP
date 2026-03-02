#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "dialogue.h"
#include "draw_utils.h"

#define DLG_BG    (aColor_t){ 0x09, 0x0a, 0x14, 230 }
#define DLG_FG    (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }
#define DLG_HL    (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define DLG_PAD      10.0f
#define DLG_LINE     18.0f
#define PORTRAIT_SZ  64.0f   /* portrait square size */
#define PORTRAIT_PAD 10.0f   /* gap between portrait and text */

/* Layout offsets from panel top-left */
#define NAME_OY     DLG_PAD
#define SEP_OY      (NAME_OY + DLG_LINE + 4.0f)
#define TEXT_OY     (SEP_OY + 7.0f)

/* X offset for text area (after portrait) */
#define TEXT_OX     (DLG_PAD + PORTRAIT_SZ + PORTRAIT_PAD)

static int dlg_cursor = 0;
static aSoundEffect_t* sfx_move  = NULL;
static aSoundEffect_t* sfx_click = NULL;
static aRectf_t dlg_panel = { 0 };
static float opts_base_y = 0;   /* computed each frame in draw */

void DialogueUIInit( aSoundEffect_t* move, aSoundEffect_t* click )
{
  sfx_move  = move;
  sfx_click = click;
  dlg_cursor = 0;
}

void DialogueUISetRect( aRectf_t rect )
{
  dlg_panel = rect;
}

/* Compute the screen Y for option index i */
static float opt_y( int i )
{
  return opts_base_y + i * DLG_LINE;
}

static float opt_x( void )
{
  return dlg_panel.x + TEXT_OX;
}

static float opt_w( void )
{
  return dlg_panel.w - TEXT_OX - DLG_PAD;
}

int DialogueUILogic( void )
{
  if ( !DialogueActive() ) return 0;

  int count = DialogueGetOptionCount();
  if ( count <= 0 ) return 1;

  /* Mouse hover on option rows */
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;
    float ox = opt_x();
    float ow = opt_w();

    for ( int i = 0; i < count; i++ )
    {
      float oy = opt_y( i );
      if ( PointInRect( mx, my, ox - 2, oy - 1, ow + 4, DLG_LINE ) )
      {
        if ( i != dlg_cursor )
        {
          dlg_cursor = i;
          if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
        }
        break;
      }
    }
  }

  /* Mouse click on option */
  if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;
    float ox = opt_x();
    float ow = opt_w();

    for ( int i = 0; i < count; i++ )
    {
      float oy = opt_y( i );
      if ( PointInRect( mx, my, ox - 2, oy - 1, ow + 4, DLG_LINE ) )
      {
        app.mouse.pressed = 0;
        dlg_cursor = i;
        if ( sfx_click ) a_AudioPlaySound( sfx_click, NULL );
        DialogueSelectOption( i );
        dlg_cursor = 0;
        return 1;
      }
    }
    app.mouse.pressed = 0;
  }

  /* Scroll wheel */
  if ( app.mouse.wheel != 0 )
  {
    dlg_cursor += ( app.mouse.wheel < 0 ) ? 1 : -1;
    if ( dlg_cursor < 0 ) dlg_cursor = count - 1;
    if ( dlg_cursor >= count ) dlg_cursor = 0;
    if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
    app.mouse.wheel = 0;
  }

  /* Navigate with W/S or Up/Down */
  if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    dlg_cursor--;
    if ( dlg_cursor < 0 ) dlg_cursor = count - 1;
    if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
  }
  if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    dlg_cursor++;
    if ( dlg_cursor >= count ) dlg_cursor = 0;
    if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
  }

  /* Number keys 1-4 */
  for ( int k = 0; k < count && k < 4; k++ )
  {
    if ( app.keyboard[SDL_SCANCODE_1 + k] == 1 )
    {
      app.keyboard[SDL_SCANCODE_1 + k] = 0;
      dlg_cursor = k;
      if ( sfx_click ) a_AudioPlaySound( sfx_click, NULL );
      DialogueSelectOption( k );
      dlg_cursor = 0;
      return 1;
    }
  }

  /* Enter/Space - confirm */
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 || app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    if ( sfx_click ) a_AudioPlaySound( sfx_click, NULL );
    DialogueSelectOption( dlg_cursor );
    dlg_cursor = 0;
    return 1;
  }

  /* ESC - close dialogue */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    DialogueEnd();
    dlg_cursor = 0;
    return 1;
  }

  /* Consume movement keys so nothing leaks through */
  app.keyboard[SDL_SCANCODE_W] = 0;
  app.keyboard[SDL_SCANCODE_S] = 0;
  app.keyboard[SDL_SCANCODE_A] = 0;
  app.keyboard[SDL_SCANCODE_D] = 0;
  app.keyboard[SDL_SCANCODE_UP] = 0;
  app.keyboard[SDL_SCANCODE_DOWN] = 0;
  app.keyboard[SDL_SCANCODE_LEFT] = 0;
  app.keyboard[SDL_SCANCODE_RIGHT] = 0;

  return 1;
}

void DialogueUIDraw( aRectf_t panel_rect )
{
  if ( !DialogueActive() ) return;

  /* ---- Compute content height, then grow panel upward from bottom ---- */
  float text_area_w = panel_rect.w - TEXT_OX - DLG_PAD;

  /* Ask Archimedes for the exact wrapped text height in pixels */
  int text_h = a_GetWrappedTextHeight( (char*)DialogueGetText(),
                                        a_default_text_style.type,
                                        (int)text_area_w );
  if ( text_h < DLG_LINE ) text_h = (int)DLG_LINE;

  int num_opts = DialogueGetOptionCount();
  float content_h = TEXT_OY
                   + text_h + 4.0f
                   + num_opts * DLG_LINE
                   + DLG_PAD;

  /* Minimum height: at least as tall as the console panel */
  if ( content_h < panel_rect.h ) content_h = panel_rect.h;

  /* Anchor to bottom of console rect, expand upward */
  float bottom = panel_rect.y + panel_rect.h;
  aRectf_t r = {
    panel_rect.x,
    bottom - content_h,
    panel_rect.w,
    content_h
  };

  dlg_panel = r;

  a_DrawFilledRect( r, DLG_BG );
  a_DrawRect( r, DLG_FG );

  /* ---- Portrait ---- */
  float px = r.x + DLG_PAD;
  float py = r.y + DLG_PAD;
  aRectf_t port = { px, py, PORTRAIT_SZ, PORTRAIT_SZ };

  a_DrawFilledRect( port, (aColor_t){ 0x05, 0x06, 0x0a, 255 } );
  a_DrawRect( port, DLG_FG );

  aImage_t* img = DialogueGetNPCImage();
  if ( img && settings.gfx_mode == GFX_IMAGE )
  {
    float scale = PORTRAIT_SZ / img->rect.w;
    a_BlitRect( img, NULL, &(aRectf_t){ px, py, img->rect.w, img->rect.h }, scale );
  }
  else
  {
    const char* glyph = DialogueGetNPCGlyph();
    if ( glyph && glyph[0] )
    {
      a_DrawGlyph( glyph, (int)px, (int)py,
                   (int)PORTRAIT_SZ, (int)PORTRAIT_SZ,
                   DialogueGetNPCColor(), (aColor_t){ 0, 0, 0, 0 },
                   FONT_CODE_PAGE_437 );
    }
  }

  /* ---- Text area (right of portrait) ---- */
  float x = r.x + TEXT_OX;
  float y = r.y + NAME_OY;

  /* NPC name in their color */
  {
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = DialogueGetNPCColor();
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.2f;
    ts.align = TEXT_ALIGN_LEFT;
    a_DrawText( DialogueGetNPCName(), (int)x, (int)y, ts );
  }

  /* Separator line */
  y = r.y + SEP_OY;
  a_DrawFilledRect( (aRectf_t){ x, y, text_area_w, 1 }, DLG_FG );

  /* Speech text - wrap to available width */
  y = r.y + TEXT_OY;
  {
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = DLG_FG;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_LEFT;
    ts.wrap_width = (int)text_area_w;
    a_DrawText( DialogueGetText(), (int)x, (int)y, ts );
  }

  /* Options positioned below wrapped text */
  opts_base_y = r.y + TEXT_OY + text_h + 4.0f;

  /* Options */
  int count = DialogueGetOptionCount();
  for ( int i = 0; i < count; i++ )
  {
    float oy = opt_y( i );
    int hovered = ( i == dlg_cursor );
    aColor_t opt_col = DialogueGetOptionColor( i );
    aTextStyle_t ts = a_default_text_style;
    if ( opt_col.a > 0 )
      ts.fg = opt_col;
    else
      ts.fg = DLG_FG;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_LEFT;

    char buf[270];
    snprintf( buf, sizeof( buf ), "%d. %s", i + 1, DialogueGetOptionLabel( i ) );

    if ( hovered )
    {
      a_DrawFilledRect( (aRectf_t){ x - 2, oy - 1, opt_w() + 4, DLG_LINE },
                        (aColor_t){ 0xde, 0x9e, 0x41, 30 } );
    }

    a_DrawText( buf, (int)x, (int)oy, ts );
  }
}
