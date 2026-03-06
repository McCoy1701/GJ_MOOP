#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "shop_ui.h"
#include "shop.h"
#include "items.h"
#include "player.h"
#include "draw_utils.h"
#include "console.h"

extern Player_t player;

#define SHOP_BG   (aColor_t){ 0x09, 0x0a, 0x14, 230 }
#define SHOP_FG   (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }
#define SHOP_GOLD (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define SHOP_RED  (aColor_t){ 0xcf, 0x57, 0x3c, 255 }
#define SHOP_PAD      10.0f
#define SHOP_LINE     18.0f
#define PORTRAIT_SZ   64.0f
#define PORTRAIT_PAD  10.0f

#define TEXT_OX   (SHOP_PAD + PORTRAIT_SZ + PORTRAIT_PAD)
#define NAME_OY   SHOP_PAD
#define SEP_OY    (NAME_OY + SHOP_LINE + 4.0f)
#define TEXT_OY   (SEP_OY + 7.0f)

static int shop_ui_active   = 0;
static int shop_ui_cursor   = 0;    /* 0 = Buy, 1 = Leave */
static int shop_ui_item_idx = -1;
static int shop_ui_can_buy  = 0;

static aSoundEffect_t* sfx_move  = NULL;
static aSoundEffect_t* sfx_click = NULL;
static Console_t*      shop_console = NULL;
static aRectf_t        shop_panel = { 0 };
static float            opts_base_y = 0;

void ShopUIInit( aSoundEffect_t* move, aSoundEffect_t* click, Console_t* con )
{
  sfx_move     = move;
  sfx_click    = click;
  shop_console = con;
  shop_ui_active = 0;
}

static void get_item_info( ShopItem_t* si,
                           const char** name, const char** desc,
                           const char** glyph, aColor_t* color,
                           aImage_t** img )
{
  if ( si->item_type == INV_CONSUMABLE )
  {
    ConsumableInfo_t* ci = &g_consumables[si->item_index];
    *name  = ci->name;
    *desc  = ci->description;
    *glyph = ci->glyph;
    *color = ci->color;
    *img   = ci->image;
  }
  else
  {
    EquipmentInfo_t* ei = &g_equipment[si->item_index];
    *name  = ei->name;
    *desc  = ei->description;
    *glyph = ei->glyph;
    *color = ei->color;
    *img   = NULL;
  }
}

void ShopUIOpen( int item_idx )
{
  if ( item_idx < 0 || item_idx >= g_num_shop_items ) return;
  shop_ui_item_idx = item_idx;
  shop_ui_active   = 1;

  ShopItem_t* si = &g_shop_items[item_idx];
  shop_ui_can_buy = ( player.gold >= si->cost );

  /* Check inventory space */
  int has_space = 0;
  for ( int i = 0; i < player.max_inventory; i++ )
  {
    if ( player.inventory[i].type == INV_EMPTY )
    { has_space = 1; break; }
  }
  if ( !has_space ) shop_ui_can_buy = 0;

  shop_ui_cursor = shop_ui_can_buy ? 0 : 1;

  /* Console info */
  if ( shop_console )
  {
    const char* name; const char* desc; const char* glyph;
    aColor_t color; aImage_t* img;
    get_item_info( si, &name, &desc, &glyph, &color, &img );
    ConsolePushF( shop_console, color, "%s -- %dg", name, si->cost );
    ConsolePushF( shop_console, (aColor_t){ 0xa8, 0xb5, 0xb2, 255 },
                  "  %s", desc );
  }
}

int ShopUIActive( void ) { return shop_ui_active; }

static float opt_y( int i ) { return opts_base_y + i * SHOP_LINE; }
static float opt_x( void )  { return shop_panel.x + TEXT_OX; }
static float opt_w( void )  { return shop_panel.w - TEXT_OX - SHOP_PAD; }

static void do_buy( void )
{
  ShopItem_t* si = &g_shop_items[shop_ui_item_idx];
  const char* name; const char* desc; const char* glyph;
  aColor_t color; aImage_t* img;
  get_item_info( si, &name, &desc, &glyph, &color, &img );

  /* Check inventory space */
  int has_space = 0;
  for ( int i = 0; i < player.max_inventory; i++ )
  {
    if ( player.inventory[i].type == INV_EMPTY )
    { has_space = 1; break; }
  }
  if ( !has_space )
  {
    if ( shop_console )
      ConsolePushF( shop_console, SHOP_RED, "Inventory full!" );
    shop_ui_active = 0;
    return;
  }

  if ( player.gold < si->cost )
  {
    if ( shop_console )
      ConsolePushF( shop_console, SHOP_RED,
                    "Can't afford. Need %dg, have %dg.",
                    si->cost, player.gold );
    shop_ui_active = 0;
    return;
  }

  PlayerSpendGold( si->cost );
  InventoryAdd( si->item_type, si->item_index );
  si->alive = 0;

  if ( shop_console )
    ConsolePushF( shop_console, color,
                  "Bought %s for %dg.", name, si->cost );
  if ( sfx_click ) a_AudioPlaySound( sfx_click, NULL );

  shop_ui_active = 0;
}

