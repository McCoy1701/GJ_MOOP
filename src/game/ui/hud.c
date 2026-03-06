#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "transitions.h"
#include "draw_utils.h"

extern Player_t player;

/* Top bar layout */
#define TB_PAD_X        12.0f
#define TB_PAD_Y         8.0f
#define TB_NAME_SCALE    1.6f
#define TB_STAT_SCALE    1.2f
#define TB_STAT_GAP     12.0f   /* gap between each stat token */
#define TB_SECTION_GAP  32.0f   /* gap between name and stats */
#define TB_ESC_SCALE     1.0f

#define PANEL_BG  (aColor_t){ 0x09, 0x0a, 0x14, 200 }
#define PANEL_FG  (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }

static int pause_hovered = 0;

/* Passive hover tooltip */
#define MAX_PASSIVES 8
typedef struct { float x, y, w, h; int equip_idx; } PassiveHit_t;
static PassiveHit_t g_passives[MAX_PASSIVES];
static int g_num_passives = 0;
static int g_passive_hover = -1;

static void PassivesClear( void ) { g_num_passives = 0; g_passive_hover = -1; }

static int FindEquipByEffect( const char* effect )
{
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    if ( player.equipment[i] < 0 ) continue;
    if ( strcmp( g_equipment[player.equipment[i]].effect, effect ) == 0 )
      return player.equipment[i];
  }
  return -1;
}

static void PassiveAdd( float x, float y, float w, float h, int equip_idx )
{
  if ( g_num_passives < MAX_PASSIVES )
  {
    g_passives[g_num_passives++] = (PassiveHit_t){ x, y, w, h, equip_idx };
  }
}

static int PassivesFindHover( void )
{
  int mx = app.mouse.x, my = app.mouse.y;
  for ( int i = 0; i < g_num_passives; i++ )
  {
    PassiveHit_t* p = &g_passives[i];
    if ( PointInRect( mx, my, p->x, p->y, p->w, p->h ) )
      return i;
  }
  return -1;
}

/* Modal dimensions */
#define HUD_MODAL_MIN_W   200.0f
#define HUD_MODAL_PAD_X   10.0f
#define HUD_MODAL_PAD_Y    8.0f
#define HUD_MODAL_NAME_S   1.4f
#define HUD_MODAL_TEXT_S   1.1f
#define HUD_MODAL_DESC_S   1.0f
#define HUD_MODAL_LINE_SM  18.0f
#define HUD_MODAL_LINE_MD  22.0f
#define HUD_MODAL_LINE_LG  24.0f
#define HUD_MODAL_H       130.0f

