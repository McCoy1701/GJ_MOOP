#include <Archimedes.h>
#include "context_menu.h"

#define CTX_BG      (aColor_t){ 20, 20, 20, 255 }
#define CTX_BORDER  (aColor_t){ 80, 80, 80, 255 }
#define CTX_SEL_BG  (aColor_t){ 40, 40, 40, 255 }
#define CTX_SEL_FG  (aColor_t){ 218, 175, 32, 255 }


float DrawContextMenu( float x, float y,
                       const char** labels, int count,
                       int cursor )
{
  float ah = count * ( CTX_MENU_ROW_H + CTX_MENU_PAD ) - CTX_MENU_PAD;

  a_DrawFilledRect( (aRectf_t){ x, y, CTX_MENU_W, ah }, CTX_BG );
  a_DrawRect( (aRectf_t){ x, y, CTX_MENU_W, ah }, CTX_BORDER );

  aTextStyle_t ts = a_default_text_style;
  ts.bg    = (aColor_t){ 0, 0, 0, 0 };
  ts.scale = CTX_MENU_TEXT_S;
  ts.align = TEXT_ALIGN_LEFT;

  for ( int i = 0; i < count; i++ )
  {
    float ry = y + i * ( CTX_MENU_ROW_H + CTX_MENU_PAD );
    aRectf_t row = { x + 2, ry, CTX_MENU_W - 4, CTX_MENU_ROW_H };

    if ( i == cursor )
    {
      a_DrawFilledRect( row, CTX_SEL_BG );
      a_DrawRect( row, CTX_SEL_FG );
      ts.fg = CTX_SEL_FG;
    }
    else
    {
      ts.fg = white;
    }

    a_DrawText( labels[i], (int)( row.x + 8 ), (int)( ry + 5 ), ts );
  }

  return ah;
}
