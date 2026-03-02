#ifndef __INTERACTIVE_TILE_H__
#define __INTERACTIVE_TILE_H__

#include "world.h"

#define MAX_ITILES          16
#define ITILE_RAT_HOLE       0
#define ITILE_HIDDEN_WALL    1

typedef struct {
  int row, col, type, active;
  int revealed;
} ITile_t;

void         ITileInit( void );
void         ITilePlace( World_t* world, int x, int y, int type );
ITile_t*     ITileAt( int row, int col );
void         ITileBreak( World_t* world, int row, int col );
const char*  ITileDescription( int type );

/* Hidden walls */
void         ITileReveal( World_t* world, int row, int col );
int          ITileIsRevealedHiddenWall( int row, int col );
void         ITileOpenHiddenWall( World_t* world, int row, int col );

#endif
