#include <Archimedes.h>

#include "objects.h"

#define MAX_OBJECTS 32

typedef struct {
  int x, y, type;
} ObjectEntry_t;

static Console_t* console;
static ObjectEntry_t objects[MAX_OBJECTS];
static int num_objects = 0;

static aColor_t object_color( int type )
{
  switch ( type )
  {
    case OBJ_EASEL: return (aColor_t){ 0xc0, 0x94, 0x73, 255 };
    case OBJ_CHAIR: return (aColor_t){ 0x8b, 0x6d, 0x4a, 255 };
    default:        return (aColor_t){ 0x81, 0x97, 0x96, 255 };
  }
}

static char* object_glyph( int type )
{
  switch ( type )
  {
    case OBJ_EASEL: return "E";
    case OBJ_CHAIR: return "h";
    default:        return "?";
  }
}

static ObjectEntry_t* object_at( int x, int y )
{
  for ( int i = 0; i < num_objects; i++ )
    if ( objects[i].x == x && objects[i].y == y )
      return &objects[i];
  return NULL;
}

void ObjectsInit( Console_t* con )
{
  console = con;
  num_objects = 0;
}

void ObjectPlace( World_t* w, int x, int y, int type )
{
  if ( num_objects >= MAX_OBJECTS ) return;
  objects[num_objects++] = (ObjectEntry_t){ x, y, type };

  int idx = y * w->width + x;
  w->background[idx].tile     = 0;  /* floor tile - object sits on floor */
  w->background[idx].glyph    = object_glyph( type );
  w->background[idx].glyph_fg = object_color( type );
  w->background[idx].solid    = 1;
}

int ObjectIsObject( int x, int y )
{
  return object_at( x, y ) != NULL;
}

void ObjectDescribe( int x, int y )
{
  ObjectEntry_t* obj = object_at( x, y );
  if ( !obj ) return;

  aColor_t color = object_color( obj->type );
  switch ( obj->type )
  {
    case OBJ_EASEL:
      ConsolePush( console, "An easel with artwork.", color );
      break;
    case OBJ_CHAIR:
      ConsolePush( console, "A sturdy chair. Someone has been sitting here a long time.", color );
      break;
    default:
      ConsolePush( console, "Something unusual.", color );
      break;
  }
}
