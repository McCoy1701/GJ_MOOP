#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "draw_utils.h"
#include "lore.h"
#include "lore_scene.h"
#include "main_menu.h"

static void ls_Logic( float );
static void ls_Draw( float );

#define ITEM_H       32.0f
#define ITEM_SPACING  6.0f
#define HEADER_H     28.0f
#define MAX_CATS      16
#define MAX_SIDEBAR   48

/* Sidebar item: either a floor header or a selectable category */
typedef struct {
  int  is_header;
  int  floor;
  char category[32];
} SidebarItem_t;

static SidebarItem_t sidebar[MAX_SIDEBAR];
static int           num_sidebar;

/* Panel focus: 0 = sidebar, 1 = entries */
static int focus_panel;
static int sidebar_cursor;
static int entry_cursor;

static int back_hovered;

static aSoundEffect_t sfx_move;
static aSoundEffect_t sfx_click;

/* ---- Build sidebar items from lore data ---- */

static void build_sidebar( void )
{
  num_sidebar = 0;

  int floors[MAX_LORE_FLOORS];
  int nf = LoreGetFloors( floors, MAX_LORE_FLOORS );

  for ( int fi = 0; fi < nf && num_sidebar < MAX_SIDEBAR; fi++ )
  {
    /* Floor header */
    sidebar[num_sidebar].is_header = 1;
    sidebar[num_sidebar].floor     = floors[fi];
    sidebar[num_sidebar].category[0] = '\0';
    num_sidebar++;

    /* Categories for this floor */
    char cats[MAX_CATS][32];
    int nc = LoreGetFloorCategories( floors[fi], cats, MAX_CATS );
    for ( int ci = 0; ci < nc && num_sidebar < MAX_SIDEBAR; ci++ )
    {
      sidebar[num_sidebar].is_header = 0;
      sidebar[num_sidebar].floor     = floors[fi];
      strncpy( sidebar[num_sidebar].category, cats[ci], 31 );
      num_sidebar++;
    }
  }
}

/* ---- Sidebar navigation helpers ---- */

static int sidebar_next_selectable( int from, int dir )
{
  int i = from + dir;
  while ( i >= 0 && i < num_sidebar )
  {
    if ( !sidebar[i].is_header ) return i;
    i += dir;
  }
  return from;
}

static int sidebar_first_selectable( void )
{
  for ( int i = 0; i < num_sidebar; i++ )
    if ( !sidebar[i].is_header ) return i;
  return 0;
}

/* ---- Count entries for a floor+category ---- */

static int entries_in_floor_cat( int floor, const char* cat )
{
  int n = 0;
  for ( int i = 0; i < LoreGetCount(); i++ )
  {
    LoreEntry_t* le = LoreGetEntry( i );
    if ( le->floor == floor && strcmp( le->category, cat ) == 0 ) n++;
  }
  return n;
}

static LoreEntry_t* get_entry_in_floor_cat( int floor, const char* cat,
                                             int index )
{
  int n = 0;
  for ( int i = 0; i < LoreGetCount(); i++ )
  {
    LoreEntry_t* le = LoreGetEntry( i );
    if ( le->floor == floor && strcmp( le->category, cat ) == 0 )
    {
      if ( n == index ) return le;
      n++;
    }
  }
  return NULL;
}

void LoreSceneInit( void )
{
  app.delegate.logic = ls_Logic;
  app.delegate.draw  = ls_Draw;

  app.options.scale_factor = 1;

  focus_panel     = 0;
  sidebar_cursor  = 0;
  entry_cursor    = 0;
  back_hovered    = 0;

  build_sidebar();
  sidebar_cursor = sidebar_first_selectable();

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  a_WidgetsInit( "resources/widgets/lore.auf" );
}

static void ls_Leave( void )
{
  a_WidgetCacheFree();
  MainMenuInit();
}

