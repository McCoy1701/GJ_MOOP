#include <stdio.h>
#include <string.h>
#include <Archimedes.h>
#include "defines.h"
#include "player.h"
#include "items.h"
#include "draw_utils.h"
#include "game_scene.h"
#include "main_menu.h"

static void cs_Logic( float );
static void cs_Draw( float );

static void mercenary_button( void );
static void rogue_button( void );
static void mage_button( void );

#define IMAGE_SCALE       8.0f

/* Item list layout (shared by consumables + doors panels) */
#define LIST_ITEM_SIZE    32.0f
#define LIST_ROW_SPACING  6.0f
#define LIST_PAD_X        8.0f
#define LIST_PAD_Y        4.0f
#define LIST_TITLE_GAP    24.0f   /* gap below "Can Use:" / "Can Open:" label */
#define LIST_TEXT_GAP     8.0f    /* gap between icon and name text */
#define LIST_HIT_MARGIN   2.0f    /* extra px around row for mouse hit detection */

/* Detail modal (consumable info popup) */
#define MODAL_W           260.0f
#define MODAL_H           160.0f
#define MODAL_PAD_X       12.0f
#define MODAL_PAD_Y       10.0f
#define MODAL_NAME_SCALE  1.5f
#define MODAL_TEXT_SCALE  1.2f
#define MODAL_DESC_SCALE  1.1f
#define MODAL_LINE_SM     22.0f
#define MODAL_LINE_MD     26.0f
#define MODAL_LINE_LG     28.0f

/* Embark button */
#define EMBARK_W          360.0f
#define EMBARK_H          72.0f

/* Back button */
#define BACK_W            120.0f
#define BACK_H            30.0f

static int last_class_idx = -1;
static int prev_class_idx = -1;
static int selected_item = 0;
static int browsing_items = 0;

/* Unified filtered list — consumables + openables for current class */
static FilteredItem_t filtered[MAX_CONSUMABLES + MAX_DOORS];
static int num_filtered = 0;

static aSoundEffect_t sfx_hover;
static aSoundEffect_t sfx_click;
static int back_hovered = 0;
static int embark_hovered = 0;

static void cs_RebuildFiltered( void )
{
  num_filtered = ItemsBuildFiltered( last_class_idx, filtered, MAX_CONSUMABLES + MAX_DOORS );
  selected_item = 0;
  browsing_items = 1;
}

static void cs_SelectClass( int index )
{
  strncpy( player.name, g_classes[index].name, MAX_NAME_LENGTH - 1 );
  player.hp = g_classes[index].hp;
  player.max_hp = g_classes[index].hp;
  player.base_damage = g_classes[index].base_damage;
  player.defense = g_classes[index].defense;
  strncpy( player.consumable_type, g_classes[index].consumable_type, MAX_NAME_LENGTH - 1 );
  strncpy( player.description, g_classes[index].description, 255 );
  player.image = g_classes[index].image;
  player.inventory_count = 0;
  player.selected_consumable = 0;
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
    player.equipment[i] = -1;

  a_WidgetCacheFree();
  GameSceneInit();
}

static void cs_BindActions( void )
{
  aContainerWidget_t* class_container = a_GetContainerFromWidget( "class_select" );
  for ( int i = 0; i < class_container->num_components; i++ )
  {
    aWidget_t* current = &class_container->components[i];
    if ( strncmp( current->name, "mercenary", MAX_NAME_LENGTH ) == 0 )
    {
      current->action = mercenary_button;
    }
    if ( strncmp( current->name, "rogue", MAX_NAME_LENGTH ) == 0 )
      current->action = rogue_button;
    if ( strncmp( current->name, "mage", MAX_NAME_LENGTH ) == 0 )
      current->action = mage_button;
  }

}

void ClassSelectInit( void )
{
  app.delegate.logic = cs_Logic;
  app.delegate.draw  = cs_Draw;

  app.options.scale_factor = 1;

  last_class_idx = -1;
  prev_class_idx = -1;
  selected_item = 0;
  num_filtered = 0;
  browsing_items = 0;
  back_hovered = 0;
  ItemsLoadAll();

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_hover );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  a_WidgetsInit( "resources/widgets/class_select.auf" );
  app.active_widget = a_GetWidget( "class_select" );
  cs_BindActions();
}

