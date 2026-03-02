/*
 * @file world.c
 *
 * This file defines the functions used to create, display, and manipulate
 * worlds.
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>

#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"
#include "world.h"

World_t* WorldCreate( const int width, const int height,
                      const int tile_w, const int tile_h )
{
  World_t* new_world = malloc( sizeof( World_t ) );
  if ( new_world == NULL ) return NULL;

  new_world->width  = width;
  new_world->height = height;
  new_world->tile_w = tile_w;
  new_world->tile_h = tile_h;
  new_world->tile_count = width * height;

  size_t layer_size = sizeof( Tile_t ) * new_world->tile_count;
  
  new_world->room_ids = malloc( sizeof( uint16_t ) * new_world->tile_count );
  if ( new_world->room_ids == NULL )
  {
    free( new_world );
    return NULL;
  }

  new_world->background = malloc( layer_size );
  if ( new_world->background == NULL )
  {
    free( new_world );
    return NULL;
  }

  new_world->midground = malloc( layer_size );
  if ( new_world->midground == NULL )
  {
    free( new_world->background );
    free( new_world );
    return NULL;
  }

  new_world->foreground = malloc( layer_size );
  if ( new_world->foreground == NULL )
  {
    free( new_world->midground );
    free( new_world->background );
    free( new_world );
    return NULL;
  }

  for ( int i = 0; i < new_world->tile_count; i++ )
  {
    new_world->room_ids[i]               = 0;
    new_world->background[i].solid       = 0;
    new_world->background[i].glyph_index = 0;
    new_world->background[i].tile        = 0;
    new_world->background[i].glyph       = ".";
    new_world->background[i].glyph_fg    = (aColor_t){ 0x39, 0x4a, 0x50, 255 };
    new_world->background[i].glyph_bg    = (aColor_t){ 0x09, 0x0a, 0x14, 255 };

    new_world->midground[i].solid       = 0;
    new_world->midground[i].glyph_index = 0;
    new_world->midground[i].tile        = TILE_EMPTY;
    new_world->midground[i].glyph       = "";
    new_world->midground[i].glyph_fg    = (aColor_t){ 0xc7, 0xcf, 0xcc, 255 };
    new_world->midground[i].glyph_bg    = (aColor_t){ 0, 0, 0, 0 };

    new_world->foreground[i].solid       = 0;
    new_world->foreground[i].glyph_index = 0;
    new_world->foreground[i].tile        = TILE_EMPTY;
    new_world->foreground[i].glyph       = "";
    new_world->foreground[i].glyph_fg    = (aColor_t){ 0xc7, 0xcf, 0xcc, 255 };
    new_world->foreground[i].glyph_bg    = (aColor_t){ 0, 0, 0, 0 };
  }

  return new_world;
}

/* Legacy renderer - used by the editor. Game uses GV_DrawWorld instead. */
void WorldDraw( const int x_off, const int y_off,
                World_t* world, Tileset_t* tile_set,
                const uint8_t draw_ascii )
{
  int has_viewport = ( app.g_viewport.w != 0 && app.g_viewport.h != 0 );
  aRectf_t glyph_size = a_GetGlyphSize();

  for ( int i = 0; i < world->tile_count; i++ )
  {
    int x_tile = i % world->width;
    int y_tile = i / world->width;
    int x, y;

    if ( draw_ascii )
    {
      x = ( x_tile * glyph_size.w ) + x_off;
      y = ( y_tile * glyph_size.h ) + y_off;
    }
    else
    {
      x = ( x_tile * world->tile_w ) + x_off;
      y = ( y_tile * world->tile_h ) + y_off;
    }

    float draw_x = x;
    float draw_y = y;

    if ( has_viewport && !draw_ascii )
    {
      draw_x += world->tile_w;
      draw_y += world->tile_h;

      /*if ( x + world->tile_w < ( app.g_viewport.x - app.g_viewport.w ) ||
           x > ( app.g_viewport.x + app.g_viewport.w ) ||
           y + world->tile_h < ( app.g_viewport.x - app.g_viewport.w ) ||
           y > ( app.g_viewport.x + app.g_viewport.w ) )
      {
        continue;
      }*/
    }

    Tile_t bg = world->background[i];
    Tile_t mg = world->midground[i];
    Tile_t fg = world->foreground[i];
    int has_mg = ( mg.tile != TILE_EMPTY );
    int has_fg = ( fg.tile != TILE_EMPTY );

    if ( draw_ascii )
    {
      aTextStyle_t ts = {
        .type = FONT_CODE_PAGE_437,
        .align = TEXT_ALIGN_CENTER,
        .wrap_width = 0,
        .scale = 1.0f,
        .padding = 0
      };

      ts.fg = bg.glyph_fg;
      ts.bg = bg.glyph_bg;

      a_ViewportBlitTextureRect( app.font_textures[app.font_type],
                                 &app.glyphs[app.font_type][bg.glyph_index],
                                 x, y, bg.glyph_bg );
      //a_DrawText( bg.glyph, x, y, ts );

      if ( has_mg && mg.glyph[0] != '\0' )
      {
        ts.fg = mg.glyph_fg;
        ts.bg = mg.glyph_bg;
        a_ViewportBlitTextureRect( app.font_textures[app.font_type],
                                  &app.glyphs[app.font_type][mg.glyph_index],
                                  x, y, mg.glyph_bg );
        //a_DrawText( mg.glyph, x, y, ts );
      }

      if ( has_fg && fg.glyph[0] != '\0' )
      {
        ts.fg = fg.glyph_fg;
        ts.bg = fg.glyph_bg;
        a_ViewportBlitTextureRect( app.font_textures[app.font_type],
                                  &app.glyphs[app.font_type][fg.glyph_index],
                                  x, y, fg.glyph_bg );
        //a_DrawText( fg.glyph, x, y, ts );
      }
    }
    else if ( has_viewport )
    {
      a_ViewportBlit( tile_set->img_array[bg.tile].img, draw_x, draw_y );
      if ( has_mg ) a_ViewportBlit( tile_set->img_array[mg.tile].img, draw_x, draw_y );
      if ( has_fg ) a_ViewportBlit( tile_set->img_array[fg.tile].img, draw_x, draw_y );
    }
    else
    {
      a_Blit( tile_set->img_array[bg.tile].img, x, y );
      if ( has_mg ) a_Blit( tile_set->img_array[mg.tile].img, x, y );
      if ( has_fg ) a_Blit( tile_set->img_array[fg.tile].img, x, y );
    }
  }
}