static void ls_Logic( float dt )
{
  (void)dt;
  a_DoInput();

  /* ESC - back to main menu */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    ls_Leave();
    return;
  }

  /* Hot reload */
  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/lore.auf" );
  }

  /* Left/Right - switch panels */
  if ( app.keyboard[A_LEFT] == 1 || app.keyboard[A_RIGHT] == 1 )
  {
    app.keyboard[A_LEFT] = 0;
    app.keyboard[A_RIGHT] = 0;
    focus_panel = !focus_panel;
    a_AudioPlaySound( &sfx_click, NULL );

    if ( focus_panel == 1 && sidebar_cursor >= 0 &&
         sidebar_cursor < num_sidebar && !sidebar[sidebar_cursor].is_header )
    {
      SidebarItem_t* si = &sidebar[sidebar_cursor];
      int count = entries_in_floor_cat( si->floor, si->category );
      if ( entry_cursor >= count )
        entry_cursor = count > 0 ? count - 1 : 0;
    }
  }

  /* Up / Down */
  if ( app.keyboard[A_W] == 1 || app.keyboard[A_UP] == 1 )
  {
    app.keyboard[A_W] = 0;
    app.keyboard[A_UP] = 0;

    if ( focus_panel == 0 )
    {
      sidebar_cursor = sidebar_next_selectable( sidebar_cursor, -1 );
      entry_cursor = 0;
    }
    else
    {
      SidebarItem_t* si = &sidebar[sidebar_cursor];
      int count = entries_in_floor_cat( si->floor, si->category );
      if ( count > 0 )
        entry_cursor = ( entry_cursor - 1 + count ) % count;
    }
    a_AudioPlaySound( &sfx_move, NULL );
  }

  if ( app.keyboard[A_S] == 1 || app.keyboard[A_DOWN] == 1 )
  {
    app.keyboard[A_S] = 0;
    app.keyboard[A_DOWN] = 0;

    if ( focus_panel == 0 )
    {
      sidebar_cursor = sidebar_next_selectable( sidebar_cursor, 1 );
      entry_cursor = 0;
    }
    else
    {
      SidebarItem_t* si = &sidebar[sidebar_cursor];
      int count = entries_in_floor_cat( si->floor, si->category );
      if ( count > 0 )
        entry_cursor = ( entry_cursor + 1 ) % count;
    }
    a_AudioPlaySound( &sfx_move, NULL );
  }

  /* Enter - switch to entries panel if on sidebar */
  if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 ||
       app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;

    if ( focus_panel == 0 )
    {
      focus_panel = 1;
      entry_cursor = 0;
      a_AudioPlaySound( &sfx_click, NULL );
    }
  }

  /* ---- Mouse ---- */

  int mx = app.mouse.x;
  int my = app.mouse.y;
  int clicked = app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT;

  /* Back button */
  {
    aContainerWidget_t* bc = a_GetContainerFromWidget( "lore_back" );
    aRectf_t br = bc->rect;
    int hit = PointInRect( mx, my, br.x, br.y, br.w, br.h );

    if ( hit && !back_hovered )
      a_AudioPlaySound( &sfx_move, NULL );
    back_hovered = hit;

    if ( hit && clicked )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      ls_Leave();
      return;
    }
  }

  /* Sidebar panel - mouse hover + click */
  {
    aContainerWidget_t* cc = a_GetContainerFromWidget( "lore_categories" );
    aRectf_t r = cc->rect;
    float by = r.y + 8;

    for ( int i = 0; i < num_sidebar; i++ )
    {
      float item_h = sidebar[i].is_header ? HEADER_H : ITEM_H;
      float byi = by;
      by += item_h + ITEM_SPACING;

      if ( sidebar[i].is_header ) continue;

      int hit = PointInRect( mx, my, r.x + 4, byi, r.w - 8, item_h );
      if ( hit )
      {
        if ( focus_panel != 0 || sidebar_cursor != i )
        {
          focus_panel = 0;
          if ( sidebar_cursor != i )
          {
            sidebar_cursor = i;
            entry_cursor = 0;
          }
          a_AudioPlaySound( &sfx_move, NULL );
        }

        if ( clicked )
        {
          focus_panel = 1;
          entry_cursor = 0;
          a_AudioPlaySound( &sfx_click, NULL );
        }
      }
    }
  }

  /* Entries panel - mouse hover + click */
  if ( sidebar_cursor >= 0 && sidebar_cursor < num_sidebar &&
       !sidebar[sidebar_cursor].is_header )
  {
    aContainerWidget_t* ec = a_GetContainerFromWidget( "lore_entries" );
    aRectf_t r = ec->rect;
    SidebarItem_t* si = &sidebar[sidebar_cursor];
    int count = entries_in_floor_cat( si->floor, si->category );
    float by = r.y + 8;

    for ( int i = 0; i < count; i++ )
    {
      float byi = by + i * ( ITEM_H + ITEM_SPACING );
      int hit = PointInRect( mx, my, r.x + 4, byi, r.w - 8, ITEM_H );
      if ( hit )
      {
        if ( focus_panel != 1 || entry_cursor != i )
        {
          focus_panel = 1;
          entry_cursor = i;
          a_AudioPlaySound( &sfx_move, NULL );
        }
      }
    }
  }
}

