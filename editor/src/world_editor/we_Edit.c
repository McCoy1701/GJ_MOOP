/*
 * world_editor/we_Edit.c:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"

#include "editor.h"
#include "tile.h"
#include "utils.h"
#include "world.h"
#include "world_editor.h"
#include "spawn_data.h"
#include "item_catalog.h"

static dVec2_t selected_pos = {.x = 0, .y = 0};
static dVec2_t highlighted_pos = {.x = 0, .y = 0};
static int glyph_index = 0;
static int fg_index    = 0;
static int bg_index    = 0;
static int tile_index  = 0;
static aPoint2i_t selected_glyph;
static aPoint2i_t selected_fg;
static aPoint2i_t selected_bg;
static aPoint2i_t selected_tile;
static int editor_mode    = 0;
static int selected       = 0;
static Tile_t* clipboard  = NULL;
static int clipbard_amout = 0;

/* Consumable spawn overlay (declared early for undo) */
SpawnList_t g_edit_spawns = {0};
int         g_spawns_loaded = 0;

/* Undo / Redo */
enum { EDIT_TILE, EDIT_SPAWN_ADD, EDIT_SPAWN_REMOVE };

typedef struct {
  int edit_type;
  int index;
  int old_bg, old_mg, old_fg;
  int new_bg, new_mg, new_fg;
  int old_room, new_room;
  SpawnPoint_t spawn;
} TileEdit_t;

static dArray_t* undo_stack = NULL;
static dArray_t* redo_stack = NULL;

static void UndoInit( void )
{
  if ( !undo_stack ) undo_stack = d_ArrayInit( 256, sizeof( TileEdit_t ) );
  if ( !redo_stack ) redo_stack = d_ArrayInit( 256, sizeof( TileEdit_t ) );
}

static void UndoPush( int index, int new_bg, int new_mg, int new_fg )
{
  if ( !undo_stack || !g_map ) return;
  TileEdit_t e = {0};
  e.edit_type = EDIT_TILE;
  e.index  = index;
  e.old_bg = (int)g_map->background[index].tile;
  e.old_mg = (int)g_map->midground[index].tile;
  e.old_fg = (int)g_map->foreground[index].tile;
  e.new_bg = new_bg;
  e.new_mg = new_mg;
  e.new_fg = new_fg;
  e.old_room = g_map->room_ids[index];
  e.new_room = g_map->room_ids[index];
  d_ArrayAppend( undo_stack, &e );
  d_ArrayClear( redo_stack );
}

static void UndoPushRoom( int index, int new_room )
{
  if ( !undo_stack || !g_map ) return;
  TileEdit_t e = {0};
  e.edit_type = EDIT_TILE;
  e.index  = index;
  e.old_bg = (int)g_map->background[index].tile;
  e.old_mg = (int)g_map->midground[index].tile;
  e.old_fg = (int)g_map->foreground[index].tile;
  e.new_bg = e.old_bg;
  e.new_mg = e.old_mg;
  e.new_fg = e.old_fg;
  e.old_room = g_map->room_ids[index];
  e.new_room = new_room;
  d_ArrayAppend( undo_stack, &e );
  d_ArrayClear( redo_stack );
}

static void UndoPushSpawnAdd( SpawnPoint_t sp )
{
  if ( !undo_stack ) return;
  TileEdit_t e = {0};
  e.edit_type = EDIT_SPAWN_ADD;
  e.spawn = sp;
  d_ArrayAppend( undo_stack, &e );
  d_ArrayClear( redo_stack );
}

static void UndoPushSpawnRemove( SpawnPoint_t sp )
{
  if ( !undo_stack ) return;
  TileEdit_t e = {0};
  e.edit_type = EDIT_SPAWN_REMOVE;
  e.spawn = sp;
  d_ArrayAppend( undo_stack, &e );
  d_ArrayClear( redo_stack );
}

static void UndoPerform( void )
{
  if ( !undo_stack || undo_stack->count == 0 ) return;
  TileEdit_t* e = d_ArrayPop( undo_stack );
  if ( !e ) return;
  TileEdit_t re = *e;
  d_ArrayAppend( redo_stack, &re );

  if ( re.edit_type == EDIT_SPAWN_ADD )
  {
    int idx = SpawnListFindAt( &g_edit_spawns, re.spawn.row, re.spawn.col );
    if ( idx >= 0 ) SpawnListRemoveAt( &g_edit_spawns, idx );
  }
  else if ( re.edit_type == EDIT_SPAWN_REMOVE )
  {
    SpawnListAdd( &g_edit_spawns, re.spawn );
  }
  else
  {
    e_UpdateTile( re.index, re.old_bg, re.old_mg, re.old_fg );
    g_map->room_ids[re.index] = re.old_room;
  }
}

static void RedoPerform( void )
{
  if ( !redo_stack || redo_stack->count == 0 ) return;
  TileEdit_t* e = d_ArrayPop( redo_stack );
  if ( !e ) return;
  TileEdit_t ue = *e;
  d_ArrayAppend( undo_stack, &ue );

  if ( ue.edit_type == EDIT_SPAWN_ADD )
  {
    SpawnListAdd( &g_edit_spawns, ue.spawn );
  }
  else if ( ue.edit_type == EDIT_SPAWN_REMOVE )
  {
    int idx = SpawnListFindAt( &g_edit_spawns, ue.spawn.row, ue.spawn.col );
    if ( idx >= 0 ) SpawnListRemoveAt( &g_edit_spawns, idx );
  }
  else
  {
    e_UpdateTile( ue.index, ue.new_bg, ue.new_mg, ue.new_fg );
    g_map->room_ids[ue.index] = ue.new_room;
  }
}

static int  g_spawn_type_cursor = SPAWN_RANDOM_T1;
static int  g_hover_spawn_idx  = -1;
static float g_hover_cycle_timer = 0.0f;
static int  g_hover_cycle_idx  = 0;

/* Consumable sidebar state */
static int   g_spawn_item_scroll = 0;
static char  g_spawn_selected_key[64] = {0};
static int   g_spawn_selected_pool = -1;
static int   g_cons_filtered[ED_MAX_ITEMS];
static int   g_cons_filtered_count = 0;
static int   g_cons_last_type = -1;

static int originx = 0;
static int originy = 0;

static int edit_menu_x = 1120;
static int edit_menu_y = 0;

