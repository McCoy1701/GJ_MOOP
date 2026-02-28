#include <stdio.h>
#include <string.h>
#include <Archimedes.h>
#include "defines.h"
#include "player.h"
#include "items.h"
#include "draw_utils.h"
#include "transitions.h"
#include "sound_manager.h"
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

/* Drop target — center of game viewport widget on screen */
#define DROP_SIZE         64.0f
#define DROP_TARGET_X     ( 8.0f + ( SCREEN_WIDTH * 0.72f ) / 2.0f )
#define DROP_TARGET_Y     ( 66.0f + ( SCREEN_HEIGHT - 216.0f ) / 2.0f )

/* Embark button */
#define EMBARK_W          288.0f
#define EMBARK_H          58.0f

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
static int pending_class = -1;   /* set when embark triggers outro */

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
  player.class_damage = g_classes[index].base_damage;
  player.class_defense = g_classes[index].defense;
  strncpy( player.consumable_type, g_classes[index].consumable_type, MAX_NAME_LENGTH - 1 );
  strncpy( player.description, g_classes[index].description, 255 );
  player.image = g_classes[index].image;
  player.world_x = 64.0f;
  player.world_y = 64.0f;
  memset( player.inventory, 0, sizeof( player.inventory ) );
  player.inv_cursor = 0;
  player.selected_consumable = 0;
  player.equip_cursor = 0;
  player.inv_focused = 1;
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
    player.equipment[i] = -1;
  EquipStarterGear( g_class_keys[index] );
  PlayerInitStats();
  PlayerRecalcStats();

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

  /* Outro in progress — blocks all input except ESC to skip */
  if ( TransitionOutroActive() || TransitionOutroDone() )
  {
    if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
      TransitionOutroSkip();
    }
    /* Eat keys so a_DoWidget doesn't re-trigger button actions */
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_SPACE]  = 0;
    app.mouse.pressed = 0;

    TransitionOutroUpdate( dt );

    /* Outro just finished — do the actual scene switch */
    if ( TransitionOutroDone() )
    {
      cs_SelectClass( pending_class );
      return;
    }
    a_DoWidget();
    return;
  }

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
      pending_class = last_class_idx;
      SoundManagerCrossfadeToGame();
      TransitionOutroStart();
      return;
    }

    /* Enter/Space also triggers embark */
    if ( app.keyboard[SDL_SCANCODE_RETURN] == 1 || app.keyboard[SDL_SCANCODE_SPACE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      a_AudioPlaySound( &sfx_click, NULL );
      pending_class = last_class_idx;
      SoundManagerCrossfadeToGame();
      TransitionOutroStart();
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
  int in_outro = TransitionOutroActive();
  float panel_a = in_outro ? TransitionGetOutroAlpha() : 1.0f;

  /* Draw item panel backgrounds at correct auto-sized height (AUF boxed:0) */
  if ( last_class_idx >= 0 && num_filtered > 0 && panel_a > 0.01f )
  {
    int nc = 0, no = 0;
    for ( int i = 0; i < num_filtered; i++ )
      if ( filtered[i].type == FILTERED_CONSUMABLE ) nc++; else no++;

    aColor_t panel_bg = (aColor_t){ 0, 0, 0, (int)( 200 * panel_a ) };
    aColor_t panel_fg = (aColor_t){ 255, 255, 255, (int)( 255 * panel_a ) };

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

  /* Game viewport box — fades in during outro before the character drops */
  if ( in_outro )
  {
    float vp_a = TransitionGetOutroVPAlpha();
    if ( vp_a > 0.01f )
    {
      float vp_x = 8.0f;
      float vp_y = 66.0f;   /* 8 + 50 topbar + 8 gap */
      float vp_w = SCREEN_WIDTH * 0.72f - 1.0f;
      float vp_h = SCREEN_HEIGHT - 216.0f;
      a_DrawFilledRect( (aRectf_t){ vp_x, vp_y, vp_w, vp_h },
                        (aColor_t){ 0, 0, 0, (int)( 255 * vp_a ) } );
      a_DrawRect( (aRectf_t){ vp_x, vp_y, vp_w, vp_h },
                  (aColor_t){ 255, 255, 255, (int)( 255 * vp_a ) } );
    }
  }

  /* Widget labels — skip once fade is underway (can't modulate widget alpha) */
  if ( !in_outro )
    a_DrawWidgets();

  /* Update WT_OUTPUT widgets with hovered class data */
  aContainerWidget_t* panel = a_GetContainerFromWidget( "info_panel" );

  if ( last_class_idx >= 0 )
  {
    /* Output widget text — only when not fading */
    if ( !in_outro )
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
    }

    /* Draw character image or glyph — stays visible during outro */
    {
      aContainerWidget_t* img_panel = a_GetContainerFromWidget( "image_panel" );
      aRectf_t ir = img_panel->rect;
      float portrait_size = ir.h * 1.2f;
      float gx = ir.x + ( ir.w - portrait_size ) / 2.0f;
      float gy = ir.y + ( ir.h - portrait_size ) / 2.0f;

      if ( in_outro )
      {
        /* Drop target = center of game viewport widget */
        float target_x = DROP_TARGET_X - DROP_SIZE / 2.0f;
        float target_y = DROP_TARGET_Y - DROP_SIZE / 2.0f;

        float drop_t = TransitionGetOutroDropT();
        float cur_size = portrait_size + ( DROP_SIZE - portrait_size ) * drop_t;
        float cur_x = gx + ( target_x - gx ) * drop_t;
        float cur_y = gy + ( target_y - gy ) * drop_t
                      + TransitionGetOutroCharOY();

        /* Use flipped blit when sprite has turned */
        aImage_t* img = g_classes[last_class_idx].image;
        if ( TransitionGetOutroFlipped() && img && settings.gfx_mode == GFX_IMAGE )
        {
          float scale = cur_size / img->rect.w;
          a_BlitRectFlipped( img, NULL,
                             &(aRectf_t){ cur_x, cur_y, img->rect.w, img->rect.h },
                             scale, 'x' );
        }
        else
        {
          DrawImageOrGlyph( img, g_classes[last_class_idx].glyph,
                            g_classes[last_class_idx].color,
                            cur_x, cur_y, cur_size );
        }
      }
      else
      {
        DrawImageOrGlyph( g_classes[last_class_idx].image,
                          g_classes[last_class_idx].glyph,
                          g_classes[last_class_idx].color,
                          gx, gy, portrait_size );
      }
    }

    /* Item lists + modal + buttons — fade with panels during outro */
    if ( panel_a > 0.01f )
    {
      aColor_t fade_white = { 255, 255, 255, (int)( 255 * panel_a ) };

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
          aColor_t ic = g_consumables[ci].color;
          aColor_t ic_a = { ic.r, ic.g, ic.b, (int)( ic.a * panel_a ) };

          /* Highlight selected row — full width */
          if ( fi == selected_item && browsing_items )
          {
            a_DrawFilledRect( (aRectf_t){ cr.x + LIST_HIT_MARGIN, cy - LIST_HIT_MARGIN, cr.w - LIST_HIT_MARGIN * 2, row_h },
                              (aColor_t){ 255, 255, 255, (int)( 40 * panel_a ) } );
            a_DrawRect( (aRectf_t){ cr.x + LIST_HIT_MARGIN, cy - LIST_HIT_MARGIN, cr.w - LIST_HIT_MARGIN * 2, row_h },
                        ic_a );
          }

          DrawImageOrGlyph( g_consumables[ci].image, g_consumables[ci].glyph, ic_a,
                               cx, cy, LIST_ITEM_SIZE );

          aTextStyle_t ns = a_default_text_style;
          ns.fg = ( fi == selected_item && browsing_items ) ? ic_a : fade_white;
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
          aColor_t oc = g_openables[oi].color;
          aColor_t oc_a = { oc.r, oc.g, oc.b, (int)( oc.a * panel_a ) };

          /* Highlight selected row */
          if ( fi == selected_item && browsing_items )
          {
            a_DrawFilledRect( (aRectf_t){ dr.x + LIST_HIT_MARGIN, dy - LIST_HIT_MARGIN, dr.w - LIST_HIT_MARGIN * 2, row_h },
                              (aColor_t){ 255, 255, 255, (int)( 40 * panel_a ) } );
            a_DrawRect( (aRectf_t){ dr.x + LIST_HIT_MARGIN, dy - LIST_HIT_MARGIN, dr.w - LIST_HIT_MARGIN * 2, row_h },
                        oc_a );
          }

          DrawImageOrGlyph( g_openables[oi].image, g_openables[oi].glyph, oc_a,
                               dx, dy, LIST_ITEM_SIZE );

          aTextStyle_t ns = a_default_text_style;
          ns.fg = ( fi == selected_item && browsing_items ) ? oc_a : fade_white;
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
          aColor_t cc_a = { c->color.r, c->color.g, c->color.b, (int)( c->color.a * panel_a ) };

          a_DrawFilledRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, (aColor_t){ 0, 0, 0, (int)( 255 * panel_a ) } );
          a_DrawRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, cc_a );

          float ty = my + MODAL_PAD_Y;
          float tx = mx + MODAL_PAD_X;

          aTextStyle_t ts = a_default_text_style;
          ts.bg = (aColor_t){ 0, 0, 0, 0 };

          /* Name */
          ts.fg = cc_a;
          ts.scale = MODAL_NAME_SCALE;
          a_DrawText( c->name, (int)tx, (int)ty, ts );
          ty += MODAL_LINE_LG;

          /* Effect */
          char buf[128];
          ts.fg = fade_white;
          ts.scale = MODAL_TEXT_SCALE;
          snprintf( buf, sizeof( buf ), "Effect: %s", c->effect );
          a_DrawText( buf, (int)tx, (int)ty, ts );
          ty += MODAL_LINE_SM;

          /* Bonus damage */
          snprintf( buf, sizeof( buf ), "+%d Bonus Damage", c->bonus_damage );
          ts.fg = (aColor_t){ yellow.r, yellow.g, yellow.b, (int)( yellow.a * panel_a ) };
          a_DrawText( buf, (int)tx, (int)ty, ts );
          ty += MODAL_LINE_MD;

          /* Description */
          ts.fg = (aColor_t){ 180, 180, 180, (int)( 255 * panel_a ) };
          ts.scale = MODAL_DESC_SCALE;
          ts.wrap_width = (int)( MODAL_W - MODAL_PAD_X * 2 );
          a_DrawText( c->description, (int)tx, (int)ty, ts );
        }
        else /* FILTERED_OPENABLE */
        {
          OpenableInfo_t* o = &g_openables[sel->index];
          aColor_t oc_a = { o->color.r, o->color.g, o->color.b, (int)( o->color.a * panel_a ) };

          a_DrawFilledRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, (aColor_t){ 0, 0, 0, (int)( 255 * panel_a ) } );
          a_DrawRect( (aRectf_t){ mx, my, MODAL_W, MODAL_H }, oc_a );

          float ty = my + MODAL_PAD_Y;
          float tx = mx + MODAL_PAD_X;

          aTextStyle_t ts = a_default_text_style;
          ts.bg = (aColor_t){ 0, 0, 0, 0 };

          /* Name */
          ts.fg = oc_a;
          ts.scale = MODAL_NAME_SCALE;
          a_DrawText( o->name, (int)tx, (int)ty, ts );
          ty += MODAL_LINE_LG;

          /* Kind */
          char buf[128];
          ts.fg = fade_white;
          ts.scale = MODAL_TEXT_SCALE;
          snprintf( buf, sizeof( buf ), "Type: %s", o->kind );
          a_DrawText( buf, (int)tx, (int)ty, ts );
          ty += MODAL_LINE_MD;

          /* Description */
          ts.fg = (aColor_t){ 180, 180, 180, (int)( 255 * panel_a ) };
          ts.scale = MODAL_DESC_SCALE;
          ts.wrap_width = (int)( MODAL_W - MODAL_PAD_X * 2 );
          a_DrawText( o->description, (int)tx, (int)ty, ts );
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
      {
        aContainerWidget_t* cs = a_GetContainerFromWidget( "class_select" );
        float ex = cs->rect.x + ( cs->rect.w - EMBARK_W ) / 2.0f;
        float ey = btn_panel_bot + 30.0f;

        DrawButton( ex, ey, EMBARK_W, EMBARK_H, "Embark [Enter]", 2.0f, embark_hovered,
                    (aColor_t){ 0, 0, 0, (int)( 255 * panel_a ) }, (aColor_t){ 60, 60, 60, (int)( 255 * panel_a ) },
                    (aColor_t){ 222, 222, 222, (int)( 255 * panel_a ) }, (aColor_t){ 255, 255, 255, (int)( 255 * panel_a ) } );
      }

      /* Draw back button — below embark */
      {
        aContainerWidget_t* cs = a_GetContainerFromWidget( "class_select" );
        float bx = cs->rect.x + ( cs->rect.w - BACK_W ) / 2.0f;
        float by = btn_panel_bot + 30.0f + EMBARK_H + 30.0f;

        DrawButton( bx, by, BACK_W, BACK_H, "Back [ESC]", 1.0f, back_hovered,
                    (aColor_t){ 0, 0, 0, (int)( 255 * panel_a ) }, (aColor_t){ 60, 60, 60, (int)( 255 * panel_a ) },
                    (aColor_t){ 150, 150, 150, (int)( 255 * panel_a ) }, fade_white );
      }
    } /* end panel fade */
  }

  /* "Press ESC to skip" hint — centered at top during outro */
  if ( in_outro )
  {
    aTextStyle_t ts = a_default_text_style;
    ts.fg = (aColor_t){ 180, 180, 180, 180 };
    ts.bg = (aColor_t){ 0, 0, 0, 0 };
    ts.scale = 1.0f;
    const char* hint = "Press ESC to skip";
    float tw, th;
    a_CalcTextDimensions( hint, app.font_type, &tw, &th );
    a_DrawText( hint, (int)( SCREEN_WIDTH / 2 - tw / 2 ), 4, ts );
  }
}

static void mercenary_button( void )
{
  pending_class = 0;
  SoundManagerCrossfadeToGame();
  TransitionOutroStart();
}

static void rogue_button( void )
{
  pending_class = 1;
  SoundManagerCrossfadeToGame();
  TransitionOutroStart();
}

static void mage_button( void )
{
  pending_class = 2;
  SoundManagerCrossfadeToGame();
  TransitionOutroStart();
}


