#include <stdlib.h>
#include <string.h>

#include "visibility.h"

#define VIS_RADIUS 8

static World_t* world;
static float*   vis;

void VisibilityInit( World_t* w )
{
  world = w;
  vis   = calloc( w->tile_count, sizeof( float ) );
}

/* Bresenham line-of-sight check.
   Returns 1 if (x1,y1) is visible from (x0,y0) - i.e. no solid tile
   blocks the path.  The destination tile itself is allowed to be solid
   (so you can "see" a wall or closed door). */
int los_clear( int x0, int y0, int x1, int y1 )
{
  int dx = abs( x1 - x0 );
  int dy = abs( y1 - y0 );
  int sx = ( x0 < x1 ) ? 1 : -1;
  int sy = ( y0 < y1 ) ? 1 : -1;
  int err = dx - dy;

  int cx = x0, cy = y0;

  while ( cx != x1 || cy != y1 )
  {
    int e2 = 2 * err;
    if ( e2 > -dy ) { err -= dy; cx += sx; }
    if ( e2 <  dx ) { err += dx; cy += sy; }

    /* Reached destination - don't block on the target itself */
    if ( cx == x1 && cy == y1 )
      break;

    /* Intermediate tile - check if it blocks sight */
    if ( cx < 0 || cx >= world->width || cy < 0 || cy >= world->height )
      return 0;

    int idx = cy * world->width + cx;
    if ( world->background[idx].solid )  return 0;
    if ( world->midground[idx].solid )   return 0;
  }

  return 1;
}

void VisibilityUpdate( int pr, int pc )
{
  int w = world->width;
  int h = world->height;

  /* Reset all tiles to dark */
  memset( vis, 0, world->tile_count * sizeof( float ) );

  /* Pass 1 - light non-solid tiles (floor) via line-of-sight */
  for ( int y = pc - VIS_RADIUS; y <= pc + VIS_RADIUS; y++ )
  {
    for ( int x = pr - VIS_RADIUS; x <= pr + VIS_RADIUS; x++ )
    {
      if ( x < 0 || x >= w || y < 0 || y >= h )
        continue;

      int dx = abs( x - pr );
      int dy = abs( y - pc );
      int dist = ( dx > dy ) ? dx : dy;

      if ( dist > VIS_RADIUS )
        continue;

      int idx = y * w + x;
      int solid = world->background[idx].solid || world->midground[idx].solid;

      if ( solid )
        continue;  /* walls/doors handled in pass 2 */

      if ( dist <= 1 || los_clear( pr, pc, x, y ) )
        vis[idx] = 1.0f - ( (float)dist / ( VIS_RADIUS + 1.0f ) );
    }
  }

  /* Pass 2 - solid tiles (walls, doors) inherit from brightest
     visible neighbor.  If you can see the floor next to a wall,
     you can see that wall. */
  static const int ox[] = { -1, 1, 0, 0, -1, -1, 1, 1 };
  static const int oy[] = { 0, 0, -1, 1, -1, 1, -1, 1 };

  for ( int y = pc - VIS_RADIUS; y <= pc + VIS_RADIUS; y++ )
  {
    for ( int x = pr - VIS_RADIUS; x <= pr + VIS_RADIUS; x++ )
    {
      if ( x < 0 || x >= w || y < 0 || y >= h )
        continue;

      int idx = y * w + x;
      if ( vis[idx] > 0 )  continue;  /* already lit */

      int solid = world->background[idx].solid || world->midground[idx].solid;
      if ( !solid )  continue;

      float best = 0;
      for ( int d = 0; d < 8; d++ )
      {
        int nx = x + ox[d];
        int ny = y + oy[d];
        if ( nx < 0 || nx >= w || ny < 0 || ny >= h )
          continue;
        int nidx = ny * w + nx;
        /* Only inherit from non-solid (floor) tiles - prevents cascade */
        if ( world->background[nidx].solid || world->midground[nidx].solid )
          continue;
        if ( vis[nidx] > best ) best = vis[nidx];
      }

      if ( best > 0 )
        vis[idx] = best;
    }
  }
}

float VisibilityGet( int r, int c )
{
  if ( r < 0 || r >= world->width || c < 0 || c >= world->height )
    return 0.0f;
  return vis[c * world->width + r];
}