static int color_x   = 0;
static int color_y   = 0;
static int preview_y = 0;
static int glyph_x   = 0;
static int glyph_y   = 0;
static int tile_x    = 0;
static int tile_y    = 0;

static void we_EditLogic( float dt );
static void we_EditDraw( float dt );

static dString_t* highlight_buf;
static dString_t* select_buf;

/* Hover + right-click context menu */
static int hover_grid_x = -1, hover_grid_y = -1;
static int ctx_open   = 0;
static int ctx_row    = 0, ctx_col = 0;
static int ctx_cursor = 0;
static int ctx_screen_x = 0, ctx_screen_y = 0;  /* screen pos where menu opened */
#define CTX_ED_MAX   3
#define CTX_ED_ROW_H 24.0f
#define CTX_ED_PAD    4.0f
#define CTX_ED_W    170.0f
static const char* ctx_labels_normal[] = { "Select", "Prepare Same Tile" };
static const char* ctx_labels_cons[]   = { "Select", "Prepare Same Tile",
                                            "Prepare Same Consumable",
                                            "Remove Consumable" };
static const char** ctx_labels = ctx_labels_normal;
static int ctx_count = 2;

aRectf_t edit_menu_rect = {0};

char* wem_strings[WEM_MAX+1] =
{
  "WEM_NONE",
  "WEM_BRUSH",
  "WEM_CONSUMABLE",
  "WEM_PASTE",
  "WEM_MASS_CHANGE",
  "WEM_SELECT",
  "WEM_MAX"
};

/* Mode button actions */
static void mode_brush_action( void )  { editor_mode = WEM_BRUSH; }
static void mode_paste_action( void )  { editor_mode = WEM_PASTE; }
static void mode_mass_action( void )   { editor_mode = WEM_MASS_CHANGE; }
static void mode_select_action( void ) { editor_mode = WEM_SELECT; }


void we_Edit( void )
{
  app.delegate.logic = we_EditLogic;
  app.delegate.draw  = we_EditDraw;

  highlight_buf = d_StringInit();
  select_buf    = d_StringInit();
  UndoInit();

  e_GetOrigin( g_map, &originx, &originy );

  edit_menu_rect = (aRectf_t){
    .x = edit_menu_x,
    .y = edit_menu_y,
    .w = 155,
    .h = SCREEN_HEIGHT
  };

  /* Show edit-only widgets */
  aWidget_t* mode_w = a_GetWidget( "mode_bar" );
  aWidget_t* toggle_w = a_GetWidget( "toggle_bar" );
  if ( mode_w )   mode_w->hidden = 0;
  if ( toggle_w ) toggle_w->hidden = 0;

  /* Hide load menu */
  aWidget_t* load_w = a_GetWidget( "load_menu" );
  if ( load_w ) load_w->hidden = 1;

  /* Wire up mode_bar button actions */
  aContainerWidget_t* mode_container = a_GetContainerFromWidget( "mode_bar" );
  if ( mode_container )
  {
    for ( int i = 0; i < mode_container->num_components; i++ )
    {
      aWidget_t* w = &mode_container->components[i];
      if ( strcmp( w->name, "mode_brush" ) == 0 )  w->action = mode_brush_action;
      if ( strcmp( w->name, "mode_paste" ) == 0 )   w->action = mode_paste_action;
      if ( strcmp( w->name, "mode_mass" ) == 0 )    w->action = mode_mass_action;
      if ( strcmp( w->name, "mode_select" ) == 0 )  w->action = mode_select_action;
    }
  }

  /* Side panel layout: below mode_bar, then preview, then tile palette */
  color_x   = edit_menu_x + 5;
  color_y   = 5 + 5 * GLYPH_HEIGHT + 80;  /* below mode buttons */
  preview_y = color_y - 24;  /* selected tile preview */

  tile_x  = edit_menu_x + 5;
  tile_y  = color_y + 50;  /* below tile preview */
  glyph_x = edit_menu_x + 5;
  glyph_y = 100 + tile_y +
    (g_tile_sets[g_current_tileset]->col + g_tile_sets[g_current_tileset]->tile_h) + 5;
}

