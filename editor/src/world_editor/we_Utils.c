/*
 * world_editor/we_Utils.c:
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
#include "utils.h"
#include "world.h"
#include "world_editor.h"

void we_DrawColorPalette( int originx, int originy, int fg_index, int bg_index )
{
  int cx = 0, cy = 0;
  for ( int i = 0; i < MAX_COLOR_PALETTE; i++ )
  {

    if ( cx + GLYPH_WIDTH > ( 6 * GLYPH_WIDTH ) )
    {
      cx = 0;
      cy += GLYPH_HEIGHT;
      if ( cy > ( 6 * GLYPH_HEIGHT ) )
      {
        //        printf( "color grid is too large!\n" );
        //        return;
      }

    }
    aRectf_t color_palette_rect = (aRectf_t){ .x = ( cx + originx ), .y = ( cy + originy ),
    .w = GLYPH_WIDTH, .h = GLYPH_HEIGHT };
    a_DrawFilledRect( color_palette_rect, master_colors[APOLLO_PALETE][i] );

    if ( i == fg_index )
    {
      aRectf_t select_rect = {
        .x = color_palette_rect.x-1,
        .y = color_palette_rect.y-1,
        .w = color_palette_rect.w+1,
        .h = color_palette_rect.h+1
      };
      a_DrawRect( select_rect, magenta );
    }

    if ( i == bg_index )
    {
      aRectf_t select_rect = {
        .x = color_palette_rect.x-1,
        .y = color_palette_rect.y-1,
        .w = color_palette_rect.w+1,
        .h = color_palette_rect.h+1
      };
      a_DrawRect( select_rect, yellow );
    }

    cx += GLYPH_WIDTH;
  }
}

void we_DrawGlyphPalette( int originx, int originy, int glyph_index )
{
  int gx = 0, gy = 0;
  int width = 16, height = 20;
  for ( int i = 0; i < 254; i++ )
  {
    if ( gx + GLYPH_WIDTH > ( width * GLYPH_WIDTH ) )
    {
      gx = 0;
      gy += GLYPH_HEIGHT;
    }
    
    aRectf_t glyph_palette_rect = (aRectf_t){
      .x = ( gx + originx ),
      .y = ( gy + originy ),
      .w = app.glyphs[app.font_type][i].w,
      .h = app.glyphs[app.font_type][i].h
    };

    a_DrawGlyph_special( i, glyph_palette_rect,
                        white, black, FONT_CODE_PAGE_437 );
    
    if ( i == glyph_index )
    {
      a_DrawRect( glyph_palette_rect, magenta );
    }

    gx += GLYPH_WIDTH;
  }
}

void we_DrawTilePalette( int originx, int originy, int tile_index, int tileset )
{
  for ( int i = 0; i < g_tile_sets[tileset]->tile_count; i++ )
  {
    int row = i % g_tile_sets[tileset]->col;
    int col = i / g_tile_sets[tileset]->col;

    int x = ( row * g_tile_sets[tileset]->tile_w ) + originx;
    int y = ( col * g_tile_sets[tileset]->tile_h ) + originy;

    a_Blit( g_tile_sets[tileset]->img_array[i].img, x, y );
    aRectf_t dest = {
      .x = x,
      .y = y,
      .w = g_tile_sets[tileset]->tile_w,
      .h = g_tile_sets[tileset]->tile_h
    };
    //a_BlitRect( tile_sets[tileset]->img_array[i].img, NULL, &dest, 2 );
    
    if ( i == tile_index )
    {
      aRectf_t rect = {
        .x = x,
        .y = y,
        .w = g_tile_sets[tileset]->tile_w,
        .h = g_tile_sets[tileset]->tile_h,
      };
      a_DrawRect( rect, magenta );
    }
  }
}

void we_MapMouseCheck( dVec2_t* pos, aRectf_t menu_rect )
{
  if ( g_map == NULL || pos == NULL ) return;
  int originx = 0, originy = 0;
  int grid_x = 0, grid_y =0;
  int clicked = 0;
  e_GetOrigin( g_map, &originx, &originy );
  clicked = e_GetCellAtMouseInViewport( g_map->width, g_map->height,
                                        g_map->tile_w, g_map->tile_h,
                                        menu_rect, 
                                        originx, originy,
                                        &grid_x, &grid_y );
  if ( clicked )
  {
    pos->x = grid_x;
    pos->y = grid_y;
  }
}

/*static void GetSelectGridSize( dVec2_t* select_pos, dVec2_t* highlight_pos,
                               int* grid_w, int* grid_h,
                               int* current_x, int* current_y )
{
  aPoint2f_t scale = a_ViewportCalculateScale();
  float view_x = app.g_viewport.x - app.g_viewport.w;
  float view_y = app.g_viewport.y - app.g_viewport.h;

  float world_mouse_x = ( select_pos->x / scale.y ) + view_x;
  float world_mouse_y = ( select_pos->y / scale.y ) + view_y;

  float r_select_x = world_mouse_x - originx;
  float r_select_y = world_mouse_y - originy;
  
  float world_highlight_x = ( highlight_pos->x / scale.y ) + view_x;
  float world_highlight_y = ( highlight_pos->y / scale.y ) + view_y;

  float r_highlight_x = world_highlight_x - originx;
  float r_highlight_y = world_highlight_y - originy;

  *current_x = global_pos_x;
  *current_y = global_pos_y;
  *current_z = pos.local_z;

  *grid_w = ( global_highlight_x - global_pos_x );
  *grid_h = ( global_highlight_y - global_pos_y );

  if ( *grid_w < 0 )
  {
    *grid_w = ( global_pos_x - global_highlight_x );
    *current_x = global_highlight_x;
  }

  if ( *grid_h < 0 )
  {
    *grid_h = ( global_pos_y - global_highlight_y );
    *current_y = global_highlight_y;
  }
  }
  
  else
  {
    *grid_w    = ( highlight.x - pos.x );
    *grid_h    = ( highlight.y - pos.y );
    *current_x = pos.x;
    *current_y = pos.y;
    *current_z = pos.local_z;

    if ( *grid_w < 0 )
    {
      *grid_w = ( pos.x - highlight.x );
      *current_x = highlight.x;
    }

    if ( *grid_h < 0 )
    {
      *grid_h = ( pos.y - highlight.y );
      *current_y = highlight.y;
    }
  }
}*/

