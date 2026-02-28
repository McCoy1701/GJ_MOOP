#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "transitions.h"

extern Player_t player;

/* Top bar layout */
#define TB_PAD_X        12.0f
#define TB_PAD_Y         8.0f
#define TB_NAME_SCALE    1.6f
#define TB_STAT_SCALE    1.2f
#define TB_STAT_GAP     12.0f   /* gap between each stat token */
#define TB_SECTION_GAP  32.0f   /* gap between name and stats */
#define TB_ESC_SCALE     1.0f

#define PANEL_BG  (aColor_t){ 0, 0, 0, 200 }
#define PANEL_FG  (aColor_t){ 255, 255, 255, 255 }

void HUDDrawTopBar( void )
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
