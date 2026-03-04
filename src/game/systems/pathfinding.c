#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "pathfinding.h"

#define GRID_MAX  10000   /* max supported grid cells (100x100) */

/* ---- Binary min-heap on f-score ---- */

typedef struct { int idx; int f; } HeapNode_t;

static HeapNode_t heap[GRID_MAX];
static int        heap_size;

static void heap_swap( HeapNode_t* a, HeapNode_t* b )
{
  HeapNode_t tmp = *a;
  *a = *b;
  *b = tmp;
}

static void heap_push( int idx, int f )
{
  if ( heap_size >= GRID_MAX ) return;
  int i = heap_size++;
  heap[i].idx = idx;
  heap[i].f   = f;
  while ( i > 0 )
  {
    int p = ( i - 1 ) / 2;
    if ( heap[p].f <= heap[i].f ) break;
    heap_swap( &heap[p], &heap[i] );
    i = p;
  }
}

static int heap_pop( int* out_idx )
{
  if ( heap_size == 0 ) return 0;
  *out_idx = heap[0].idx;
  heap[0] = heap[--heap_size];
  int i = 0;
  for ( ;; )
  {
    int l = 2 * i + 1, r = 2 * i + 2, s = i;
    if ( l < heap_size && heap[l].f < heap[s].f ) s = l;
    if ( r < heap_size && heap[r].f < heap[s].f ) s = r;
    if ( s == i ) break;
    heap_swap( &heap[i], &heap[s] );
    i = s;
  }
  return 1;
}

/* ---- A* ---- */

static int     g_score[GRID_MAX];
static int     came_from[GRID_MAX];
static uint8_t closed[GRID_MAX];

int PathfindAStar( int start_r, int start_c, int goal_r, int goal_c,
                   int grid_w, int grid_h,
                   int (*blocked)( int r, int c, void* ctx ), void* ctx,
                   PathNode_t out[PATH_MAX_LEN] )
{
  int total = grid_w * grid_h;
  if ( total <= 0 || total > GRID_MAX ) return 0;

  int si = start_c * grid_w + start_r;
  int gi = goal_c  * grid_w + goal_r;

  /* Trivial case */
  if ( si == gi )
  {
    out[0] = (PathNode_t){ start_r, start_c };
    return 1;
  }

  /* Init arrays (only the portion we use) */
  memset( g_score,   0x7f, total * sizeof( int ) );
  memset( came_from,   -1, total * sizeof( int ) );
  memset( closed,       0, total * sizeof( uint8_t ) );
  heap_size = 0;

  g_score[si] = 0;
  heap_push( si, abs( goal_r - start_r ) + abs( goal_c - start_c ) );

  static const int dr[] = { 1, -1, 0, 0 };
  static const int dc[] = { 0, 0, 1, -1 };

  while ( heap_size > 0 )
  {
    int ci;
    heap_pop( &ci );
    if ( ci == gi ) break;
    if ( closed[ci] ) continue;
    closed[ci] = 1;

    int cr = ci % grid_w;
    int cc = ci / grid_w;
    int cg = g_score[ci];

    for ( int d = 0; d < 4; d++ )
    {
      int nr = cr + dr[d];
      int nc = cc + dc[d];
      if ( nr < 0 || nr >= grid_w || nc < 0 || nc >= grid_h ) continue;

      int ni = nc * grid_w + nr;
      if ( closed[ni] ) continue;

      /* Goal tile exempt from blocker - caller handles it */
      if ( ni != gi && blocked( nr, nc, ctx ) ) continue;

      int ng = cg + 1;
      if ( ng < g_score[ni] )
      {
        g_score[ni]   = ng;
        came_from[ni] = ci;
        heap_push( ni, ng + abs( goal_r - nr ) + abs( goal_c - nc ) );
      }
    }
  }

  /* No path found */
  if ( came_from[gi] < 0 ) return 0;

  /* Reconstruct path backwards into temp buffer */
  PathNode_t rev[PATH_MAX_LEN];
  int len = 0;
  int idx = gi;
  while ( idx >= 0 && len < PATH_MAX_LEN )
  {
    rev[len].row = idx % grid_w;
    rev[len].col = idx / grid_w;
    len++;
    if ( idx == si ) break;
    idx = came_from[idx];
  }

  /* Sanity: path must end at start */
  if ( len == 0 || rev[len - 1].row != start_r || rev[len - 1].col != start_c )
    return 0;

  /* Reverse into out[] */
  for ( int i = 0; i < len; i++ )
    out[i] = rev[len - 1 - i];

  return len;
}
