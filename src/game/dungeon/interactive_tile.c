#include <string.h>
#include <Archimedes.h>

#include "interactive_tile.h"

static ITile_t itiles[MAX_ITILES];
static int     num_itiles = 0;

static const struct {
  int         tile_id;
  const char* glyph;
  aColor_t    color;
  const char* description;
  int         solid;
} itile_types[] = {
  [ITILE_RAT_HOLE] = {
    10, "O", { 0x09, 0x0a, 0x14, 255 },
    "A dark hole gnawed through the wall. Something is scratching inside.", 1
  },
  [ITILE_HIDDEN_WALL] = {
    1, "#", { 0x81, 0x97, 0x96, 255 },
    "You see a stone wall.", 1
  },
};

void ITileInit( void )
{
  memset( itiles, 0, sizeof( itiles ) );
  num_itiles = 0;
}

void ITilePlace( World_t* world, int x, int y, int type )
{
  if ( num_itiles >= MAX_ITILES ) return;

  int idx = y * world->width + x;

  world->midground[idx].tile     = itile_types[type].tile_id;
  world->midground[idx].glyph    = (char*)itile_types[type].glyph;
  world->midground[idx].glyph_fg = itile_types[type].color;
  world->midground[idx].solid    = itile_types[type].solid;

  itiles[num_itiles].row    = x;
  itiles[num_itiles].col    = y;
  itiles[num_itiles].type   = type;
  itiles[num_itiles].active = 1;
  num_itiles++;
}

ITile_t* ITileAt( int row, int col )
{
  for ( int i = 0; i < num_itiles; i++ )
  {
    if ( itiles[i].active && itiles[i].row == row && itiles[i].col == col )
      return &itiles[i];
  }
  return NULL;
}

void ITileBreak( World_t* world, int row, int col )
{
  ITile_t* t = ITileAt( row, col );
  if ( !t ) return;

  int idx = t->col * world->width + t->row;

  world->midground[idx].tile     = 0;
  world->midground[idx].glyph    = ".";
  world->midground[idx].glyph_fg = (aColor_t){ 0x39, 0x4a, 0x50, 255 };
  world->midground[idx].solid    = 0;

  t->active = 0;
}

const char* ITileDescription( int type )
{
  return itile_types[type].description;
}

void ITileReveal( World_t* world, int row, int col )
{
  ITile_t* t = ITileAt( row, col );
  if ( !t || t->type != ITILE_HIDDEN_WALL || t->revealed ) return;

  t->revealed = 1;

  int idx = t->col * world->width + t->row;
  world->midground[idx].glyph    = "#";
  world->midground[idx].glyph_fg = (aColor_t){ 0xc0, 0x94, 0x73, 255 };
}

int ITileIsRevealedHiddenWall( int row, int col )
{
  ITile_t* t = ITileAt( row, col );
  return ( t && t->type == ITILE_HIDDEN_WALL && t->revealed );
}

void ITileOpenHiddenWall( World_t* world, int row, int col )
{
  ITile_t* t = ITileAt( row, col );
  if ( !t || t->type != ITILE_HIDDEN_WALL ) return;

  int idx = t->col * world->width + t->row;

  world->midground[idx].tile     = TILE_EMPTY;
  world->midground[idx].glyph    = "";
  world->midground[idx].glyph_fg = (aColor_t){ 0, 0, 0, 0 };
  world->midground[idx].solid    = 0;

  t->active = 0;
}