static void we_EditLogic( float dt )
{
  a_DoInput();
  
  a_DoWidget();
  
  if ( g_map != NULL )
  {
    we_MapMouseCheck( &highlighted_pos, edit_menu_rect );
    d_StringClear(highlight_buf);
    d_StringFormat( highlight_buf, "hig: %.01f, %.01f", highlighted_pos.x, highlighted_pos.y );

    /* Track hover grid cell */
    if ( !g_toggle_ascii )
    {
      int hgx = 0, hgy = 0;
      if ( e_GetCellAtMouseInViewport( g_map->width, g_map->height,
                                        g_map->tile_w, g_map->tile_h,
                                        edit_menu_rect, originx, originy,
                                        &hgx, &hgy ) )
      { hover_grid_x = hgx; hover_grid_y = hgy; }
      else
      { hover_grid_x = -1; hover_grid_y = -1; }
    }
    else
    { hover_grid_x = -1; hover_grid_y = -1; }
  }

  if ( app.keyboard[A_ESCAPE] == 1 )
  {
    app.keyboard[A_ESCAPE] = 0;
    if ( ctx_open )
    {
      ctx_open = 0;
    }
    else if ( editor_mode == WEM_NONE )
    {
      e_WorldEditorInit();
    }
    else
    {
      editor_mode = WEM_NONE;
      selected = 0;
    }
  }

  /* Context menu input */
  if ( ctx_open )
  {
    float sx = ctx_screen_x;
    float sy = ctx_screen_y;

    /* Mouse hover on rows */
    for ( int i = 0; i < ctx_count; i++ )
    {
      float ry = sy + i * ( CTX_ED_ROW_H + CTX_ED_PAD );
      if ( app.mouse.x >= sx && app.mouse.x <= sx + CTX_ED_W &&
           app.mouse.y >= ry && app.mouse.y <= ry + CTX_ED_ROW_H )
        ctx_cursor = i;
    }

    /* Left-click to execute or close */
    if ( app.mouse.button == 1 && app.mouse.state == 1 )
    {
      app.mouse.button = 0; app.mouse.state = 0;
      float mh = ctx_count * ( CTX_ED_ROW_H + CTX_ED_PAD ) - CTX_ED_PAD;
      if ( app.mouse.x >= sx && app.mouse.x <= sx + CTX_ED_W &&
           app.mouse.y >= sy && app.mouse.y <= sy + mh )
      {
        if ( ctx_cursor == 0 ) /* Select */
        {
          selected_pos.x = ctx_row;
          selected_pos.y = ctx_col;
          d_StringClear( select_buf );
          d_StringFormat( select_buf, "sel: %d, %d", ctx_row, ctx_col );
          selected = 1;
        }
        else if ( ctx_cursor == 1 ) /* Select Same Tile */
        {
          int idx = ctx_col * g_map->width + ctx_row;
          tile_index = (int)g_map->background[idx].tile;
        }
        else if ( ctx_cursor == 2 ) /* Prepare Same Consumable */
        {
          int si = SpawnListFindAt( &g_edit_spawns, ctx_row, ctx_col );
          if ( si >= 0 )
          {
            SpawnPoint_t* sp = &g_edit_spawns.points[si];
            g_spawn_type_cursor = sp->type;
            if ( sp->type == SPAWN_CONSUMABLE || sp->type == SPAWN_EQUIPMENT
                 || sp->type == SPAWN_MAP )
              strncpy( g_spawn_selected_key, sp->key, 63 );
            else if ( sp->type == SPAWN_POOL )
              g_spawn_selected_pool = sp->pool_id;
            g_cons_last_type = -1;  /* force filtered list rebuild */
          }
        }
        else if ( ctx_cursor == 3 ) /* Remove Consumable */
        {
          int si = SpawnListFindAt( &g_edit_spawns, ctx_row, ctx_col );
          if ( si >= 0 )
          {
            UndoPushSpawnRemove( g_edit_spawns.points[si] );
            SpawnListRemoveAt( &g_edit_spawns, si );
          }
        }
        ctx_open = 0;
      }
      else
      {
        ctx_open = 0;
      }
    }

    /* Right-click closes too */
    if ( app.mouse.button == 3 && app.mouse.state == 1 )
    {
      app.mouse.button = 0; app.mouse.state = 0;
      ctx_open = 0;
    }

    goto skip_normal_input;
  }
  
  if ( app.mouse.button == 1 && app.mouse.state == 1
       && !( g_toggle_consumable && g_spawns_loaded ) )
  {
    app.mouse.button = 0, app.mouse.state = 0;
    e_ColorMouseCheck( color_x, color_y,
                       &bg_index, &selected_bg.x, &selected_bg.y, 0 );

    if ( g_toggle_room )
    {
      /* Room glyph palette click check (must match draw skip list) */
      static const int skip[] = { '#', '.', 'B', 'R', 'W', 'G' };
      static const int skip_count = 6;
      int gx = 0, gy = 0, width = 16;
      int ox = edit_menu_x + 5, oy = preview_y;

      for ( int i = 0; i < 254; i++ )
      {
        int excluded = 0;
        for ( int s = 0; s < skip_count; s++ )
        {
          if ( i == skip[s] ) { excluded = 1; break; }
        }
        if ( excluded ) continue;

        if ( gx + GLYPH_WIDTH > ( width * GLYPH_WIDTH ) )
        {
          gx = 0;
          gy += GLYPH_HEIGHT;
        }

        aRectf_t rect = { gx + ox, gy + oy,
                           app.glyphs[app.font_type][i].w,
                           app.glyphs[app.font_type][i].h };
        if ( WithinRange( app.mouse.x, app.mouse.y, rect ) )
        {
          glyph_index = i;
          break;
        }
        gx += GLYPH_WIDTH;
      }
    }
    else
    {
      e_GlyphMouseCheck( glyph_x, glyph_y,
                         &glyph_index, &selected_glyph.x, &selected_glyph.y, 0 );
      we_TilePaletteMouseCheck( tile_x, tile_y, &tile_index, g_current_tileset );
    }
    
    we_MapMouseCheck( &selected_pos, edit_menu_rect );
    d_StringClear(select_buf);
    d_StringFormat( select_buf, "sel: %.01f, %.01f", selected_pos.x, selected_pos.y );
    selected = 1;
    
    if ( g_map != NULL )
    {
      int grid_x = 0, grid_y = 0;
      int clicked = 0;
      if ( !g_toggle_ascii )
      {
        clicked = e_GetCellAtMouseInViewport( g_map->width,  g_map->height,
                                              g_map->tile_w, g_map->tile_h,
                                              edit_menu_rect,
                                              originx, originy,
                                              &grid_x, &grid_y );
      }

      if ( editor_mode == WEM_SELECT )
      {

      }
      
      else if ( editor_mode == WEM_NONE )
      {
        int index = grid_y * g_map->width + grid_x;
        
        if ( clicked )
        {
          if ( !g_toggle_room )
          {
            int place_tile = tile_index;

            /* Auto-orient doors: EW selected → check walls N+S for NS variant */
            if ( tile_index >= TILE_BLUE_DOOR_EW && tile_index <= TILE_WHITE_DOOR_EW )
            {
              int n_idx = ( grid_y > 0 ) ? ( grid_y - 1 ) * g_map->width + grid_x : -1;
              int s_idx = ( grid_y < g_map->height - 1 ) ? ( grid_y + 1 ) * g_map->width + grid_x : -1;

              int walls_ns = ( n_idx >= 0 && g_map->background[n_idx].tile == TILE_LVL1_WALL )
                          && ( s_idx >= 0 && g_map->background[s_idx].tile == TILE_LVL1_WALL );

              if ( walls_ns )
                place_tile = tile_index + ( TILE_BLUE_DOOR_NS - TILE_BLUE_DOOR_EW );

              UndoPush( index, TILE_LVL1_FLOOR, place_tile, TILE_EMPTY );
              e_UpdateTile( index, TILE_LVL1_FLOOR, place_tile, TILE_EMPTY );
            }
            else
            {
              UndoPush( index, tile_index, TILE_EMPTY, TILE_EMPTY );
              e_UpdateTile( index, tile_index, TILE_EMPTY, TILE_EMPTY );
            }
          }

          else
          {
            UndoPushRoom( index, glyph_index );
            g_map->room_ids[index] = glyph_index;
          }
        }
      }
    }
  }
  
  if ( app.mouse.button == 3 && app.mouse.state == 1 )
  {
    app.mouse.button = 0, app.mouse.state = 0;
    if ( !( g_toggle_consumable && g_spawns_loaded ) )
      e_ColorMouseCheck( color_x, color_y,
                        &fg_index, &selected_fg.x, &selected_fg.y, 0 );

    if ( g_map != NULL )
    {
      int grid_x = 0, grid_y = 0;
      int clicked = 0;
      if ( !g_toggle_ascii )
      {
        clicked = e_GetCellAtMouseInViewport( g_map->width,  g_map->height,
                                              g_map->tile_w, g_map->tile_h,
                                              edit_menu_rect,
                                              originx, originy,
                                              &grid_x, &grid_y );
      }

      if ( editor_mode == WEM_SELECT )
      {

      }
      
      else if ( editor_mode == WEM_MASS_CHANGE )
      {
        we_MassChange( g_map, &selected_pos, &highlighted_pos,
                       tile_index, g_toggle_room, glyph_index );
      }

      else if ( editor_mode == WEM_NONE )
      {
        if ( clicked )
        {
          ctx_row      = grid_x;
          ctx_col      = grid_y;
          ctx_cursor   = 0;
          ctx_screen_x = app.mouse.x;
          ctx_screen_y = app.mouse.y;

          /* Set context menu labels based on mode */
          if ( g_toggle_consumable && g_spawns_loaded &&
               SpawnListFindAt( &g_edit_spawns, grid_x, grid_y ) >= 0 )
          {
            ctx_labels = ctx_labels_cons;
            ctx_count  = 4;
          }
          else
          {
            ctx_labels = ctx_labels_normal;
            ctx_count  = 2;
          }
          ctx_open = 1;
        }
      }
    }
  }

  if ( app.keyboard[A_S] == 1 )
  {
    app.keyboard[A_S] = 0;

    editor_mode = WEM_SELECT;
  }
  
  if ( app.keyboard[A_X] == 1 )
  {
    app.keyboard[A_X] = 0;

    editor_mode = WEM_MASS_CHANGE;
  }

  if ( app.keyboard[A_C] == 1 )
  {
    app.keyboard[A_C] = 0;
    g_toggle_consumable = !g_toggle_consumable;
    if ( g_toggle_consumable ) g_toggle_room = 0;
  }
  
  if ( app.keyboard[A_V] == 1 )
  {
    app.keyboard[A_V] = 0;

    editor_mode = WEM_PASTE;
  }
  
  if ( app.keyboard[A_B] == 1 )
  {
    app.keyboard[A_B] = 0;

    editor_mode = WEM_BRUSH;
  }
  
  if ( app.keyboard[A_T] == 1 )
  {
    app.keyboard[A_T] = 0;
    g_toggle_ascii = !g_toggle_ascii;
  }
  
  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    g_toggle_room = !g_toggle_room;
    if ( g_toggle_room ) g_toggle_consumable = 0;
  }
  
  /* Consumable mode: sidebar clicks + map placement */
  if ( g_toggle_consumable && g_map != NULL && g_spawns_loaded )
  {
    /* Sidebar mouse checks (scroll wheel always, clicks consume event) */
    int sidebar_clicked = 0;
    int list_oy = preview_y + SPAWN_TYPE_COUNT * ( 18 + 2 ) + 8;
    int list_max_h = 400;

    /* Scroll wheel for item list (always active) */
    if ( g_spawn_type_cursor == SPAWN_CONSUMABLE ||
         g_spawn_type_cursor == SPAWN_EQUIPMENT ||
         g_spawn_type_cursor == SPAWN_MAP )
    {
      we_ItemListMouseCheck( edit_menu_x + 5, list_oy, list_max_h,
                             g_cons_filtered, g_cons_filtered_count,
                             &g_spawn_item_scroll, g_spawn_selected_key );
    }

    if ( app.mouse.button == 1 && app.mouse.state == 1 &&
         app.mouse.x >= edit_menu_x )
    {
      int new_type = g_spawn_type_cursor;
      if ( we_SpawnTypeMouseCheck( edit_menu_x + 5, preview_y, &new_type ) )
      {
        g_spawn_type_cursor = new_type;
        g_cons_last_type = -1;  /* force filtered list rebuild */
        sidebar_clicked = 1;
      }

      if ( !sidebar_clicked )
      {
        switch ( g_spawn_type_cursor )
        {
          case SPAWN_CONSUMABLE:
          case SPAWN_EQUIPMENT:
          case SPAWN_MAP:
            if ( we_ItemListMouseCheck( edit_menu_x + 5, list_oy, list_max_h,
                                        g_cons_filtered, g_cons_filtered_count,
                                        &g_spawn_item_scroll,
                                        g_spawn_selected_key ) )
              sidebar_clicked = 1;
            break;
          case SPAWN_POOL:
            if ( we_PoolListMouseCheck( edit_menu_x + 5, list_oy,
                                        &g_edit_spawns,
                                        &g_spawn_selected_pool ) )
              sidebar_clicked = 1;
            break;
          default:
            break;
        }
      }

      if ( sidebar_clicked )
      {
        app.mouse.button = 0;
        app.mouse.state = 0;
      }
    }

    if ( app.mouse.button == 1 && app.mouse.state == 1 )
    {
      app.mouse.button = 0; app.mouse.state = 0;
      int grid_x = 0, grid_y = 0;
      if ( !g_toggle_ascii &&
           e_GetCellAtMouseInViewport( g_map->width, g_map->height,
                                       g_map->tile_w, g_map->tile_h,
                                       edit_menu_rect,
                                       originx, originy,
                                       &grid_x, &grid_y ) )
      {
        if ( SpawnListFindAt( &g_edit_spawns, grid_x, grid_y ) < 0 )
        {
          SpawnPoint_t pt = {0};
          pt.row  = grid_x;
          pt.col  = grid_y;
          pt.type = (SpawnType_t)g_spawn_type_cursor;

          switch ( pt.type )
          {
            case SPAWN_CONSUMABLE:
            case SPAWN_EQUIPMENT:
            case SPAWN_MAP:
              strncpy( pt.key, g_spawn_selected_key, 63 );
              break;
            case SPAWN_CLASS_RARE:
              if ( g_ed_rare_count >= 3 )
              {
                strncpy( pt.class_keys[0],
                         g_ed_items[g_ed_rare_indices[0]].key, 63 );
                strncpy( pt.class_keys[1],
                         g_ed_items[g_ed_rare_indices[1]].key, 63 );
                strncpy( pt.class_keys[2],
                         g_ed_items[g_ed_rare_indices[2]].key, 63 );
              }
              break;
            case SPAWN_POOL:
              pt.pool_id = g_spawn_selected_pool;
              break;
            default:
              break;
          }

          SpawnListAdd( &g_edit_spawns, pt );
          UndoPushSpawnAdd( pt );
        }
      }
    }

  }

  /* Ctrl+Z: undo */
  if ( app.keyboard[A_LCTRL] && app.keyboard[A_Z] == 1 )
  {
    app.keyboard[A_Z] = 0;
    UndoPerform();
  }

  /* Ctrl+Y: redo */
  if ( app.keyboard[A_LCTRL] && app.keyboard[A_Y] == 1 )
  {
    app.keyboard[A_Y] = 0;
    RedoPerform();
  }

  /* Ctrl+S: save consumable DUF */
  if ( app.keyboard[A_LCTRL] && app.keyboard[A_S] == 1 && g_spawns_loaded )
  {
    app.keyboard[A_S] = 0;
    char duf_path[256];
    SpawnDUFFilename( g_current_filename, duf_path, sizeof(duf_path) );
    if ( SpawnDUFSave( duf_path, &g_edit_spawns ) )
      printf( "EDITOR: saved %d spawns to %s\n", g_edit_spawns.count, duf_path );
  }

  /* Number keys 1-5 select spawn type when in consumable mode */
  if ( g_toggle_consumable )
  {
    if ( app.keyboard[A_1] == 1 ) { app.keyboard[A_1] = 0; g_spawn_type_cursor = SPAWN_RANDOM_T1; }
    if ( app.keyboard[A_2] == 1 ) { app.keyboard[A_2] = 0; g_spawn_type_cursor = SPAWN_RANDOM_T2; }
    if ( app.keyboard[A_3] == 1 ) { app.keyboard[A_3] = 0; g_spawn_type_cursor = SPAWN_CONSUMABLE; }
    if ( app.keyboard[A_4] == 1 ) { app.keyboard[A_4] = 0; g_spawn_type_cursor = SPAWN_EQUIPMENT; }
    if ( app.keyboard[A_5] == 1 ) { app.keyboard[A_5] = 0; g_spawn_type_cursor = SPAWN_CLASS_RARE; }
    if ( app.keyboard[A_6] == 1 ) { app.keyboard[A_6] = 0; g_spawn_type_cursor = SPAWN_POOL; }
  }

