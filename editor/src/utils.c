
#include <stdio.h>
#include <stdlib.h>

#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"

#include "editor.h"
#include "tile.h"
#include "utils.h"
#include "world_editor.h"

static int string_width( dString_t* string, int start );

void e_GetOrigin( World_t* world, int* originx, int* originy )
{
  if ( !world ) return;
  *originx = app.g_viewport.x - ( (float)( world->width  * world->tile_w ) / 2 );
  *originy = app.g_viewport.y - ( (float)( world->height * world->tile_h ) / 2 );
}

void e_GetCellAtMouseInViewport( const int width,   const int height,
                                 const int tile_w,  const int tile_h,
                                 const int originx, const int originy,
                                 int* grid_x, int* grid_y )
{
  aPoint2f_t scale = a_ViewportCalculateScale();
  float view_x = app.g_viewport.x - app.g_viewport.w;
  float view_y = app.g_viewport.y - app.g_viewport.h;

  float world_mouse_x = ( app.mouse.x / scale.y ) + view_x;
  float world_mouse_y = ( app.mouse.y / scale.y ) + view_y;

  float relative_x = world_mouse_x - originx;
  float relative_y = world_mouse_y - originy;

  int cell_x = (int)( relative_x / 16 );
  int cell_y = (int)( relative_y / 16 );
  
  int extreme_w = ( width * tile_w );
  int extreme_h = ( height * tile_h );

  if ( cell_x >= 0 && cell_x < extreme_w &&
       cell_y >= 0 && cell_y < extreme_h )
  {
    *grid_x = cell_x;
    *grid_y = cell_y;
  }
}

void e_GetCellAtMouse( const int width,      const int height,
                       const int originx,    const int originy,
                       const int cell_width, const int cell_height,
                       int* grid_x,    int* grid_y, const int centered )
{
  int edge_x = 0;
  int edge_y = 0;

  if ( centered == 1 )
  {
    edge_x = originx - ( ( width  * cell_width  ) / 2 );
    edge_y = originy - ( ( height * cell_height ) / 2 );
  }
  
  else
  {
    edge_x = originx;
    edge_y = originy;
  }

  if ( app.mouse.x > edge_x && app.mouse.x <= ( edge_x + ( width  * cell_width ) ) &&
       app.mouse.y > edge_y && app.mouse.y <= ( edge_y + ( height * cell_height ) ) )
  {
    int mousex = ( ( app.mouse.x - edge_x ) / cell_width  );
    int mousey = ( ( app.mouse.y - edge_y ) / cell_height );
    
    *grid_x = mousex;
    *grid_y = mousey;
  }
}

void e_TilesetMouseCheck( const int originx, const int originy, int* index,
                      int* grid_x, int* grid_y, const int centered )
{

  int row = g_tile_sets[g_current_tileset]->row;
  int col = g_tile_sets[g_current_tileset]->col;
  int tile_w = g_tile_sets[g_current_tileset]->tile_w;
  int tile_h = g_tile_sets[g_current_tileset]->tile_h;

  e_GetCellAtMouse( col, row,
                    originx, originy,
                    tile_w, tile_h,
                    grid_x, grid_y, centered );

  *index = *grid_y * col + *grid_x;
}

void e_GlyphMouseCheck( const int originx, const int originy, int* index,
                        int* grid_x, int* grid_y, const int centered )
{
  int glyph_grid_w = 16;
  int glyph_grid_h = 16;

  e_GetCellAtMouse( glyph_grid_w, glyph_grid_h,
                    originx, originy,
                    GLYPH_WIDTH, GLYPH_HEIGHT,
                    grid_x, grid_y, centered );

  *index = *grid_y * glyph_grid_w + *grid_x;
}

void e_ColorMouseCheck( const int originx, const int originy, int* index,
                        int* grid_x, int* grid_y, const int centered )
{
  int color_grid_w = 6;
  int color_grid_h = 8;

  e_GetCellAtMouse( color_grid_w, color_grid_h,
                    originx, originy,
                    GLYPH_WIDTH, GLYPH_HEIGHT,
                    grid_x, grid_y, centered );

  *index = *grid_y * color_grid_w + *grid_x;
}

