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

static dVec2_t selected_pos;
static dVec2_t highlighted_pos;
static int glyph_index = 0;
static int fg_index    = 0;
static int bg_index    = 0;
static int tile_index  = 0;
static aPoint2i_t selected_glyph;
static aPoint2i_t selected_fg;
static aPoint2i_t selected_bg;
static aPoint2i_t selected_tile;
static int editor_mode = 0;
static Tile_t* clipboard = NULL;
static int clipbard_amout = 0;

static int originx = 0;
static int originy = 0;

static int edit_menu_x = 1120;
static int edit_menu_y = 100;

static int color_x = 0;
static int color_y = 0;
static int glyph_x = 0;
static int glyph_y = 0;
static int tile_x  = 0;
static int tile_y  = 0;

static void we_EditLogic( float dt );
static void we_EditDraw( float dt );

char* wem_strings[WEM_MAX+1] =
{
  "WEM_NONE",
  "WEM_BRUSH",
  "WEM_COPY",
  "WEM_PASTE",
  "WEM_MASS_CHANGE",
  "WEM_SELECT",
  "WEM_MAX"
};

void we_Edit( void )
{
  app.delegate.logic = we_EditLogic;
  app.delegate.draw  = we_EditDraw;

  e_GetOrigin( g_map, &originx, &originy );
  
  color_x = edit_menu_x + 5;
  color_y = edit_menu_y + 5;
  
  tile_x  = edit_menu_x + 5;
  tile_y  = (edit_menu_y + (8 * GLYPH_HEIGHT)) + 5;
  glyph_x = edit_menu_x + 5;
  glyph_y = edit_menu_y + tile_y +
    (g_tile_sets[g_current_tileset]->col + g_tile_sets[g_current_tileset]->tile_h) + 5;

  app.active_widget = a_GetWidget( "generation_menu" );
  aContainerWidget_t* container = a_GetContainerFromWidget( "generation_menu" );
  app.active_widget->hidden = 1;

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    current->hidden = 1;
  }
}

static void we_EditLogic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[A_ESCAPE] == 1 )
  {
    app.keyboard[A_ESCAPE] = 0;
    if ( editor_mode == WEM_NONE )
    {
      e_WorldEditorInit();
    }
  
    else
    {
      editor_mode = WEM_NONE;
    }
  }
  
  if ( app.mouse.button == 1 && app.mouse.state == 1 )
  {
    app.mouse.button = 0, app.mouse.state = 0;
    e_ColorMouseCheck( color_x, color_y,
                       &bg_index, &selected_bg.x, &selected_bg.y, 0 );
    e_GlyphMouseCheck( glyph_x, glyph_y,
                       &glyph_index, &selected_glyph.x, &selected_glyph.y, 0 );
    e_TilesetMouseCheck( tile_x, tile_y,
                         &tile_index, &selected_tile.x, &selected_tile.y, 0 );
    
    if ( g_map != NULL )
    {
      int grid_x = 0, grid_y = 0;
      if ( !g_toggle_ascii )
      {
        e_GetCellAtMouseInViewport( g_map->width,  g_map->height,
                                   g_map->tile_w, g_map->tile_h,
                                   originx, originy, &grid_x, &grid_y );
      }

      if ( editor_mode == WEM_SELECT )
      {

      }

      if ( editor_mode == WEM_NONE )
      {
        int index = grid_y * g_map->width + grid_x;
        if ( !g_toggle_room )
        {
          if ( tile_index >= TILE_BLUE_DOOR_EW && tile_index <= TILE_WHITE_DOOR_NS )
          {
            e_UpdateTile( index, TILE_LVL1_FLOOR, tile_index, TILE_EMPTY );
          }

          else
          {
            e_UpdateTile( index, tile_index, TILE_EMPTY, TILE_EMPTY );
          }
        }

        else
        {
          g_map->room_ids[index] = glyph_index;
        }
      }
    }
  }
  
  if ( app.mouse.button == 3 && app.mouse.state == 1 )
  {
    app.mouse.button = 0, app.mouse.state = 0;
    e_ColorMouseCheck( color_x, color_y,
                      &fg_index, &selected_fg.x, &selected_fg.y, 0 );
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

    editor_mode = WEM_COPY;
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
  }
  
  a_ViewportInput( &app.g_viewport, EDITOR_WORLD_WIDTH, EDITOR_WORLD_HEIGHT );
  
  a_DoWidget();
}

static void we_EditDraw( float dt )
{
  if ( g_map != NULL )
  {
    WorldDraw( originx, originy, g_map, g_tile_sets[g_current_tileset],
               g_toggle_room, g_toggle_ascii );
  }
  
  aRectf_t rect = {
    .x = edit_menu_x,
    .y = edit_menu_y,
    .w = 155,
    .h = 520
  };
  a_DrawFilledRect( rect, blue );
  
  aTextStyle_t ts = {
    .type = FONT_CODE_PAGE_437,
    .align = TEXT_ALIGN_CENTER,
    .wrap_width = 0,
    .scale = 1.0f,
    .padding = 0
  };
  
  ts.bg = master_colors[APOLLO_PALETE][bg_index];
  ts.fg = master_colors[APOLLO_PALETE][fg_index];

  a_Blit( g_tile_sets[g_current_tileset]->img_array[tile_index].img,
          1100, color_y );
  
  aRectf_t glyph_rect = {
    .x = 1100,
    .y = color_y + 20,
    .w = GLYPH_WIDTH,
    .h = GLYPH_HEIGHT
  };

  a_DrawGlyph_special( glyph_index, glyph_rect,
                       ts.fg, ts.bg, FONT_CODE_PAGE_437 );


  we_DrawColorPalette( color_x, color_y, fg_index, bg_index );
  we_DrawGlyphPalette( glyph_x, glyph_y, glyph_index );
  we_DrawTilePalette(  tile_x, tile_y, tile_index, g_current_tileset );

  a_DrawWidgets();
}

void we_EditDestroy( void )
{
  if ( clipboard != NULL )
  {
    
  }
}