static void cs_Logic( float dt )
{
  a_DoInput();

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_WidgetCacheFree();
    MainMenuInit();
    return;
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/class_select.auf" );
    cs_BindActions();
  }

  /* Detect class change from hover and rebuild filtered items */
  {
    int idx = -1;
    aContainerWidget_t* cc = a_GetContainerFromWidget( "class_select" );
    for ( int i = 0; i < cc->num_components; i++ )
    {
      if ( cc->components[i].state == WI_HOVERING )
      {
        if ( strncmp( cc->components[i].name, "mercenary", MAX_NAME_LENGTH ) == 0 ) idx = 0;
        if ( strncmp( cc->components[i].name, "rogue", MAX_NAME_LENGTH ) == 0 )     idx = 1;
        if ( strncmp( cc->components[i].name, "mage", MAX_NAME_LENGTH ) == 0 )      idx = 2;
        break;
      }
    }
    if ( idx >= 0 )
      last_class_idx = idx;

    if ( last_class_idx != prev_class_idx && last_class_idx >= 0 )
    {
      prev_class_idx = last_class_idx;
      cs_RebuildFiltered();
    }
  }

  /* Item keyboard navigation — wraps around through consumables + openables */
  if ( browsing_items && num_filtered > 0 )
  {
    if ( app.keyboard[A_S] == 1 || app.keyboard[A_DOWN] == 1 )
    {
      app.keyboard[A_S] = 0;
      app.keyboard[A_DOWN] = 0;
      selected_item = ( selected_item + 1 ) % num_filtered;
      a_AudioPlaySound( &sfx_hover, NULL );
    }
    if ( app.keyboard[A_W] == 1 || app.keyboard[A_UP] == 1 )
    {
      app.keyboard[A_W] = 0;
      app.keyboard[A_UP] = 0;
      selected_item = ( selected_item - 1 + num_filtered ) % num_filtered;
      a_AudioPlaySound( &sfx_hover, NULL );
    }
  }

  /* Mouse hovering on item lists — check both panels */
  if ( num_filtered > 0 && last_class_idx >= 0 )
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;

    /* Check consumables panel */
    {
      aContainerWidget_t* cpanel = a_GetContainerFromWidget( "consumables_panel" );
      aRectf_t cr = cpanel->rect;
      float title_h = 0;
      if ( cpanel->num_components > 0 )
        title_h = cpanel->components[0].rect.h + LIST_TITLE_GAP;
      float cy_start = cr.y + title_h + LIST_PAD_Y;

      if ( mx >= (int)cr.x && mx <= (int)( cr.x + cr.w ) )
      {
        int visual_row = 0;
        for ( int fi = 0; fi < num_filtered; fi++ )
        {
          if ( filtered[fi].type != FILTERED_CONSUMABLE ) continue;
          float row_y = cy_start + visual_row * ( LIST_ITEM_SIZE + LIST_ROW_SPACING );
          if ( my >= (int)( row_y - LIST_HIT_MARGIN ) && my <= (int)( row_y + LIST_ITEM_SIZE + LIST_HIT_MARGIN ) )
          {
            if ( selected_item != fi || !browsing_items )
            {
              if ( selected_item != fi )
                a_AudioPlaySound( &sfx_hover, NULL );
              selected_item = fi;
              browsing_items = 1;
            }
            break;
          }
          visual_row++;
        }
      }
    }

    /* Check doors/openables panel */
    {
      aContainerWidget_t* dpanel = a_GetContainerFromWidget( "doors_panel" );
      aRectf_t dr = dpanel->rect;
      float title_h = 0;
      if ( dpanel->num_components > 0 )
        title_h = dpanel->components[0].rect.h + LIST_TITLE_GAP;
      float dy_start = dr.y + title_h + LIST_PAD_Y;

      if ( mx >= (int)dr.x && mx <= (int)( dr.x + dr.w ) )
      {
        int visual_row = 0;
        for ( int fi = 0; fi < num_filtered; fi++ )
        {
          if ( filtered[fi].type != FILTERED_OPENABLE ) continue;
          float row_y = dy_start + visual_row * ( LIST_ITEM_SIZE + LIST_ROW_SPACING );
          if ( my >= (int)( row_y - LIST_HIT_MARGIN ) && my <= (int)( row_y + LIST_ITEM_SIZE + LIST_HIT_MARGIN ) )
          {
            if ( selected_item != fi || !browsing_items )
            {
              if ( selected_item != fi )
                a_AudioPlaySound( &sfx_hover, NULL );
              selected_item = fi;
              browsing_items = 1;
            }
            break;
          }
          visual_row++;
        }
      }
    }
  }

  /* Compute shared button anchor — below the taller item panel */
  float panel_bot;
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "consumables_panel" );
    aContainerWidget_t* dp = a_GetContainerFromWidget( "doors_panel" );
    float cp_bot = cp->rect.y + cp->rect.h;
    float dp_bot = dp->rect.y + dp->rect.h;
    panel_bot = cp_bot > dp_bot ? cp_bot : dp_bot;
  }

  /* Programmatic embark button — mouse hover + click */
  if ( last_class_idx >= 0 )
  {
    aContainerWidget_t* cs = a_GetContainerFromWidget( "class_select" );
    float ex = cs->rect.x + ( cs->rect.w - EMBARK_W ) / 2.0f;
    float ey = panel_bot + 30.0f;

    int hovering = PointInRect( app.mouse.x, app.mouse.y, ex, ey, EMBARK_W, EMBARK_H );

    if ( hovering && !embark_hovered )
      a_AudioPlaySound( &sfx_hover, NULL );
    embark_hovered = hovering;

    if ( hovering && app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      cs_SelectClass( last_class_idx );
      return;
    }

    /* Enter/Space also triggers embark */
    if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 || app.keyboard[SDL_SCANCODE_SPACE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      a_AudioPlaySound( &sfx_click, NULL );
      cs_SelectClass( last_class_idx );
      return;
    }
  }

  /* Programmatic back button — below embark */
  {
    aContainerWidget_t* cs = a_GetContainerFromWidget( "class_select" );
    float bx = cs->rect.x + ( cs->rect.w - BACK_W ) / 2.0f;
    float by = panel_bot + 30.0f + EMBARK_H + 30.0f;

    int hovering = PointInRect( app.mouse.x, app.mouse.y, bx, by, BACK_W, BACK_H );

    if ( hovering && !back_hovered )
      a_AudioPlaySound( &sfx_hover, NULL );
    back_hovered = hovering;

    if ( hovering && app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      a_WidgetCacheFree();
      MainMenuInit();
      return;
    }
  }

  a_DoWidget();
}

