/* 
 * @file world.c
 *
 * This file defines the functions used to create, display, and manipulate
 * worlds.
 *
 * Copyright (c) 2025 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>

#include <Archimedes.h>
#include <Daedalus.h>

#include "defines.h"
#include "world.h"

World_t* WorldCreate( int width, int height, int tile_w, int tile_h )
{
  World_t* new_world = malloc( sizeof( World_t ) );
  if ( new_world == NULL ) return NULL;
  
  new_world->rows = width;
  new_world->cols = height;
  new_world->tile_w = tile_w;
  new_world->tile_h = tile_h;
  new_world->tile_count = new_world->rows * new_world->cols;
  
  new_world->background = malloc( sizeof( Tile_t ) * 
                           ( new_world->rows * new_world->cols ) );
  
  if ( new_world->background == NULL )
  {
    free( new_world );
    return NULL;
  }
  
  new_world->midground = malloc( sizeof( Tile_t ) * 
                           ( new_world->rows * new_world->cols ) );
  
  if ( new_world->midground == NULL )
  {
    free( new_world );
    return NULL;
  }
  
  new_world->foreground = malloc( sizeof( Tile_t ) * 
                           ( new_world->rows * new_world->cols ) );
  
  if ( new_world->foreground == NULL )
  {
    free( new_world );
    return NULL;
  }

  for ( int i = 0; i < ( new_world->rows * new_world->cols ); i++ )
  {
    new_world->background[i].solid = 1;
    new_world->background[i].tile  = 0;
    new_world->background[i].glyph = "@";
    
    new_world->midground[i].solid = 0;
    new_world->midground[i].tile  = 48; //nothing
    new_world->midground[i].glyph = " ";
    
    new_world->foreground[i].solid = 0;
    new_world->foreground[i].tile  = 48; //nothing
    new_world->foreground[i].glyph = " ";
  }

  return new_world;
}

void WorldDraw( int x_off, int y_off,
                    World_t* world, aTileset_t* tile_set,
                    uint8_t draw_ascii )
{
  for ( int i = 0; i < world->tile_count; i++ )
  {
    aRectf_t glyph_size = a_GetGlyphSize();

    int row = i % world->cols;
    int col = i / world->cols;
    int x, y;

    if ( draw_ascii )
    {
      x = (row * glyph_size.w)+x_off;
      y = (col * glyph_size.h)+y_off;
    }
  
    else
    {
      x = (row * world->tile_w)+x_off;
      y = (col * world->tile_h)+y_off;
    }
    
    Tile_t bg_img_index = world->background[i];
    Tile_t mg_img_index = world->midground[i];
    Tile_t fg_img_index = world->foreground[i];
    aImage_t* bg_tile_img = tile_set[bg_img_index.tile].img;
    aImage_t* mg_tile_img = tile_set[mg_img_index.tile].img;
    aImage_t* fg_tile_img = tile_set[fg_img_index.tile].img;
    
    aTextStyle_t text_style = {
      .type = FONT_CODE_PAGE_437,
      .fg = white,
      .bg = black,
      .align = TEXT_ALIGN_CENTER,
      .wrap_width = 0,
      .scale = 1.0f,
      .padding = 0
    };

    if ( app.g_viewport.x != 0 && app.g_viewport.y != 0
         && app.g_viewport.w != 0 && app.g_viewport.h != 0 )
    {
      if ( draw_ascii )
      {
        a_DrawText( bg_img_index.glyph, x, y, text_style );
        a_DrawText( mg_img_index.glyph, x, y, text_style );
        a_DrawText( fg_img_index.glyph, x, y, text_style );
      }
      else
      {
        a_ViewportBlit( bg_tile_img, x, y );
        a_ViewportBlit( mg_tile_img, x, y );
        a_ViewportBlit( fg_tile_img, x, y );
      }
    }
    
    else
    {
      if ( draw_ascii )
      {
        a_DrawText( bg_img_index.glyph, x, y, text_style );
        a_DrawText( mg_img_index.glyph, x, y, text_style );
        a_DrawText( fg_img_index.glyph, x, y, text_style );
      }
 
      else
      {
        a_Blit( bg_tile_img, x, y );
        a_Blit( mg_tile_img, x, y );
        a_Blit( fg_tile_img, x, y );
      }
    }
  }
}

