#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "dungeon.h"
#include "doors.h"
#include "objects.h"
#include "room_enumerator.h"
#include "interactive_tile.h"

int DungeonCharToRoomId( char c )
{
  if ( c >= '0' && c <= '9' ) return c - '0';
  switch ( c )
  {
    case '!': return 10;  case '@': return 11;  case '~': return 12;
    case '$': return 13;  case '%': return 14;  case '^': return 15;
    case '&': return 16;  case '*': return 17;  case '(': return 18;
    case ')': return 19;  case '[': return 20;  case ']': return 21;
    case '{': return 22;  case '}': return 23;  case '-': return 24;
    case '_': return 25;  case '+': return 26;  case '=': return 27;
    default:  return ROOM_NONE;
  }
}

char DungeonRoomIdToChar( int id )
{
  if ( id >= 0 && id <= 9 ) return '0' + id;
  static const char lut[] = "!@~$%^&*()[]{}-_+=";
  if ( id >= 10 && id <= 27 ) return lut[id - 10];
  return '.';
}

/* Load a .map file into dString rows. Caller must free with dungeon_free_map(). */
static dString_t** dungeon_load_map( const char* path )
{
  FILE* fp = fopen( path, "r" );
  if ( !fp )
  {
    fprintf( stderr, "dungeon_load_map: failed to open '%s'\n", path );
    return NULL;
  }

  dString_t** rows = malloc( sizeof( dString_t* ) * DUNGEON_H );
  if ( !rows ) { fclose( fp ); return NULL; }

  char buf[256];
  int y = 0;

  while ( y < DUNGEON_H && fgets( buf, sizeof( buf ), fp ) )
  {
    /* Strip trailing newline */
    size_t len = strlen( buf );
    while ( len > 0 && ( buf[len-1] == '\n' || buf[len-1] == '\r' ) )
      buf[--len] = '\0';

    /* Skip comment lines */
    if ( len >= 2 && buf[0] == '/' && buf[1] == '/' )
      continue;

    rows[y] = d_StringInit();
    d_StringAppend( rows[y], buf, 0 );

    /* Pad short rows with walls */
    if ( (int)len < DUNGEON_W )
    {
      fprintf( stderr, "dungeon_load_map: row %d too short (%zu < %d)\n",
               y, len, DUNGEON_W );
      char pad[DUNGEON_W + 1];
      int needed = DUNGEON_W - (int)len;
      memset( pad, '#', needed );
      pad[needed] = '\0';
      d_StringAppend( rows[y], pad, 0 );
    }

    y++;
  }

  fclose( fp );

  /* Fill any missing rows with walls */
  while ( y < DUNGEON_H )
  {
    rows[y] = d_StringInit();
    char wall_row[DUNGEON_W + 1];
    memset( wall_row, '#', DUNGEON_W );
    wall_row[DUNGEON_W] = '\0';
    d_StringAppend( rows[y], wall_row, 0 );
    y++;
  }

  return rows;
}

static void dungeon_free_map( dString_t** rows )
{
  if ( !rows ) return;
  for ( int y = 0; y < DUNGEON_H; y++ )
    d_StringDestroy( rows[y] );
  free( rows );
}

int DungeonSaveMap( const char* path, const char** rows, int width, int height )
{
  FILE* fp = fopen( path, "w" );
  if ( !fp )
  {
    fprintf( stderr, "DungeonSaveMap: failed to open '%s'\n", path );
    return 0;
  }

  fprintf( fp, "// %d %d\n", width, height );

  for ( int y = 0; y < height; y++ )
    fprintf( fp, "%s\n", rows[y] );

  fclose( fp );
  return 1;
}

int g_current_floor = 1;

void DungeonBuild( World_t* world )
{
  const char* map_path = ( g_current_floor == 2 )
    ? "resources/data/floors/floor_02.map"
    : "resources/data/floors/floor_01.map";
  dString_t** floor = dungeon_load_map( map_path );
  if ( !floor )
  {
    fprintf( stderr, "DungeonBuild: could not load floor map!\n" );
    return;
  }

  RoomEnumeratorInit( DUNGEON_W, DUNGEON_H );
  ITileInit();

  /* Parse the character map into tiles */
  for ( int y = 0; y < DUNGEON_H; y++ )
  {
    const char* row = d_StringPeek( floor[y] );
    for ( int x = 0; x < DUNGEON_W; x++ )
    {
      int  idx = y * DUNGEON_W + x;
      char c   = row[x];

      if ( c == '#' )
      {
        world->background[idx].tile     = 1;
        world->background[idx].glyph    = "#";
        world->background[idx].glyph_fg = (aColor_t){ 0x81, 0x97, 0x96, 255 };
        world->background[idx].solid    = 1;
      }
      else if ( c == 'H' )
      {
        ITilePlace( world, x, y, ITILE_RAT_HOLE );
      }
      else if ( c == 'S' )
      {
        ITilePlace( world, x, y, ITILE_HIDDEN_WALL );
      }
      else if ( c == 'B' || c == 'G' || c == 'R' || c == 'W' )
      {
        int type = ( c == 'B' ) ? DOOR_BLUE  :
                   ( c == 'G' ) ? DOOR_GREEN :
                   ( c == 'R' ) ? DOOR_RED   : DOOR_WHITE;
        /* Walls above & below = vertical door; walls left & right = horizontal */
        const char* above = ( y > 0 )              ? d_StringPeek( floor[y-1] ) : NULL;
        const char* below = ( y < DUNGEON_H - 1 )  ? d_StringPeek( floor[y+1] ) : NULL;
        int vert = ( above && below && above[x] == '#' && below[x] == '#' );
        DoorPlace( world, x, y, type, vert );
      }
      else
      {
        int room_id = DungeonCharToRoomId( c );
        if ( room_id != ROOM_NONE )
          RoomSetTile( x, y, room_id );
        /* both room chars and '.' keep the WorldCreate default (floor, tile 0) */
      }
    }
  }

  dungeon_free_map( floor );

  RoomLoadData( ( g_current_floor == 2 )
    ? "resources/data/rooms_floor_02.duf"
    : "resources/data/rooms_floor_01.duf" );

  /* Objects - placed by coordinate, not by map char */
  if ( g_current_floor == 1 )
    ObjectPlace( world, 22, 4, OBJ_EASEL );
  if ( g_current_floor == 2 )
    ObjectPlace( world, 12, 5, OBJ_CHAIR );
}

void DungeonPlayerStart( float* wx, float* wy )
{
  /* Find center of room 0 */
  int min_x = DUNGEON_W, max_x = 0;
  int min_y = DUNGEON_H, max_y = 0;
  int found = 0;

  for ( int y = 0; y < DUNGEON_H; y++ )
    for ( int x = 0; x < DUNGEON_W; x++ )
      if ( RoomAt( x, y ) == 0 )
      {
        if ( x < min_x ) min_x = x;
        if ( x > max_x ) max_x = x;
        if ( y < min_y ) min_y = y;
        if ( y > max_y ) max_y = y;
        found = 1;
      }

  int cx = found ? ( min_x + max_x ) / 2 : 14;
  int cy = found ? ( min_y + max_y ) / 2 : 18;

  *wx = cx * 16 + 8.0f;
  *wy = cy * 16 + 8.0f;
}
