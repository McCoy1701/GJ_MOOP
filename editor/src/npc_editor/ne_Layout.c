/*
 * npc_editor/ne_Layout.c:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "npc_editor.h"

#define LAYER_SPACING_X  310.0f
#define LAYER_SPACING_Y  100.0f

static const char* sp( dString_t* s )
{
  if ( !s ) return "";
  return d_StringPeek( s );
}

static int slen( dString_t* s )
{
  if ( !s ) return 0;
  return d_StringGetLength( s );
}

static int find_node( const char* key )
{
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
    if ( strcmp( sp( g_ne_graph.nodes[i].key ), key ) == 0 )
      return i;
  return -1;
}

void ne_AutoLayout( void )
{
  if ( g_ne_graph.num_nodes == 0 ) return;

  int visited[NE_MAX_NODES];
  int layer[NE_MAX_NODES];
  memset( visited, 0, sizeof( visited ) );
  memset( layer, -1, sizeof( int ) * g_ne_graph.num_nodes );

  /* BFS queue */
  int queue[NE_MAX_NODES];
  int qh = 0, qt = 0;

  /* Seed: start nodes at layer 0 */
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
  {
    if ( g_ne_graph.nodes[i].is_start )
    {
      layer[i]   = 0;
      visited[i] = 1;
      queue[qt++] = i;
    }
  }

  /* Fallback: if no start nodes, seed node 0 */
  if ( qt == 0 )
  {
    layer[0]   = 0;
    visited[0] = 1;
    queue[qt++] = 0;
  }

  /* BFS: follow option_keys and goto_key edges */
  while ( qh < qt )
  {
    int ci = queue[qh++];
    NENode_t* cn = &g_ne_graph.nodes[ci];

    /* Speech -> Options */
    for ( int o = 0; o < cn->num_options; o++ )
    {
      int di = find_node( sp( cn->option_keys[o] ) );
      if ( di >= 0 && !visited[di] )
      {
        layer[di]   = layer[ci] + 1;
        visited[di] = 1;
        queue[qt++] = di;
      }
    }

    /* Option -> Goto */
    if ( slen( cn->goto_key ) )
    {
      int di = find_node( sp( cn->goto_key ) );
      if ( di >= 0 && !visited[di] )
      {
        layer[di]   = layer[ci] + 1;
        visited[di] = 1;
        queue[qt++] = di;
      }
    }
  }

  /* Unreachable nodes get their own layer */
  int max_layer = 0;
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
    if ( layer[i] > max_layer ) max_layer = layer[i];
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
    if ( layer[i] < 0 ) layer[i] = max_layer + 1;

  /* Assign positions: each layer is a column */
  int count[NE_MAX_NODES];
  memset( count, 0, sizeof( count ) );

  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
  {
    int L = layer[i];
    g_ne_graph.nodes[i].gx = (float)L * LAYER_SPACING_X;
    g_ne_graph.nodes[i].gy = (float)count[L] * LAYER_SPACING_Y;
    count[L]++;
  }

  printf( "NPC Editor: auto-layout %d nodes, %d layers\n",
          g_ne_graph.num_nodes, max_layer + 1 );
}