int ShopUILogic( void )
{
  if ( !shop_ui_active ) return 0;

  int opt_count = 2; /* Buy, Leave */

  /* Mouse hover on option rows (only when mouse is active input) */
  if ( InputModeGet() == INPUT_MOUSE )
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;
    float ox = opt_x();
    float ow = opt_w();

    for ( int i = 0; i < opt_count; i++ )
    {
      float oy = opt_y( i );
      if ( PointInRect( mx, my, ox - 2, oy - 1, ow + 4, SHOP_LINE ) )
      {
        if ( i != shop_ui_cursor )
        {
          shop_ui_cursor = i;
          if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
        }
        break;
      }
    }
  }

  /* Mouse click */
  if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;
    float ox = opt_x();
    float ow = opt_w();

    for ( int i = 0; i < opt_count; i++ )
    {
      float oy = opt_y( i );
      if ( PointInRect( mx, my, ox - 2, oy - 1, ow + 4, SHOP_LINE ) )
      {
        app.mouse.pressed = 0;
        shop_ui_cursor = i;
        if ( shop_ui_cursor == 0 && shop_ui_can_buy )
          do_buy();
        else
          shop_ui_active = 0;
        return 1;
      }
    }
    app.mouse.pressed = 0;
  }

  /* Scroll wheel */
  if ( app.mouse.wheel != 0 )
  {
    shop_ui_cursor += ( app.mouse.wheel < 0 ) ? 1 : -1;
    if ( shop_ui_cursor < 0 ) shop_ui_cursor = opt_count - 1;
    if ( shop_ui_cursor >= opt_count ) shop_ui_cursor = 0;
    if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
    app.mouse.wheel = 0;
  }

  /* W/S or Up/Down navigation */
  if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    shop_ui_cursor--;
    if ( shop_ui_cursor < 0 ) shop_ui_cursor = opt_count - 1;
    if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
  }
  if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    shop_ui_cursor++;
    if ( shop_ui_cursor >= opt_count ) shop_ui_cursor = 0;
    if ( sfx_move ) a_AudioPlaySound( sfx_move, NULL );
  }

  /* Number keys 1-2 */
  if ( app.keyboard[SDL_SCANCODE_1] == 1 )
  {
    app.keyboard[SDL_SCANCODE_1] = 0;
    if ( shop_ui_can_buy )
      do_buy();
    else
      shop_ui_active = 0;
    return 1;
  }
  if ( app.keyboard[SDL_SCANCODE_2] == 1 )
  {
    app.keyboard[SDL_SCANCODE_2] = 0;
    shop_ui_active = 0;
    return 1;
  }

  /* Y key - buy shortcut */
  if ( app.keyboard[SDL_SCANCODE_Y] == 1 )
  {
    app.keyboard[SDL_SCANCODE_Y] = 0;
    if ( shop_ui_can_buy )
      do_buy();
    else
      shop_ui_active = 0;
    return 1;
  }

  /* N key - leave shortcut */
  if ( app.keyboard[SDL_SCANCODE_N] == 1 )
  {
    app.keyboard[SDL_SCANCODE_N] = 0;
    shop_ui_active = 0;
    return 1;
  }

  /* Enter/Space - confirm */
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 || app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    if ( shop_ui_cursor == 0 && shop_ui_can_buy )
      do_buy();
    else
      shop_ui_active = 0;
    return 1;
  }

  /* ESC - close */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    shop_ui_active = 0;
    return 1;
  }

  /* Consume movement keys */
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

