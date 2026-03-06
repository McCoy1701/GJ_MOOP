#ifndef __INTERACTIVE_TILE_H__
#define __INTERACTIVE_TILE_H__

#include "world.h"

#define MAX_ITILES          32
#define ITILE_RAT_HOLE       0
#define ITILE_HIDDEN_WALL    1
#define ITILE_SPIDER_WEB     2
#define ITILE_OLD_CRATE      3
#define ITILE_URN            4

#define WEB_ROOT_TURNS       2
#define WEB_COOLDOWN         6

typedef struct {
  int row, col, type, active;
  int revealed;
  int cooldown;
  int gold;          /* hidden gold in web (0 = none, consumed on pickup) */
} ITile_t;

void         ITileInit( void );
void         ITilePlace( World_t* world, int x, int y, int type );
ITile_t*     ITileAt( int row, int col );
void         ITileBreak( World_t* world, int row, int col );
const char*  ITileDescription( int type );

/* Hidden walls */
void         ITileReveal( World_t* world, int row, int col );
int          ITileIsRevealedHiddenWall( int row, int col );
int          ITileIsHiddenWall( int row, int col );
void         ITileOpenHiddenWall( World_t* world, int row, int col );

/* Spider webs */
int          ITileWebCheck( World_t* world, int row, int col, int* out_gold );
void         ITileTick( void );
void         ITileWebScatterGold( void );

/* Old crates */
int          ITileCrateCheck( World_t* world, int row, int col, int* out_gold );
void         ITileCrateScatterGold( void );

/* Urns */
int          ITileUrnCheck( World_t* world, int row, int col, int* out_gold );
void         ITileUrnScatterGold( void );

#endif