Tileset_t* e_TilesetCreate( const char* filename,
                             const int tile_w, const int tile_h )
{
  aSpriteSheet_t* temp_sheet = a_SpriteSheetCreate( filename, tile_w, tile_h );
  
  Tileset_t* new_set = malloc( sizeof( Tileset_t ) );
  if ( new_set == NULL ) return NULL;

  new_set->img_array = malloc( sizeof( ImageArray_t ) * temp_sheet->img_count );
  if ( new_set->img_array == NULL ) return NULL;
  
  new_set->glyph = malloc( sizeof( uint16_t ) * temp_sheet->img_count );
  if ( new_set->glyph == NULL ) 
  {
    free( new_set->glyph );
    free( new_set );
    return NULL;
  }

  new_set->tile_count = temp_sheet->img_count;
  new_set->row = temp_sheet->h_count;
  new_set->col = temp_sheet->v_count;
  new_set->tile_w = tile_w;
  new_set->tile_h = tile_h;

  for ( int i = 0; i < temp_sheet->img_count; i++ )
  {
    int row = i % temp_sheet->v_count;
    int col = i / temp_sheet->v_count;
    new_set->img_array[i].img = a_ImageFromSpriteSheet( temp_sheet, row, col );
    new_set->glyph[i] = 0;
  }

  a_SpriteSheetDestroy( temp_sheet );

  return new_set;
}

void e_TilesetDestroy( Tileset_t* t_set )
{
  if ( t_set == NULL ) return;

  free( t_set->glyph );
  free( t_set->img_array );
  free( t_set );
}

void e_UpdateTile( int index, int bg_tile, int mg_tile, int fg_tile )
{
  g_map->background[index].tile        = bg_tile;
  g_map->background[index].glyph_index = TileGlyphConverter( bg_tile );
  g_map->midground[index].tile         = mg_tile;
  g_map->midground[index].glyph_index  = TileGlyphConverter( mg_tile );
  g_map->foreground[index].tile        = fg_tile;
  g_map->foreground[index].glyph_index = TileGlyphConverter( fg_tile );
}

void e_LoadColorPalette( aColor_t palette[MAX_COLOR_GROUPS][MAX_COLOR_PALETTE],
                       const char * filename )
{
  FILE* file;
  char line[8];
  unsigned int hex_value;
  uint8_t r, g, b;
  int i = 0;

  file = fopen( filename, "rb" );
  if ( file == NULL )
  {
    printf( "Failed to open file %s\n", filename );
    return;
  } 

  while( fgets( line, sizeof( line ), file ) != NULL )
  {
    hex_value = ( unsigned int )strtol( line, NULL, 16 );
    r = hex_value >> 16;
    g = hex_value >> 8;
    b = hex_value;
    
    if ( i >= 0 && i < MAX_COLOR_PALETTE )
    {
      palette[APOLLO_PALETE][i].r = r;
      palette[APOLLO_PALETE][i].g = g;
      palette[APOLLO_PALETE][i].b = b;
      palette[APOLLO_PALETE][i].a = 255;

    }

    i++;

  }

  fclose( file );
}

static int string_width( dString_t* string, int start )
{
  int width_count = 0;
  const char* raw_str = d_StringPeek(string);
  for ( size_t i = start; i < string->len; i++ )
  {
    if ( raw_str[i] == ' ' )
    {
      break;
    }

    width_count++;
  }

  return width_count;
}

