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