void ShopUIDraw( aRectf_t panel_rect )
{
  if ( !shop_ui_active ) return;
  if ( shop_ui_item_idx < 0 || shop_ui_item_idx >= g_num_shop_items ) return;

  ShopItem_t* si = &g_shop_items[shop_ui_item_idx];
  const char* name; const char* desc; const char* glyph;
  aColor_t color; aImage_t* img;
  get_item_info( si, &name, &desc, &glyph, &color, &img );

  /* Compute content height */
  float text_area_w = panel_rect.w - TEXT_OX - SHOP_PAD;

  int text_h = a_GetWrappedTextHeight( (char*)desc,
                                        a_default_text_style.type,
                                        (int)text_area_w );
  if ( text_h < (int)SHOP_LINE ) text_h = (int)SHOP_LINE;

  float cost_line_h = SHOP_LINE + 4.0f;
  float content_h = TEXT_OY
                   + text_h + 4.0f
                   + cost_line_h
                   + 2 * SHOP_LINE
                   + SHOP_PAD;

  if ( content_h < panel_rect.h ) content_h = panel_rect.h;

  /* Anchor to bottom, expand upward */
  float bottom = panel_rect.y + panel_rect.h;
  aRectf_t r = {
    panel_rect.x,
    bottom - content_h,
    panel_rect.w,
    content_h
  };

  shop_panel = r;

  a_DrawFilledRect( r, SHOP_BG );
  a_DrawRect( r, color );

  /* Portrait */
  float px = r.x + SHOP_PAD;
  float py = r.y + SHOP_PAD;
  aRectf_t port = { px, py, PORTRAIT_SZ, PORTRAIT_SZ };

  a_DrawFilledRect( port, (aColor_t){ 0x05, 0x06, 0x0a, 255 } );
  a_DrawRect( port, SHOP_FG );

  if ( img && settings.gfx_mode == GFX_IMAGE )
  {
    float scale = PORTRAIT_SZ / img->rect.w;
    a_BlitRect( img, NULL, &(aRectf_t){ px, py, img->rect.w, img->rect.h }, scale );
  }
  else if ( glyph && glyph[0] )
  {
    a_DrawGlyph( glyph, (int)px, (int)py,
                 (int)PORTRAIT_SZ, (int)PORTRAIT_SZ,
                 color, (aColor_t){ 0, 0, 0, 0 },
                 FONT_CODE_PAGE_437 );
  }

  /* Text area */
  float x = r.x + TEXT_OX;
  float y = r.y + NAME_OY;

  /* Item name in its color */
  {
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = color;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.2f;
    ts.align = TEXT_ALIGN_LEFT;
    a_DrawText( name, (int)x, (int)y, ts );
  }

  /* Separator */
  y = r.y + SEP_OY;
  a_DrawFilledRect( (aRectf_t){ x, y, text_area_w, 1 }, SHOP_FG );

  /* Description - wrapped */
  y = r.y + TEXT_OY;
  {
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = SHOP_FG;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_LEFT;
    ts.wrap_width = (int)text_area_w;
    a_DrawText( desc, (int)x, (int)y, ts );
  }

  /* Cost line */
  float cost_y = r.y + TEXT_OY + text_h + 4.0f;
  {
    char buf[64];
    snprintf( buf, sizeof( buf ), "Cost: %dg", si->cost );
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = SHOP_GOLD;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_LEFT;
    a_DrawText( buf, (int)x, (int)cost_y, ts );

    snprintf( buf, sizeof( buf ), "Your gold: %dg", player.gold );
    ts.fg = ( player.gold >= si->cost ) ? SHOP_GOLD : SHOP_RED;
    a_DrawText( buf, (int)( x + 120 ), (int)cost_y, ts );
  }

  /* Options */
  opts_base_y = cost_y + cost_line_h;

  const char* labels[2];
  {
    static char buy_buf[32];
    snprintf( buy_buf, sizeof( buy_buf ), "Buy (%dg)", si->cost );
    labels[0] = shop_ui_can_buy ? buy_buf : "Can't afford";
    labels[1] = "Leave it";
  }

  for ( int i = 0; i < 2; i++ )
  {
    float oy = opt_y( i );
    int hovered = ( i == shop_ui_cursor );
    aTextStyle_t ts = a_default_text_style;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_LEFT;

    if ( i == 0 && !shop_ui_can_buy )
      ts.fg = (aColor_t){ 0x60, 0x60, 0x60, 255 };
    else
      ts.fg = SHOP_FG;

    char buf[64];
    snprintf( buf, sizeof( buf ), "%d. %s", i + 1, labels[i] );

    if ( hovered )
    {
      a_DrawFilledRect( (aRectf_t){ x - 2, oy - 1, opt_w() + 4, SHOP_LINE },
                        (aColor_t){ 0xde, 0x9e, 0x41, 30 } );
    }

    a_DrawText( buf, (int)x, (int)oy, ts );
  }
}