static void ls_Draw( float dt )
{
  (void)dt;
  aColor_t bg_norm  = { 0x10, 0x14, 0x1f, 255 };
  aColor_t bg_hover = { 0x20, 0x2e, 0x37, 255 };
  aColor_t fg_norm  = { 0x81, 0x97, 0x96, 255 };
  aColor_t fg_hover = { 0xc7, 0xcf, 0xcc, 255 };
  aColor_t gold     = { 0xde, 0x9e, 0x41, 255 };
  aColor_t dim      = { 0x81, 0x97, 0x96, 120 };

  /* Title */
  {
    aContainerWidget_t* tc = a_GetContainerFromWidget( "lore_title" );
    aRectf_t tr = tc->rect;

    char title[64];
    snprintf( title, sizeof( title ), "Lore (%d/%d)",
              LoreCountDiscovered(), LoreGetCount() );

    aTextStyle_t ts = a_default_text_style;
    ts.fg    = gold;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 2.0f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( title, (int)( tr.x + tr.w / 2.0f ),
                (int)( tr.y + tr.h / 2.0f ), ts );
  }

  /* Sidebar panel (floors + categories) */
  {
    aContainerWidget_t* cc = a_GetContainerFromWidget( "lore_categories" );
    aRectf_t r = cc->rect;

    a_DrawFilledRect( r, (aColor_t){ 0x08, 0x0a, 0x10, 200 } );

    float by = r.y + 8;
    for ( int i = 0; i < num_sidebar; i++ )
    {
      if ( sidebar[i].is_header )
      {
        /* Floor header - gold text with underline */
        char hdr[32];
        snprintf( hdr, sizeof( hdr ), "Floor %02d", sidebar[i].floor );

        aTextStyle_t hs = a_default_text_style;
        hs.fg    = gold;
        hs.bg    = (aColor_t){ 0, 0, 0, 0 };
        hs.scale = 1.2f;
        hs.align = TEXT_ALIGN_LEFT;
        a_DrawText( hdr, (int)( r.x + 8 ), (int)( by + 6 ), hs );

        aRectf_t line = { r.x + 8, by + HEADER_H - 2, r.w - 16, 1 };
        a_DrawFilledRect( line, gold );

        by += HEADER_H + ITEM_SPACING;
      }
      else
      {
        /* Category button (indented) */
        int sel = ( focus_panel == 0 && sidebar_cursor == i );
        int active = ( sidebar_cursor == i );

        char buf[64];
        int disc = LoreCountInFloorCategory( sidebar[i].floor,
                                              sidebar[i].category );
        int total = entries_in_floor_cat( sidebar[i].floor,
                                           sidebar[i].category );
        snprintf( buf, sizeof( buf ), "  %s (%d/%d)",
                  sidebar[i].category, disc, total );

        DrawButton( r.x + 4, by, r.w - 8, ITEM_H, buf, 1.2f, sel,
                    active ? bg_hover : bg_norm, bg_hover, fg_norm, fg_hover );
        by += ITEM_H + ITEM_SPACING;
      }
    }

    if ( focus_panel == 0 )
      a_DrawRect( r, gold );
  }

  /* Entries panel */
  {
    aContainerWidget_t* ec = a_GetContainerFromWidget( "lore_entries" );
    aRectf_t r = ec->rect;

    a_DrawFilledRect( r, (aColor_t){ 0x08, 0x0a, 0x10, 200 } );

    if ( num_sidebar == 0 )
    {
      aTextStyle_t ts = a_default_text_style;
      ts.fg    = dim;
      ts.bg    = (aColor_t){ 0, 0, 0, 0 };
      ts.scale = 1.2f;
      ts.align = TEXT_ALIGN_CENTER;
      a_DrawText( "No lore discovered yet.",
                  (int)( r.x + r.w / 2.0f ),
                  (int)( r.y + r.h / 2.0f ), ts );
    }
    else if ( sidebar_cursor >= 0 && sidebar_cursor < num_sidebar &&
              !sidebar[sidebar_cursor].is_header )
    {
      SidebarItem_t* si = &sidebar[sidebar_cursor];
      int count = entries_in_floor_cat( si->floor, si->category );

      float by = r.y + 8;
      for ( int i = 0; i < count; i++ )
      {
        LoreEntry_t* le = get_entry_in_floor_cat( si->floor, si->category, i );
        if ( !le ) continue;

        int sel = ( focus_panel == 1 && entry_cursor == i );
        const char* display = le->discovered ? le->title : "???";

        DrawButton( r.x + 4, by, r.w - 8, ITEM_H, display, 1.2f, sel,
                    bg_norm, bg_hover, fg_norm, fg_hover );
        by += ITEM_H + ITEM_SPACING;
      }

      /* Description below entry list */
      if ( count > 0 )
      {
        LoreEntry_t* sel_entry = get_entry_in_floor_cat( si->floor,
                                                          si->category,
                                                          entry_cursor );
        if ( sel_entry && sel_entry->discovered )
        {
          float desc_y = by + 16;

          aRectf_t sep = { r.x + 16, desc_y, r.w - 32, 1 };
          a_DrawFilledRect( sep, dim );

          aTextStyle_t ts = a_default_text_style;
          ts.fg    = fg_hover;
          ts.bg    = (aColor_t){ 0, 0, 0, 0 };
          ts.scale = 1.0f;
          ts.align = TEXT_ALIGN_LEFT;
          ts.wrap_width = (int)( r.w - 32 );
          a_DrawText( sel_entry->description,
                      (int)( r.x + 16 ), (int)( desc_y + 12 ), ts );
        }
      }
    }

    if ( focus_panel == 1 )
      a_DrawRect( r, gold );
  }

  /* Back button */
  {
    aContainerWidget_t* bc = a_GetContainerFromWidget( "lore_back" );
    aRectf_t br = bc->rect;
    DrawButton( br.x, br.y, br.w, br.h, "Back [ESC]", 1.4f, back_hovered,
                bg_norm, bg_hover, fg_norm, fg_hover );
  }

  /* Hint */
  {
    aContainerWidget_t* hc = a_GetContainerFromWidget( "lore_hint" );
    aRectf_t hr = hc->rect;
    aTextStyle_t ts = a_default_text_style;
    ts.fg    = dim;
    ts.bg    = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    ts.align = TEXT_ALIGN_CENTER;
    a_DrawText( "[LEFT/RIGHT] Switch Panel  [ESC] Back",
                (int)( hr.x + hr.w / 2.0f ),
                (int)( hr.y + hr.h / 2.0f ), ts );
  }
}