int HUDDrawTopBar( int in_combat )
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

  /* Player name - left side, larger */
  ts.fg = (aColor_t){ 0xeb, 0xed, 0xe9, 255 };
  ts.scale = TB_NAME_SCALE;
  ts.align = TEXT_ALIGN_LEFT;
  a_DrawText( player.name, (int)tx, (int)ty, ts );

  /* Measure name width to place stats right after it */
  float name_w = strlen( player.name ) * 8.0f * TB_NAME_SCALE;
  float sx = tx + name_w + TB_SECTION_GAP;

  /* Stats - flexed right of name, smaller */
  ts.scale = TB_STAT_SCALE;
  ts.align = TEXT_ALIGN_LEFT;
  char buf[48];

  /* HP - label near-white, numbers colored by percentage */
  ts.fg = (aColor_t){ 0xeb, 0xed, 0xe9, 255 };
  a_DrawText( "HP: ", (int)sx, (int)( ty + 4 ), ts );
  float hp_label_w = 4 * 8.0f * TB_STAT_SCALE;

  float hp_pct = ( player.max_hp > 0 ) ? (float)player.hp / player.max_hp : 0;
  if ( hp_pct > 0.5f )       ts.fg = (aColor_t){ 0x75, 0xa7, 0x43, 255 };
  else if ( hp_pct > 0.25f ) ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
  else                        ts.fg = (aColor_t){ 0xa5, 0x30, 0x30, 255 };

  snprintf( buf, sizeof( buf ), "%d/%d", player.hp, player.max_hp );
  a_DrawText( buf, (int)( sx + hp_label_w ), (int)( ty + 4 ), ts );
  sx += hp_label_w + strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DMG */
  snprintf( buf, sizeof( buf ), "DMG: %d", PlayerStat( "damage" ) );
  ts.fg = (aColor_t){ 0xeb, 0xed, 0xe9, 255 };
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DEF */
  snprintf( buf, sizeof( buf ), "DEF: %d", PlayerStat( "defense" ) );
  ts.fg = (aColor_t){ 0xeb, 0xed, 0xe9, 255 };
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* Gold */
  snprintf( buf, sizeof( buf ), "G: %d", player.gold );
  ts.fg = (aColor_t){ 0xda, 0xaf, 0x20, 255 };
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* Equipment effects - gold (skip conditional ones) */
  PassivesClear();
  ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    if ( player.equipment[i] < 0 ) continue;
    EquipmentInfo_t* e = &g_equipment[ player.equipment[i] ];
    if ( strcmp( e->effect, "none" ) == 0 ) continue;
    if ( strcmp( e->effect, "first_strike" ) == 0 ) continue;
    if ( strcmp( e->effect, "scroll_echo" ) == 0 ) continue;
    if ( strcmp( e->effect, "berserk" ) == 0 ) continue;
    if ( strcmp( e->effect, "poison" ) == 0 ) continue;
    if ( strcmp( e->effect, "amplify" ) == 0 ) continue;
    snprintf( buf, sizeof( buf ), "%s(%d)", e->effect, e->effect_value );
    float tw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
    PassiveAdd( sx, ty + 4, tw, TB_STAT_SCALE * 8.0f, player.equipment[i] );
    a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
    sx += tw + TB_STAT_GAP;
  }

  /* first_strike - green, only when ready */
  {
    int fs = PlayerEquipEffect( "first_strike" );
    if ( fs > 0 && player.first_strike_active )
    {
      ts.fg = (aColor_t){ 0x75, 0xa7, 0x43, 255 };
      snprintf( buf, sizeof( buf ), "FIRST STRIKE(%d)", fs );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "first_strike" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* scroll_echo - blue, show progress toward free cast */
  {
    int echo = PlayerEquipEffect( "scroll_echo" );
    if ( echo > 0 )
    {
      ts.fg = (aColor_t){ 0x73, 0xbe, 0xd3, 255 };
      snprintf( buf, sizeof( buf ), "ECHO(%d/%d)", player.scroll_echo_counter, echo );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "scroll_echo" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* berserk - red, show stacks based on missing HP */
  {
    int bsk = PlayerEquipEffect( "berserk" );
    if ( bsk > 0 && player.max_hp > 0 )
    {
      int missing_pct = ( ( player.max_hp - player.hp ) * 100 ) / player.max_hp;
      int stacks = missing_pct / 40;
      if ( stacks > 2 ) stacks = 2;
      ts.fg = ( stacks > 0 )
        ? (aColor_t){ 0xa5, 0x30, 0x30, 255 }
        : (aColor_t){ 0x6e, 0x3b, 0x3b, 255 };
      snprintf( buf, sizeof( buf ), "BERSERK(+%d)", bsk * stacks );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "berserk" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* amplify - blue, always active when equipped */
  {
    int amp = PlayerEquipEffect( "amplify" );
    if ( amp > 0 )
    {
      ts.fg = (aColor_t){ 0x73, 0xbe, 0xd3, 255 };
      snprintf( buf, sizeof( buf ), "AMPLIFY(%d)", amp );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "amplify" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* dodge - green, show counter progress */
  {
    int dodge = PlayerEquipEffect( "dodge" );
    if ( dodge > 0 )
    {
      ts.fg = (aColor_t){ 0x75, 0xa7, 0x43, 255 };
      snprintf( buf, sizeof( buf ), "DODGE(%d/%d)", player.dodge_counter % dodge, dodge );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "dodge" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* armor_break - red, show counter progress */
  {
    int ab = PlayerEquipEffect( "armor_break" );
    if ( ab > 0 )
    {
      ts.fg = (aColor_t){ 0xcf, 0x57, 0x3c, 255 };
      snprintf( buf, sizeof( buf ), "BREAK(%d/%d)", player.attack_counter % ab, ab );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "armor_break" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* mana_shield - blue, always active when equipped */
  {
    int ms = PlayerEquipEffect( "mana_shield" );
    if ( ms > 0 )
    {
      ts.fg = (aColor_t){ 0x73, 0xbe, 0xd3, 255 };
      snprintf( buf, sizeof( buf ), "SHIELD" );
      float pw = strlen( buf ) * 8.0f * TB_STAT_SCALE;
      PassiveAdd( sx, ty + 4, pw, TB_STAT_SCALE * 8.0f, FindEquipByEffect( "mana_shield" ) );
      a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
      sx += pw + TB_STAT_GAP;
    }
  }

  /* Combat indicator - red */
  if ( in_combat )
  {
    ts.fg = (aColor_t){ 0xa5, 0x30, 0x30, 255 };
    ts.scale = TB_STAT_SCALE;
    ts.align = TEXT_ALIGN_LEFT;
    a_DrawText( "IN COMBAT", (int)sx, (int)( ty + 4 ), ts );
    sx += 9 * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;
  }

  /* Pause[ESC] - far right, clickable */
  {
    const char* pause_text = "Pause[ESC]";
    float text_w = strlen( pause_text ) * 8.0f * TB_ESC_SCALE;
    float btn_x = r.x + r.w - TB_PAD_X - text_w;
    float btn_y = ty + 2;
    float btn_h = r.h - TB_PAD_Y * 2;

    int hit = PointInRect( app.mouse.x, app.mouse.y,
                           btn_x, btn_y, text_w, btn_h );
    pause_hovered = hit;

    ts.scale = TB_ESC_SCALE;
    ts.fg = hit ? (aColor_t){ 0xde, 0x9e, 0x41, 255 }
                : (aColor_t){ 0x57, 0x72, 0x77, 255 };
    ts.align = TEXT_ALIGN_RIGHT;
    a_DrawText( pause_text, (int)( r.x + r.w - TB_PAD_X ), (int)( ty + 6 ), ts );

    if ( hit && app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
      return 1;
  }

  /* Passive hover tooltip */
  g_passive_hover = PassivesFindHover();
  if ( g_passive_hover >= 0 )
  {
    PassiveHit_t* ph = &g_passives[g_passive_hover];
    int ei = ph->equip_idx;
    if ( ei >= 0 && ei < g_num_equipment )
    {
      EquipmentInfo_t* e = &g_equipment[ei];

      float tw_name, th_name;
      a_CalcTextDimensions( e->name, a_default_text_style.type, &tw_name, &th_name );
      float mw = tw_name * HUD_MODAL_NAME_S + HUD_MODAL_PAD_X * 2;
      if ( mw < HUD_MODAL_MIN_W ) mw = HUD_MODAL_MIN_W;

      float header_h = HUD_MODAL_PAD_Y + HUD_MODAL_LINE_LG;
      if ( e->damage > 0 )  header_h += HUD_MODAL_LINE_SM;
      if ( e->defense > 0 ) header_h += HUD_MODAL_LINE_SM;
      if ( strcmp( e->effect, "none" ) != 0 )
        header_h += HUD_MODAL_LINE_MD;
      else
        header_h += HUD_MODAL_LINE_SM;
      int wrap_w = (int)( mw - HUD_MODAL_PAD_X * 2 );
      int desc_h = a_GetWrappedTextHeight( e->description,
                                            a_default_text_style.type, wrap_w );
      float mh = header_h + desc_h + HUD_MODAL_PAD_Y;
      if ( mh < HUD_MODAL_H ) mh = HUD_MODAL_H;

      float mx = ph->x;
      float my = r.y + r.h + 4;

      a_DrawFilledRect( (aRectf_t){ mx, my, mw, mh },
                        (aColor_t){ 0x09, 0x0a, 0x14, 255 } );
      a_DrawRect( (aRectf_t){ mx, my, mw, mh }, e->color );

      float modal_ty = my + HUD_MODAL_PAD_Y;
      float modal_tx = mx + HUD_MODAL_PAD_X;

      ts.bg = (aColor_t){ 0, 0, 0, 0 };
      ts.align = TEXT_ALIGN_LEFT;

      ts.fg = e->color;
      ts.scale = HUD_MODAL_NAME_S;
      a_DrawText( e->name, (int)modal_tx, (int)modal_ty, ts );
      modal_ty += HUD_MODAL_LINE_LG;

      ts.fg = (aColor_t){ 0xeb, 0xed, 0xe9, 255 };
      ts.scale = HUD_MODAL_TEXT_S;
      if ( e->damage > 0 )
      {
        snprintf( buf, sizeof( buf ), "DMG: +%d", e->damage );
        a_DrawText( buf, (int)modal_tx, (int)modal_ty, ts );
        modal_ty += HUD_MODAL_LINE_SM;
      }
      if ( e->defense > 0 )
      {
        snprintf( buf, sizeof( buf ), "DEF: +%d", e->defense );
        a_DrawText( buf, (int)modal_tx, (int)modal_ty, ts );
        modal_ty += HUD_MODAL_LINE_SM;
      }
      if ( strcmp( e->effect, "none" ) != 0 )
      {
        ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
        snprintf( buf, sizeof( buf ), "%s (%d)", e->effect, e->effect_value );
        a_DrawText( buf, (int)modal_tx, (int)modal_ty, ts );
        modal_ty += HUD_MODAL_LINE_MD;
      }
      else
      {
        modal_ty += HUD_MODAL_LINE_SM;
      }

      ts.fg = (aColor_t){ 0xa8, 0xb5, 0xb2, 255 };
      ts.scale = HUD_MODAL_DESC_S;
      ts.wrap_width = wrap_w;
      a_DrawText( e->description, (int)modal_tx, (int)modal_ty, ts );
      ts.wrap_width = 0;
    }
  }

  return 0;
}