World_t* convert_mats_worlds( const char* filename )
{
  int file_size = 0;
  int newline_count = 0;
  char* file_string;
  char** lines;
  
  int world_width  = 0;
  int world_height = 0;
  
  dArray_t* door_positions = d_ArrayInit( 10, sizeof( dVec2_t ) );

  World_t* new_world = malloc( sizeof( World_t ) );
  if ( new_world == NULL ) return NULL;

  file_string = a_ReadFile( filename, &file_size );
  
  newline_count = a_CountNewLines( file_string, file_size );

  lines = a_ParseLinesInFile( file_string, file_size, newline_count );
   
  char* string = lines[0];
  if ( string != NULL )
  {
    if ( string[0] == '/' && string[1] == '/' )
    {
      dString_t* line   = d_StringInit();
      dString_t* width  = d_StringInit();
      dString_t* height = d_StringInit();
      d_StringSet( line, string );
      int num_start = 3;
      int first_num_w  = string_width( line, num_start );
      first_num_w += num_start;
      int second_num_w = string_width( line, first_num_w+1 );
      second_num_w += first_num_w+1;

      d_StringSlice( width,  d_StringPeek( line ), 3, num_start+first_num_w );
      d_StringSlice( height, d_StringPeek( line ), first_num_w+1, second_num_w );
      world_width  = atoi( d_StringPeek( width ) );
      world_height = atoi( d_StringPeek( height ) );
    }

    new_world->width  = world_width;
    new_world->height = world_height;

    new_world->tile_w = 16;
    new_world->tile_h = 16;

    new_world->tile_count = world_width * world_height;
    new_world->filename = malloc( sizeof(char) * MAX_FILENAME_LENGTH );
    if (new_world->filename == NULL ) return NULL;
    STRNCPY(new_world->filename, filename, MAX_FILENAME_LENGTH);

    new_world->background = malloc( sizeof(Tile_t) * new_world->tile_count );
    if ( new_world->background == NULL ) return NULL;
    memset( new_world->background, 0, sizeof(Tile_t) * new_world->tile_count );
    
    new_world->midground = malloc( sizeof(Tile_t) * new_world->tile_count );
    if ( new_world->midground == NULL ) return NULL;
    memset( new_world->midground, 0, sizeof(Tile_t) * new_world->tile_count );
    
    new_world->foreground = malloc( sizeof(Tile_t) *  new_world->tile_count );
    if ( new_world->foreground == NULL ) return NULL;
    memset( new_world->foreground, 0, sizeof(Tile_t) * new_world->tile_count );
    
    new_world->room_ids = malloc( sizeof(uint16_t) * new_world->tile_count );
    if ( new_world->room_ids == NULL ) return NULL;
    memset( new_world->room_ids, 0, sizeof(uint16_t) * new_world->tile_count );
    
    int line_pos = 1;
    for ( int j = 0; j < world_height; j++ )
    {
      for ( int i = 0; i < world_width; i++ )
      {
        char* current_line = lines[line_pos];
        int tile = current_line[i];
        int index = j * world_width + i;
        
        new_world->background[index].solid       = 0;
        new_world->background[index].tile        = 0;
        new_world->background[index].glyph_index = TILE_EMPTY;
        new_world->background[index].glyph       = ".";
        new_world->background[index].glyph_fg    = (aColor_t){ 0x39, 0x4a, 0x50, 255 };
        new_world->background[index].glyph_bg    = (aColor_t){ 0x09, 0x0a, 0x14, 255 };

        new_world->midground[index].solid       = 0;
        new_world->midground[index].tile = TILE_EMPTY;
        new_world->midground[index].glyph_index = TILE_EMPTY;
        new_world->midground[index].glyph       = "";
        new_world->midground[index].glyph_fg    = (aColor_t){ 0xc7, 0xcf, 0xcc, 255 };
        new_world->midground[index].glyph_bg    = (aColor_t){ 0, 0, 0, 0 };

        new_world->foreground[index].solid       = 0;
        new_world->foreground[index].tile = TILE_EMPTY;
        new_world->foreground[index].glyph_index = TILE_EMPTY;
        new_world->foreground[index].glyph       = "";
        new_world->foreground[index].glyph_fg    = (aColor_t){ 0xc7, 0xcf, 0xcc, 255 };
        new_world->foreground[index].glyph_bg    = (aColor_t){ 0, 0, 0, 0 };
        
        new_world->room_ids[index] = TILE_EMPTY;

        if ( tile == TILE_GLYPH_WALL || tile == TILE_GLYPH_FLOOR )
        {
          new_world->background[index].tile = GlyphTileConverter( tile, 0 );
          new_world->background[index].glyph_index = tile-1;
        }

        else if ( tile == TILE_GLYPH_RED_DOOR ||
                  tile == TILE_GLYPH_GREEN_DOOR ||
                  tile == TILE_GLYPH_BLUE_DOOR ||
                  tile == TILE_GLYPH_WHITE_DOOR )
        {
          new_world->background[index].tile        = TILE_LVL1_FLOOR;
          new_world->background[index].glyph_index = tile-1;
          new_world->midground[index].tile = GlyphTileConverter( tile, 0 );
          new_world->midground[index].glyph_index = tile;
          dVec2_t pos = { .x = i, .y = j };
          d_ArrayAppend( door_positions, &pos );
        }

        else
        {
          new_world->background[index].tile = TILE_LVL1_FLOOR;
          new_world->background[index].glyph_index = TILE_GLYPH_FLOOR-1;
          new_world->midground[index].tile = TILE_EMPTY;
          new_world->midground[index].glyph_index = TILE_EMPTY;
          new_world->room_ids[index] = tile-1;
        }
      }

      line_pos++;
    }
  }

  for ( size_t i = 0; i < door_positions->count; i++ )
  {
    dVec2_t* pos = d_ArrayGet( door_positions, i );
    int door_index = pos->y * world_width + pos->x;
    int tile_index = new_world->midground[door_index].glyph_index;
    
    dVec2_t n = { .x = pos->x ,    .y = pos->y + 1 };
    dVec2_t e = { .x = pos->x + 1, .y = pos->y };
    dVec2_t s = { .x = pos->x,     .y = pos->y - 1 };
    dVec2_t w = { .x = pos->x - 1, .y = pos->y };
    
    int index_n = n.y * world_width + n.x;
    int index_e = e.y * world_width + e.x;
    int index_s = s.y * world_width + s.x;
    int index_w = w.y * world_width + w.x;
    
    if ( new_world->background[index_n].tile == TILE_LVL1_WALL &&
         new_world->background[index_s].tile == TILE_LVL1_WALL )
    {
      new_world->midground[door_index].tile = GlyphTileConverter( tile_index, 0 );
    }
    
    else if ( new_world->background[index_e].tile == TILE_LVL1_WALL &&
              new_world->background[index_w].tile == TILE_LVL1_WALL )
    {
      new_world->midground[door_index].tile = GlyphTileConverter( tile_index, 1 );
    }
    
    new_world->midground[door_index].glyph_index = tile_index-1;
  }

  return new_world;
}