skip_normal_input:
  a_ViewportInput( &app.g_viewport, EDITOR_WORLD_WIDTH, EDITOR_WORLD_HEIGHT );
}

static void we_EditDraw( float dt )
{
  if ( g_map != NULL )
  {
    WorldDraw( originx, originy, g_map, g_tile_sets[g_current_tileset],
               g_toggle_room, g_toggle_ascii );
  }

  /* Hover border on tile under cursor */
  if ( g_map != NULL && hover_grid_x >= 0 && hover_grid_y >= 0 )
  {
    aPoint2f_t scale = a_ViewportCalculateScale();
    float view_x = app.g_viewport.x - app.g_viewport.w;
    float view_y = app.g_viewport.y - app.g_viewport.h;
    int tw = g_map->tile_w;
    int th = g_map->tile_h;
    float wx = originx + hover_grid_x * tw + tw;
    float wy = originy + hover_grid_y * th + th;
    float sx = ( wx - view_x ) * scale.y;
    float sy = ( wy - view_y ) * scale.y;
    float sw = tw * scale.y;
    float sh = th * scale.y;
    a_DrawRect( (aRectf_t){ sx, sy, sw, sh },
                (aColor_t){ 0xeb, 0xed, 0xe9, 80 } );
  }

  a_DrawFilledRect( edit_menu_rect, blue );
  
  aTextStyle_t ts = {
    .type = FONT_CODE_PAGE_437,
    .align = TEXT_ALIGN_CENTER,
    .wrap_width = 0,
    .scale = 1.0f,
    .padding = 0,
    .bg = black,
    .fg = white
  };
  
  //ts.bg = master_colors[APOLLO_PALETE][bg_index];
  //ts.fg = master_colors[APOLLO_PALETE][fg_index];

  if ( g_toggle_room )
  {
    /* Room mode: show full glyph palette minus reserved room chars */
    static const int skip[] = { '#', '.', 'B', 'R', 'W', 'G' };
    static const int skip_count = 6;

    int gx = 0, gy = 0;
    int width = 16;
    int ox = edit_menu_x + 5;
    int oy = preview_y;

    for ( int i = 0; i < 254; i++ )
    {
      int excluded = 0;
      for ( int s = 0; s < skip_count; s++ )
      {
        if ( i == skip[s] ) { excluded = 1; break; }
      }
      if ( excluded ) continue;

      if ( gx + GLYPH_WIDTH > ( width * GLYPH_WIDTH ) )
      {
        gx = 0;
        gy += GLYPH_HEIGHT;
      }

      aRectf_t glyph_rect = {
        .x = gx + ox,
        .y = gy + oy,
        .w = app.glyphs[app.font_type][i].w,
        .h = app.glyphs[app.font_type][i].h
      };

      a_DrawGlyph_special( i, glyph_rect, white, black, FONT_CODE_PAGE_437 );

      if ( i == glyph_index )
        a_DrawRect( glyph_rect, magenta );

      gx += GLYPH_WIDTH;
    }
  }
  else if ( g_toggle_consumable && g_spawns_loaded )
  {
    /* Consumable mode sidebar */
    int zone_b_y = we_DrawSpawnTypePalette( edit_menu_x + 5, preview_y,
                                             g_spawn_type_cursor );

    int list_oy = zone_b_y + 8;
    int list_max_h = 400;

    switch ( g_spawn_type_cursor )
    {
      case SPAWN_CONSUMABLE:
      case SPAWN_EQUIPMENT:
      case SPAWN_MAP:
        /* Rebuild filtered list when type changes */
        if ( g_cons_last_type != g_spawn_type_cursor )
        {
          we_BuildFilteredItems( g_spawn_type_cursor,
                                 g_cons_filtered, &g_cons_filtered_count );
          g_cons_last_type = g_spawn_type_cursor;
          g_spawn_item_scroll = 0;
        }
        we_DrawItemList( edit_menu_x + 5, list_oy, list_max_h,
                         g_cons_filtered, g_cons_filtered_count,
                         g_spawn_item_scroll, g_spawn_selected_key );
        break;

      case SPAWN_POOL:
        we_DrawPoolList( edit_menu_x + 5, list_oy,
                         &g_edit_spawns, g_spawn_selected_pool );
        break;

      case SPAWN_RANDOM_T1:
      {
        aTextStyle_t info_ts = ts;
        info_ts.fg = (aColor_t){ 0x38, 0xb7, 0x64, 255 };
        a_DrawText( "Random T1", edit_menu_x + 5, list_oy, info_ts );
        break;
      }
      case SPAWN_RANDOM_T2:
      {
        aTextStyle_t info_ts = ts;
        info_ts.fg = (aColor_t){ 0xf7, 0xe2, 0x6b, 255 };
        a_DrawText( "Random T2", edit_menu_x + 5, list_oy, info_ts );
        break;
      }
      case SPAWN_CLASS_RARE:
      {
        aTextStyle_t info_ts = ts;
        info_ts.fg = (aColor_t){ 0xe4, 0x3b, 0x44, 255 };
        a_DrawText( "Rare Consumable", edit_menu_x + 5, list_oy, info_ts );
        a_DrawText( "(auto-filled)", edit_menu_x + 5, list_oy + 14, ts );
        break;
      }
      case SPAWN_CLASS_EQUIPMENT:
      {
        aTextStyle_t info_ts = ts;
        info_ts.fg = (aColor_t){ 0xe4, 0x3b, 0x44, 255 };
        a_DrawText( "Class Equipment", edit_menu_x + 5, list_oy, info_ts );
        a_DrawText( "(auto-filled)", edit_menu_x + 5, list_oy + 14, ts );
        break;
      }
      default:
        break;
    }
  }
  else
  {
    aImage_t* tile_img = g_tile_sets[g_current_tileset]->img_array[tile_index].img;
    float preview_s = 3.0f;
    int pw = (int)( tile_img->rect.w * preview_s );
    int ph = (int)( tile_img->rect.h * preview_s );
    int preview_x = edit_menu_x + ( 155 - pw ) / 2;
    a_BlitRect( tile_img, NULL,
                &(aRectf_t){ preview_x, preview_y, tile_img->rect.w, tile_img->rect.h },
                preview_s );

    /* Arrow triangle pointing right, to the left of the preview */
    int ax = preview_x - 12;
    int ay = preview_y + ph / 2;
    int sz = 5;
    for ( int r = 0; r < sz; r++ )
    {
      a_DrawFilledRect( (aRectf_t){ ax + r, ay - r, 2, 1 + r * 2 }, white );
    }

    we_DrawTilePalette( tile_x, tile_y, tile_index, g_current_tileset );
  }

  /* Highlight active mode button */
  {
    aColor_t active_bg   = { 0x38, 0xb7, 0x64, 255 };
    aColor_t inactive_bg = { 40, 40, 40, 255 };

    const char* mode_names[] = { "mode_brush", "mode_paste", "mode_mass", "mode_select" };
    int mode_vals[] = { WEM_BRUSH, WEM_PASTE, WEM_MASS_CHANGE, WEM_SELECT };

    for ( int i = 0; i < 4; i++ )
    {
      aWidget_t* w = a_GetWidget( mode_names[i] );
      if ( w ) w->bg = ( editor_mode == mode_vals[i] ) ? active_bg : inactive_bg;
    }
  }

  a_DrawText(d_StringPeek(highlight_buf), edit_menu_x+80, 620, ts );
  a_DrawText(d_StringPeek(select_buf), edit_menu_x+80, 640, ts );

  if ( editor_mode == WEM_SELECT || editor_mode == WEM_MASS_CHANGE )
  {
    if ( selected )
    {
      DrawSelected( g_map, &selected_pos, &highlighted_pos );
    }
  }
  
  /* Consumable spawn overlays (viewport-aware) */
  if ( g_toggle_consumable && g_map != NULL && g_spawns_loaded )
  {
    aColor_t sp_green   = { 0x38, 0xb7, 0x64, 200 };
    aColor_t sp_yellow  = { 0xf7, 0xe2, 0x6b, 200 };
    aColor_t sp_cyan    = { 0x41, 0xa6, 0xf6, 200 };
    aColor_t sp_magenta = { 0xb5, 0x5a, 0x9e, 200 };
    aColor_t sp_red     = { 0xe4, 0x3b, 0x44, 200 };
    aColor_t sp_orange  = { 0xf7, 0x76, 0x22, 200 };

    aPoint2f_t scale = a_ViewportCalculateScale();
    float view_x = app.g_viewport.x - app.g_viewport.w;
    float view_y = app.g_viewport.y - app.g_viewport.h;
    int tw = g_map->tile_w;
    int th = g_map->tile_h;

    /* Detect hovered spawn point */
    int mouse_grid_x = 0, mouse_grid_y = 0;
    int mouse_on_map = 0;
    if ( !g_toggle_ascii )
    {
      mouse_on_map = e_GetCellAtMouseInViewport(
          g_map->width, g_map->height, tw, th,
          edit_menu_rect, originx, originy,
          &mouse_grid_x, &mouse_grid_y );
    }

    int new_hover = -1;
    if ( mouse_on_map )
    {
      new_hover = SpawnListFindAt( &g_edit_spawns,
                                    mouse_grid_x, mouse_grid_y );
    }
    g_hover_spawn_idx = new_hover;

    /* Global cycle timer — always ticks so all random/pool/class spawns animate */
    g_hover_cycle_timer += dt;
    if ( g_hover_cycle_timer >= 0.5f )
    {
      g_hover_cycle_timer -= 0.5f;
      g_hover_cycle_idx++;
    }

    for ( int i = 0; i < g_edit_spawns.count; i++ )
    {
      SpawnPoint_t* pt = &g_edit_spawns.points[i];

      /* row = grid_x (col in world), col = grid_y (row in world) */
      float world_x = originx + pt->row * tw + tw;
      float world_y = originy + pt->col * th + th;

      /* Skip spawns that would draw over the sidebar */
      float check_sx = ( world_x - tw - view_x ) * scale.y;
      if ( check_sx >= edit_menu_x ) continue;

      /* Try to blit the item image via viewport */
      int drew_image = 0;

      if ( pt->type == SPAWN_CONSUMABLE || pt->type == SPAWN_EQUIPMENT
           || pt->type == SPAWN_MAP )
      {
        int idx = EdItemByKey( pt->key );
        if ( idx >= 0 && g_ed_items[idx].image != NULL )
        {
          a_ViewportBlit( g_ed_items[idx].image, world_x, world_y );
          drew_image = 1;
        }
      }
      else if ( pt->type == SPAWN_CLASS_RARE || pt->type == SPAWN_CLASS_EQUIPMENT )
      {
        /* Always cycle through the 3 class variants */
        int ci = g_hover_cycle_idx % 3;
        int idx = EdItemByKey( pt->class_keys[ci] );
        if ( idx >= 0 && g_ed_items[idx].image != NULL )
        {
          a_ViewportBlit( g_ed_items[idx].image, world_x, world_y );
          drew_image = 1;
        }
      }
      else if ( pt->type == SPAWN_POOL )
      {
        if ( pt->pool_id >= 0 && pt->pool_id < g_edit_spawns.num_pools )
        {
          SpawnPool_t* pool = &g_edit_spawns.pools[pt->pool_id];

          if ( i == g_hover_spawn_idx )
          {
            /* Hovered: show the pick item (image or glyph) */
            const char* pk = pool->pick_key;
            if ( strcmp( pool->pick_type, "class_equipment" ) == 0 )
              pk = pool->pick_class_keys[0];
            int idx = EdItemByKey( pk );
            if ( idx >= 0 )
            {
              float sx = ( world_x - tw - view_x ) * scale.y;
              float sy = ( world_y - th - view_y ) * scale.y;
              float sz = tw * scale.y;
              aImage_t* img = g_ed_items[idx].image;
              if ( img )
              {
                float s = sz / img->rect.w;
                a_BlitRect( img, NULL, &(aRectf_t){ sx, sy, img->rect.w, img->rect.h }, s );
              }
              else if ( g_ed_items[idx].glyph[0] != '\0' )
              {
                a_DrawGlyph( g_ed_items[idx].glyph, (int)sx, (int)sy,
                             (int)sz, (int)sz, g_ed_items[idx].color,
                             (aColor_t){ 0, 0, 0, 0 }, FONT_CODE_PAGE_437 );
              }
              drew_image = 1;
            }
          }
          else
          {
            /* Not hovered: cycle through fill tier items */
            int* fill_arr = NULL;
            int  fill_cnt = 0;
            if ( strcmp( pool->fill_type, "random_t1" ) == 0 )
            { fill_arr = g_ed_t1_indices; fill_cnt = g_ed_t1_count; }
            else if ( strcmp( pool->fill_type, "random_t2" ) == 0 )
            { fill_arr = g_ed_t2_indices; fill_cnt = g_ed_t2_count; }

            if ( fill_arr && fill_cnt > 0 )
            {
              int ci = fill_arr[ g_hover_cycle_idx % fill_cnt ];
              if ( g_ed_items[ci].image != NULL )
              {
                a_ViewportBlit( g_ed_items[ci].image, world_x, world_y );
                drew_image = 1;
              }
            }
          }
        }
      }
      else if ( pt->type == SPAWN_RANDOM_T1 && g_ed_t1_count > 0 )
      {
        int ci = g_ed_t1_indices[ g_hover_cycle_idx % g_ed_t1_count ];
        if ( g_ed_items[ci].image != NULL )
        {
          a_ViewportBlit( g_ed_items[ci].image, world_x, world_y );
          drew_image = 1;
        }
      }
      else if ( pt->type == SPAWN_RANDOM_T2 && g_ed_t2_count > 0 )
      {
        int ci = g_ed_t2_indices[ g_hover_cycle_idx % g_ed_t2_count ];
        if ( g_ed_items[ci].image != NULL )
        {
          a_ViewportBlit( g_ed_items[ci].image, world_x, world_y );
          drew_image = 1;
        }
      }

      /* Fallback: colored rect for types without images */
      if ( !drew_image )
      {
        float sx = ( world_x - tw - view_x ) * scale.y;
        float sy = ( world_y - th - view_y ) * scale.y;
        float sw = tw * scale.y;
        float sh = th * scale.y;
        aRectf_t r = { .x = sx, .y = sy, .w = sw, .h = sh };

        aColor_t c = sp_green;
        switch ( pt->type )
        {
          case SPAWN_RANDOM_T1:       c = sp_green;   break;
          case SPAWN_RANDOM_T2:       c = sp_yellow;  break;
          case SPAWN_CONSUMABLE:      c = sp_cyan;    break;
          case SPAWN_EQUIPMENT:       c = sp_magenta; break;
          case SPAWN_CLASS_RARE:      c = sp_red;     break;
          case SPAWN_CLASS_EQUIPMENT: c = sp_red;     break;
          case SPAWN_POOL:            c = sp_orange;  break;
          default:                    c = sp_green;   break;
        }

        a_DrawFilledRect( r, c );
      }
    }

    /* Hover tooltip: show item name near mouse */
    if ( g_hover_spawn_idx >= 0 )
    {
      SpawnPoint_t* pt = &g_edit_spawns.points[g_hover_spawn_idx];
      const char* tooltip = NULL;

      if ( pt->type == SPAWN_CONSUMABLE || pt->type == SPAWN_EQUIPMENT
           || pt->type == SPAWN_MAP )
      {
        int idx = EdItemByKey( pt->key );
        if ( idx >= 0 ) tooltip = g_ed_items[idx].name;
      }
      else if ( pt->type == SPAWN_CLASS_RARE )
        tooltip = "Rare Consumable";
      else if ( pt->type == SPAWN_CLASS_EQUIPMENT )
        tooltip = "Class Equipment";
      else if ( pt->type == SPAWN_POOL )
      {
        if ( pt->pool_id >= 0 && pt->pool_id < g_edit_spawns.num_pools )
        {
          SpawnPool_t* pool = &g_edit_spawns.pools[pt->pool_id];
          static char pool_tip[256];
          if ( strcmp( pool->pick_type, "class_equipment" ) == 0 )
            snprintf( pool_tip, sizeof(pool_tip),
                      "pool_%d\npick: class_equipment\nmerc: %s\nrogue: %s\nmage: %s\nfill: %s",
                      pt->pool_id, pool->pick_class_keys[0],
                      pool->pick_class_keys[1], pool->pick_class_keys[2],
                      pool->fill_type );
          else
            snprintf( pool_tip, sizeof(pool_tip),
                      "pool_%d\npick: %s\nkey: %s\nfill: %s",
                      pt->pool_id, pool->pick_type, pool->pick_key, pool->fill_type );
          tooltip = pool_tip;
        }
      }
      else if ( pt->type == SPAWN_RANDOM_T1 )
        tooltip = "T1 Consumable";
      else if ( pt->type == SPAWN_RANDOM_T2 )
        tooltip = "T2 Consumable";

      if ( tooltip != NULL )
      {
        aTextStyle_t tip_ts = {
          .type = FONT_CODE_PAGE_437,
          .fg = white,
          .bg = { 0, 0, 0, 0 },
          .align = TEXT_ALIGN_LEFT,
          .wrap_width = 0,
          .scale = 1.0f,
          .padding = 0
        };

        /* Split tooltip into lines, measure size, draw popup box */
        const char* lines[8];
        int nlines = 0;
        static char tip_copy[256];
        strncpy( tip_copy, tooltip, sizeof(tip_copy) - 1 );
        tip_copy[sizeof(tip_copy) - 1] = '\0';
        char* tok = strtok( tip_copy, "\n" );
        while ( tok && nlines < 8 )
        {
          lines[nlines++] = tok;
          tok = strtok( NULL, "\n" );
        }

        aRectf_t gs = a_GetGlyphSize();
        int line_h = (int)gs.h;
        int max_w = 0;
        for ( int l = 0; l < nlines; l++ )
        {
          int w = (int)strlen( lines[l] ) * (int)gs.w;
          if ( w > max_w ) max_w = w;
        }

        int pad = 4;
        int px = app.mouse.x + 16;
        int py = app.mouse.y - 12;
        int box_w = max_w + pad * 2;
        int box_h = nlines * line_h + pad * 2;

        /* Border color = pick item glyph color for pools */
        aColor_t border = { 0x60, 0x60, 0x60, 255 };
        if ( pt->type == SPAWN_POOL
             && pt->pool_id >= 0 && pt->pool_id < g_edit_spawns.num_pools )
        {
          SpawnPool_t* pool = &g_edit_spawns.pools[pt->pool_id];
          const char* bk = pool->pick_key;
          if ( strcmp( pool->pick_type, "class_equipment" ) == 0 )
            bk = pool->pick_class_keys[0];
          int pidx = EdItemByKey( bk );
          if ( pidx >= 0 ) border = g_ed_items[pidx].color;
        }

        /* Border */
        a_DrawFilledRect( (aRectf_t){ px - 1, py - 1, box_w + 2, box_h + 2 }, border );
        /* Background */
        a_DrawFilledRect( (aRectf_t){ px, py, box_w, box_h },
                          (aColor_t){ 0x10, 0x14, 0x1f, 240 } );

        for ( int l = 0; l < nlines; l++ )
          a_DrawText( lines[l], px + pad, py + pad + l * line_h, tip_ts );
      }
    }
  }

  /* Update toggle button colors based on active state */
  {
    aColor_t active_bg   = { 0x38, 0xb7, 0x64, 255 };
    aColor_t inactive_bg = { 40, 40, 40, 255 };

    aWidget_t* room_btn = a_GetWidget( "toggle_room" );
    aWidget_t* cons_btn = a_GetWidget( "toggle_cons" );
    if ( room_btn ) room_btn->bg = g_toggle_room       ? active_bg : inactive_bg;
    if ( cons_btn ) cons_btn->bg = g_toggle_consumable  ? active_bg : inactive_bg;
  }

  a_DrawWidgets();

  /* Right-click context menu — drawn after widgets so it's in screen space */
  if ( ctx_open && g_map != NULL )
  {
    /* Highlight the targeted tile in viewport space */
    {
      aPoint2f_t scale = a_ViewportCalculateScale();
      float view_x = app.g_viewport.x - app.g_viewport.w;
      float view_y = app.g_viewport.y - app.g_viewport.h;
      int tw = g_map->tile_w;
      int th = g_map->tile_h;
      float twx = originx + ctx_row * tw + tw;
      float twy = originy + ctx_col * th + th;
      float tsx = ( twx - view_x ) * scale.y;
      float tsy = ( twy - view_y ) * scale.y;
      float tsw = tw * scale.y;
      float tsh = th * scale.y;
      a_DrawRect( (aRectf_t){ tsx, tsy, tsw, tsh },
                  (aColor_t){ 0xde, 0x9e, 0x41, 180 } );
    }

    /* Menu at saved screen position */
    float mx = ctx_screen_x;
    float my = ctx_screen_y;
    float ah = ctx_count * ( CTX_ED_ROW_H + CTX_ED_PAD ) - CTX_ED_PAD;

    a_DrawFilledRect( (aRectf_t){ mx, my, CTX_ED_W, ah },
                      (aColor_t){ 0x09, 0x0a, 0x14, 255 } );
    a_DrawRect( (aRectf_t){ mx, my, CTX_ED_W, ah },
                (aColor_t){ 0x39, 0x4a, 0x50, 255 } );

    aTextStyle_t ctx_ts = {
      .type = FONT_CODE_PAGE_437,
      .bg = { 0, 0, 0, 0 },
      .scale = 1.1f,
      .align = TEXT_ALIGN_LEFT,
      .wrap_width = 0,
      .padding = 0
    };

    for ( int i = 0; i < ctx_count; i++ )
    {
      float ry = my + i * ( CTX_ED_ROW_H + CTX_ED_PAD );
      aRectf_t row = { mx + 2, ry, CTX_ED_W - 4, CTX_ED_ROW_H };

      if ( i == ctx_cursor )
      {
        a_DrawFilledRect( row, (aColor_t){ 0x15, 0x1d, 0x28, 255 } );
        a_DrawRect( row, (aColor_t){ 0xde, 0x9e, 0x41, 255 } );
        ctx_ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
      }
      else
      {
        ctx_ts.fg = (aColor_t){ 0xc7, 0xcf, 0xcc, 255 };
      }

      a_DrawText( ctx_labels[i], (int)( row.x + 8 ), (int)( ry + 5 ), ctx_ts );
    }
  }
}

void we_EditDestroy( void )
{
  if ( clipboard != NULL )
  {

  }

  if ( undo_stack ) { d_ArrayDestroy( undo_stack ); undo_stack = NULL; }
  if ( redo_stack ) { d_ArrayDestroy( redo_stack ); redo_stack = NULL; }

  if ( g_spawns_loaded )
  {
    SpawnListDestroy( &g_edit_spawns );
    g_spawns_loaded = 0;
  }
}