static void cs_Draw( float dt )
{
  /* Draw item panel backgrounds at correct auto-sized height (AUF boxed:0) */
  if ( last_class_idx >= 0 && num_filtered > 0 )
  {
    int nc = 0, no = 0;
    for ( int i = 0; i < num_filtered; i++ )
      if ( filtered[i].type == FILTERED_CONSUMABLE ) nc++; else no++;

    aColor_t panel_bg = (aColor_t){ 0, 0, 0, 200 };
    aColor_t panel_fg = (aColor_t){ 255, 255, 255, 255 };

    aContainerWidget_t* cp = a_GetContainerFromWidget( "consumables_panel" );
    float cp_th = 0;
    if ( cp->num_components > 0 ) cp_th = cp->components[0].rect.h + LIST_TITLE_GAP;
    float cp_h = cp_th + LIST_PAD_Y + nc * ( LIST_ITEM_SIZE + LIST_ROW_SPACING ) + LIST_PAD_Y;
    a_DrawFilledRect( (aRectf_t){ cp->rect.x, cp->rect.y, cp->rect.w, cp_h }, panel_bg );
    a_DrawRect( (aRectf_t){ cp->rect.x, cp->rect.y, cp->rect.w, cp_h }, panel_fg );

    aContainerWidget_t* dp = a_GetContainerFromWidget( "doors_panel" );
    float dp_th = 0;
    if ( dp->num_components > 0 ) dp_th = dp->components[0].rect.h + LIST_TITLE_GAP;
    float dp_h = dp_th + LIST_PAD_Y + no * ( LIST_ITEM_SIZE + LIST_ROW_SPACING ) + LIST_PAD_Y;
    a_DrawFilledRect( (aRectf_t){ dp->rect.x, dp->rect.y, dp->rect.w, dp_h }, panel_bg );
    a_DrawRect( (aRectf_t){ dp->rect.x, dp->rect.y, dp->rect.w, dp_h }, panel_fg );
  }

  a_DrawWidgets();

  /* Update WT_OUTPUT widgets with hovered class data */
  aContainerWidget_t* panel = a_GetContainerFromWidget( "info_panel" );

  if ( last_class_idx >= 0 )
  {
    char stat_buf[128];

    for ( int i = 0; i < panel->num_components; i++ )
    {
      aWidget_t* w = &panel->components[i];

      if ( strncmp( w->name, "out_name", MAX_NAME_LENGTH ) == 0 )
        a_OutputWidgetSetText( w, g_classes[last_class_idx].name );

      if ( strncmp( w->name, "out_stats", MAX_NAME_LENGTH ) == 0 )
      {
        snprintf( stat_buf, sizeof( stat_buf ), "HP: %d  DMG: %d  DEF: %d",
                  g_classes[last_class_idx].hp, g_classes[last_class_idx].base_damage, g_classes[last_class_idx].defense );
        a_OutputWidgetSetText( w, stat_buf );
      }

      if ( strncmp( w->name, "out_uses", MAX_NAME_LENGTH ) == 0 )
      {
        snprintf( stat_buf, sizeof( stat_buf ), "%s", g_classes[last_class_idx].consumable_type );
        a_OutputWidgetSetText( w, stat_buf );
      }

      if ( strncmp( w->name, "out_desc", MAX_NAME_LENGTH ) == 0 )
        a_OutputWidgetSetText( w, g_classes[last_class_idx].description );
    }

    /* Draw character image or glyph centered inside image_panel */
    {
      aContainerWidget_t* img_panel = a_GetContainerFromWidget( "image_panel" );
      aRectf_t ir = img_panel->rect;
      float char_size = ir.w < ir.h ? ir.w : ir.h;

      if ( g_classes[last_class_idx].image )
      {
        float orig_w = g_classes[last_class_idx].image->rect.w;
        float orig_h = g_classes[last_class_idx].image->rect.h;
        float scaled_w = orig_w * IMAGE_SCALE;
        float scaled_h = orig_h * IMAGE_SCALE;
        float img_x = ir.x + ( ir.w - scaled_w ) / 2.0f;
        float img_y = ir.y + ( ir.h - scaled_h ) / 2.0f;
        a_BlitRect( g_classes[last_class_idx].image, NULL, &(aRectf_t){ img_x, img_y, orig_w, orig_h }, IMAGE_SCALE );
      }
      else
      {
        float gx = ir.x + ( ir.w - char_size ) / 2.0f;
        float gy = ir.y + ( ir.h - char_size ) / 2.0f;
        DrawImageOrGlyph( NULL, g_classes[last_class_idx].glyph, g_classes[last_class_idx].color, gx, gy, char_size );
      }
    }

    /* Draw consumables for the hovered class (left panel) */
    {
      aContainerWidget_t* cpanel = a_GetContainerFromWidget( "consumables_panel" );
      float title_h = 0;
      if ( cpanel->num_components > 0 )
        title_h = cpanel->components[0].rect.h + LIST_TITLE_GAP;

      aRectf_t cr = cpanel->rect;
      float cx = cr.x + LIST_PAD_X;
      float cy = cr.y + title_h + LIST_PAD_Y;

      for ( int fi = 0; fi < num_filtered; fi++ )
      {
        if ( filtered[fi].type != FILTERED_CONSUMABLE ) continue;
        int ci = filtered[fi].index;
        float row_h = LIST_ITEM_SIZE + LIST_HIT_MARGIN * 2;

        /* Highlight selected row — full width */
        if ( fi == selected_item && browsing_items )
        {
          a_DrawFilledRect( (aRectf_t){ cr.x + LIST_HIT_MARGIN, cy - LIST_HIT_MARGIN, cr.w - LIST_HIT_MARGIN * 2, row_h },
                            (aColor_t){ 255, 255, 255, 40 } );
          a_DrawRect( (aRectf_t){ cr.x + LIST_HIT_MARGIN, cy - LIST_HIT_MARGIN, cr.w - LIST_HIT_MARGIN * 2, row_h },
                      g_consumables[ci].color );
        }

        DrawImageOrGlyph( g_consumables[ci].image, g_consumables[ci].glyph, g_consumables[ci].color,
                             cx, cy, LIST_ITEM_SIZE );

        aTextStyle_t ns = a_default_text_style;
        ns.fg = ( fi == selected_item && browsing_items ) ? g_consumables[ci].color : white;
        ns.bg = (aColor_t){ 0, 0, 0, 0 };
        ns.scale = 1.0f;
        a_DrawText( g_consumables[ci].name, (int)( cx + LIST_ITEM_SIZE + LIST_TEXT_GAP ), (int)( cy + LIST_ITEM_SIZE * 0.25f ), ns );

        cy += LIST_ITEM_SIZE + LIST_ROW_SPACING;
      }
    }

    /* Draw openables for the hovered class (right panel) */
    {
      aContainerWidget_t* dpanel = a_GetContainerFromWidget( "doors_panel" );
      float title_h = 0;
      if ( dpanel->num_components > 0 )
        title_h = dpanel->components[0].rect.h + LIST_TITLE_GAP;

      aRectf_t dr = dpanel->rect;
      float dx = dr.x + LIST_PAD_X;
      float dy = dr.y + title_h + LIST_PAD_Y;

      for ( int fi = 0; fi < num_filtered; fi++ )
      {
        if ( filtered[fi].type != FILTERED_OPENABLE ) continue;
        int oi = filtered[fi].index;
        float row_h = LIST_ITEM_SIZE + LIST_HIT_MARGIN * 2;

        /* Highlight selected row */
        if ( fi == selected_item && browsing_items )
        {
          a_DrawFilledRect( (aRectf_t){ dr.x + LIST_HIT_MARGIN, dy - LIST_HIT_MARGIN, dr.w - LIST_HIT_MARGIN * 2, row_h },
                            (aColor_t){ 255, 255, 255, 40 } );
          a_DrawRect( (aRectf_t){ dr.x + LIST_HIT_MARGIN, dy - LIST_HIT_MARGIN, dr.w - LIST_HIT_MARGIN * 2, row_h },
                      g_openables[oi].color );
        }

        DrawImageOrGlyph( g_openables[oi].image, g_openables[oi].glyph, g_openables[oi].color,
                             dx, dy, LIST_ITEM_SIZE );

        aTextStyle_t ns = a_default_text_style;
        ns.fg = ( fi == selected_item && browsing_items ) ? g_openables[oi].color : white;
        ns.bg = (aColor_t){ 0, 0, 0, 0 };
        ns.scale = 1.0f;
        a_DrawText( g_openables[oi].name, (int)( dx + LIST_ITEM_SIZE + LIST_TEXT_GAP ), (int)( dy + LIST_ITEM_SIZE * 0.25f ), ns );

        dy += LIST_ITEM_SIZE + LIST_ROW_SPACING;
      }
    }

    /* Detail modal — centered between panels, shows consumable or openable info */
    if ( browsing_items && num_filtered > 0 && selected_item < num_filtered )
    {
      aContainerWidget_t* cpanel = a_GetContainerFromWidget( "consumables_panel" );
      aContainerWidget_t* dpanel = a_GetContainerFromWidget( "doors_panel" );
      aRectf_t cr = cpanel->rect;
      aRectf_t dr = dpanel->rect;

      float gap_left = cr.x + cr.w;
      float gap_right = dr.x;
      float mx = gap_left + ( gap_right - gap_left - MODAL_W ) / 2.0f;
      float my = cr.y;

      FilteredItem_t* sel = &filtered[selected_item];

      if ( sel->type == FILTERED_CONSUMABLE )
      {
        ConsumableInfo_t* c = &g_consumables[sel->index];

        a_DrawFilledRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, (aColor_t){ 0, 0, 0, 255 } );
        a_DrawRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, c->color );

        float ty = my + MODAL_PAD_Y;
        float tx = mx + MODAL_PAD_X;

        aTextStyle_t ts = a_default_text_style;
        ts.bg = (aColor_t){ 0, 0, 0, 0 };

        /* Name */
        ts.fg = c->color;
        ts.scale = MODAL_NAME_SCALE;
        a_DrawText( c->name, (int)tx, (int)ty, ts );
        ty += MODAL_LINE_LG;

        /* Effect */
        char buf[128];
        ts.fg = white;
        ts.scale = MODAL_TEXT_SCALE;
        snprintf( buf, sizeof( buf ), "Effect: %s", c->effect );
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += MODAL_LINE_SM;

        /* Bonus damage */
        snprintf( buf, sizeof( buf ), "+%d Bonus Damage", c->bonus_damage );
        ts.fg = yellow;
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += MODAL_LINE_MD;

        /* Description */
        ts.fg = (aColor_t){ 180, 180, 180, 255 };
        ts.scale = MODAL_DESC_SCALE;
        ts.wrap_width = (int)( MODAL_W - MODAL_PAD_X * 2 );
        a_DrawText( c->description, (int)tx, (int)ty, ts );
      }
      else /* FILTERED_OPENABLE */
      {
        OpenableInfo_t* o = &g_openables[sel->index];

        a_DrawFilledRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, (aColor_t){ 0, 0, 0, 255 } );
        a_DrawRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, o->color );

        float ty = my + MODAL_PAD_Y;
        float tx = mx + MODAL_PAD_X;

        aTextStyle_t ts = a_default_text_style;
        ts.bg = (aColor_t){ 0, 0, 0, 0 };

        /* Name */
        ts.fg = o->color;
        ts.scale = MODAL_NAME_SCALE;
        a_DrawText( o->name, (int)tx, (int)ty, ts );
        ty += MODAL_LINE_LG;

        /* Kind */
        char buf[128];
        ts.fg = white;
        ts.scale = MODAL_TEXT_SCALE;
        snprintf( buf, sizeof( buf ), "Type: %s", o->kind );
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += MODAL_LINE_MD;

        /* Description */
        ts.fg = (aColor_t){ 180, 180, 180, 255 };
        ts.scale = MODAL_DESC_SCALE;
        ts.wrap_width = (int)( MODAL_W - MODAL_PAD_X * 2 );
        a_DrawText( o->description, (int)tx, (int)ty, ts );
      }
    }
  }

  /* Compute shared button anchor */
  float btn_panel_bot;
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "consumables_panel" );
    aContainerWidget_t* dp = a_GetContainerFromWidget( "doors_panel" );
    float cp_bot = cp->rect.y + cp->rect.h;
    float dp_bot = dp->rect.y + dp->rect.h;
    btn_panel_bot = cp_bot > dp_bot ? cp_bot : dp_bot;
  }

  /* Draw embark button — only when a class is hovered */
  if ( last_class_idx >= 0 )
  {
    aContainerWidget_t* cs = a_GetContainerFromWidget( "class_select" );
    float ex = cs->rect.x + ( cs->rect.w - EMBARK_W ) / 2.0f;
    float ey = btn_panel_bot + 30.0f;

    DrawButton( ex, ey, EMBARK_W, EMBARK_H, "Embark [Enter]", 2.0f, embark_hovered,
                (aColor_t){ 0, 0, 0, 255 }, (aColor_t){ 60, 60, 60, 255 },
                (aColor_t){ 222, 222, 222, 255 }, (aColor_t){ 255, 255, 255, 255 } );
  }

  /* Draw back button — below embark */
  {
    aContainerWidget_t* cs = a_GetContainerFromWidget( "class_select" );
    float bx = cs->rect.x + ( cs->rect.w - BACK_W ) / 2.0f;
    float by = btn_panel_bot + 30.0f + EMBARK_H + 30.0f;

    DrawButton( bx, by, BACK_W, BACK_H, "Back [ESC]", 1.0f, back_hovered,
                (aColor_t){ 0, 0, 0, 255 }, (aColor_t){ 60, 60, 60, 255 },
                (aColor_t){ 150, 150, 150, 255 }, white );
  }
}

static void mercenary_button( void )
{
  cs_SelectClass( 0 );
}

static void rogue_button( void )
{
  cs_SelectClass( 1 );
}

static void mage_button( void )
{
  cs_SelectClass( 2 );
}