void e_SaveWorld( World_t* world, const char* filename )
{
  if ( world == NULL ) return;
  FILE* file;

  file = fopen( filename, "w+" );
  dString_t* size_string = d_StringInit();
  d_StringFormat( size_string, "// %d %d\n", world->width, world->height );
  fwrite( d_StringPeek( size_string ), sizeof(char), size_string->len, file );

  dString_t* line_string = d_StringInit();
  for ( int j = 0; j < world->height; j++ )
  {
    for ( int i = 0; i < world->width; i++ )
    {
      int index = j * world->width + i;
      char current_char;
      if ( world->room_ids[index] != TILE_EMPTY )
      {
        current_char = world->room_ids[index];
        d_StringAppendChar( line_string, current_char+1 );
        continue;
      }
      
      else if ( world->midground[index].glyph_index != TILE_EMPTY )
      {
        current_char = world->midground[index].glyph_index;
        d_StringAppendChar( line_string, current_char+1 );
        continue;
      }

      else if ( world->background[index].glyph_index != TILE_EMPTY )
      {
        current_char = world->background[index].glyph_index;
        d_StringAppendChar( line_string, current_char+1 );
        continue;
      }
    }
    d_StringAppendChar(line_string, '\n');
    fwrite( d_StringPeek( line_string ), sizeof(char), line_string->len, file );
    d_StringClear( line_string );
  }

  fclose(file);
}

uint16_t GlyphTileConverter( int glyph_index, int rotated )
{
  switch ( glyph_index )
  {
    case TILE_GLYPH_WALL:
      return TILE_LVL1_WALL;
    
    case TILE_GLYPH_FLOOR:
      return TILE_LVL1_FLOOR;
    
    case TILE_GLYPH_RED_DOOR:
      if ( rotated )
      {
        return TILE_RED_DOOR_EW;
      }
      return TILE_RED_DOOR_NS;
    
    case TILE_GLYPH_GREEN_DOOR:
      if ( rotated )
      {
        return TILE_GREEN_DOOR_EW;
      }
      return TILE_GREEN_DOOR_NS;
    
    case TILE_GLYPH_BLUE_DOOR:
      if ( rotated )
      {
        return TILE_BLUE_DOOR_EW;
      }
      return TILE_BLUE_DOOR_NS;
    
    case TILE_GLYPH_WHITE_DOOR:
      if ( rotated )
      {
        return TILE_WHITE_DOOR_EW;
      }
      return TILE_WHITE_DOOR_NS;
  }

  return TILE_EMPTY;
}

uint16_t TileGlyphConverter( int tile_index )
{
  switch ( tile_index )
  {
    case TILE_LVL1_WALL:
      return TILE_GLYPH_WALL-1;

    case TILE_LVL1_FLOOR:
      return TILE_GLYPH_FLOOR-1;
    
    case TILE_RED_DOOR_EW  & TILE_RED_DOOR_NS:
      return TILE_GLYPH_RED_DOOR-1;
    
    case TILE_GREEN_DOOR_EW & TILE_GREEN_DOOR_NS:
      return TILE_GLYPH_GREEN_DOOR-1;
    
    case TILE_BLUE_DOOR_EW & TILE_BLUE_DOOR_NS:
      return TILE_GLYPH_BLUE_DOOR-1;
    
    case TILE_WHITE_DOOR_EW & TILE_WHITE_DOOR_NS:
      return TILE_GLYPH_WHITE_DOOR-1;
  }

  return TILE_EMPTY;
}
