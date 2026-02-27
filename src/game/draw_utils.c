#include <Archimedes.h>
#include "draw_utils.h"

#define GLYPH_SCALE 2.0f

void DrawImageOrGlyph( aImage_t* img, const char* glyph, aColor_t color,
                       float x, float y, float size )
{
  if ( img )
  {
    float orig_w = img->rect.w;
    float orig_h = img->rect.h;
    float scale = size / orig_w;
    a_BlitRect( img, NULL, &(aRectf_t){ x, y, orig_w, orig_h }, scale );
  }
  else if ( glyph && glyph[0] != '\0' )
  {
    float pad_x = size * 0.1f;
    float pad_y = size * 0.05f;
    a_DrawFilledRect( (aRectf_t){ x - pad_x, y - pad_y, size + pad_x * 2, size + pad_y * 2 }, (aColor_t){ 30, 30, 30, 200 } );
    aTextStyle_t style = a_default_text_style;
    style.fg = color;
    style.bg = (aColor_t){ 0, 0, 0, 0 };
    style.scale = GLYPH_SCALE;
    style.align = TEXT_ALIGN_CENTER;
    a_DrawText( glyph, (int)( x + size / 2.0f ), (int)y, style );
  }
}

void DrawButton( float x, float y, float w, float h,
                 const char* label, float text_scale, int hovered,
                 aColor_t bg, aColor_t bg_hover,
                 aColor_t fg, aColor_t fg_hover )
{
  aColor_t bg_col = hovered ? bg_hover : bg;
  aColor_t fg_col = hovered ? fg_hover : fg;

  a_DrawFilledRect( (aRectf_t){ x, y, w, h }, bg_col );
  a_DrawRect( (aRectf_t){ x, y, w, h }, fg_col );

  aTextStyle_t ts = a_default_text_style;
  ts.fg = fg_col;
  ts.bg = (aColor_t){ 0, 0, 0, 0 };
  ts.scale = text_scale;
  ts.align = TEXT_ALIGN_CENTER;
  a_DrawText( label, (int)( x + w / 2.0f ), (int)( y + h * 0.25f ), ts );
}

int PointInRect( int mx, int my, float rx, float ry, float rw, float rh )
{
  return ( mx >= (int)rx && mx <= (int)( rx + rw ) &&
           my >= (int)ry && my <= (int)( ry + rh ) );
}
