/*
 * npc_editor/ne_Graph.c:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "npc_editor.h"

/* ---- Node dimensions (world space) ---- */

#define NODE_W       240.0f
#define NODE_H        72.0f
#define NODE_PAD       6.0f
#define ZOOM_MIN       0.25f
#define ZOOM_MAX       3.0f
#define ZOOM_STEP      0.15f

/* ---- Camera state ---- */

static float cam_x     = 0.0f;
static float cam_y     = 0.0f;
static float cam_zoom  = 1.0f;

/* ---- Interaction state ---- */

static int   dragging_node = -1;
static float drag_ox       = 0.0f;
static float drag_oy       = 0.0f;
static int   panning       = 0;
static int   pan_start_mx  = 0;
static int   pan_start_my  = 0;
static float pan_start_cx  = 0.0f;
static float pan_start_cy  = 0.0f;
static int   left_held     = 0;
static int   right_held    = 0;
static int   click_node    = -1;  /* node under cursor at press time */
static int   click_mx      = 0;   /* mouse pos at press time */
static int   click_my      = 0;
static int   rclick_mx     = 0;   /* right-click press pos */
static int   rclick_my     = 0;
static int   rclick_btn    = 0;   /* which button started the pan (2 or 3) */
#define DRAG_THRESHOLD 4

/* Context menu: set when right-click without drag */
static int   ctx_menu_open = 0;
static float ctx_menu_wx   = 0.0f;  /* world-space position to create node */
static float ctx_menu_wy   = 0.0f;
static int   ctx_menu_sx   = 0;     /* screen-space for drawing */
static int   ctx_menu_sy   = 0;
static int   ctx_menu_node = -1;    /* node index if right-clicked on a node, -1 if empty */

/* ---- Helpers ---- */

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

/* ---- Coordinate transforms ---- */

static float w2sx( float wx, aRectf_t p )
{
  return p.x + p.w * 0.5f + ( wx - cam_x ) * cam_zoom;
}

static float w2sy( float wy, aRectf_t p )
{
  return p.y + p.h * 0.5f + ( wy - cam_y ) * cam_zoom;
}

static float s2wx( float sx, aRectf_t p )
{
  return cam_x + ( sx - p.x - p.w * 0.5f ) / cam_zoom;
}

static float s2wy( float sy, aRectf_t p )
{
  return cam_y + ( sy - p.y - p.h * 0.5f ) / cam_zoom;
}

/* ---- Find node by key ---- */

static int find_node( const char* key )
{
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
    if ( strcmp( sp( g_ne_graph.nodes[i].key ), key ) == 0 )
      return i;
  return -1;
}

/* ---- Camera accessors ---- */

float ne_GraphCamX( void ) { return cam_x; }
float ne_GraphCamY( void ) { return cam_y; }

/* ---- Context menu accessors ---- */

int ne_GraphCtxMenuOpen( void ) { return ctx_menu_open; }
void ne_GraphCtxMenuClose( void ) { ctx_menu_open = 0; }
float ne_GraphCtxMenuWX( void ) { return ctx_menu_wx; }
float ne_GraphCtxMenuWY( void ) { return ctx_menu_wy; }
int ne_GraphCtxMenuSX( void ) { return ctx_menu_sx; }
int ne_GraphCtxMenuSY( void ) { return ctx_menu_sy; }
int ne_GraphCtxMenuNode( void ) { return ctx_menu_node; }

/* ---- Clear interaction state (e.g. when modal opens) ---- */

void ne_GraphClearInput( void )
{
  dragging_node = -1;
  click_node    = -1;
  left_held     = 0;
  right_held    = 0;
  panning       = 0;
  ctx_menu_open = 0;
}

/* ---- Reset camera to center on graph ---- */

void ne_GraphReset( void )
{
  cam_zoom = 1.0f;
  dragging_node = -1;
  panning       = 0;
  left_held     = 0;
  right_held    = 0;

  if ( g_ne_graph.num_nodes == 0 )
  {
    cam_x = 0; cam_y = 0;
    return;
  }

  float min_x = g_ne_graph.nodes[0].gx;
  float max_x = min_x + NODE_W;
  float min_y = g_ne_graph.nodes[0].gy;
  float max_y = min_y + NODE_H;

  for ( int i = 1; i < g_ne_graph.num_nodes; i++ )
  {
    NENode_t* n = &g_ne_graph.nodes[i];
    if ( n->gx < min_x ) min_x = n->gx;
    if ( n->gx + NODE_W > max_x ) max_x = n->gx + NODE_W;
    if ( n->gy < min_y ) min_y = n->gy;
    if ( n->gy + NODE_H > max_y ) max_y = n->gy + NODE_H;
  }

  cam_x = ( min_x + max_x ) * 0.5f;
  cam_y = ( min_y + max_y ) * 0.5f;
}

/* ---- Logic ---- */

void ne_GraphLogic( aRectf_t panel )
{
  float mx  = (float)app.mouse.x;
  float my  = (float)app.mouse.y;
  float wmx = s2wx( mx, panel );
  float wmy = s2wy( my, panel );

  /* Zoom (scroll wheel) */
  if ( app.mouse.wheel != 0 )
  {
    float old_zoom = cam_zoom;
    cam_zoom += ( app.mouse.wheel > 0 ) ? ZOOM_STEP : -ZOOM_STEP;
    if ( cam_zoom < ZOOM_MIN ) cam_zoom = ZOOM_MIN;
    if ( cam_zoom > ZOOM_MAX ) cam_zoom = ZOOM_MAX;

    /* Keep world point under cursor fixed */
    float dx = ( wmx - cam_x ) * old_zoom;
    float dy = ( wmy - cam_y ) * old_zoom;
    cam_x = wmx - dx / cam_zoom;
    cam_y = wmy - dy / cam_zoom;

    app.mouse.wheel = 0;
  }

  /* Left click closes context menu */
  if ( ctx_menu_open && app.mouse.button == 1 && app.mouse.state == 1 )
  {
    ctx_menu_open = 0;
    /* Don't consume — let normal left-click logic handle it */
  }

  /* Right or middle button press: start pan (context menu on release if no drag) */
  if ( ( app.mouse.button == 3 || app.mouse.button == 2 )
       && app.mouse.state == 1 )
  {
    ctx_menu_open = 0;  /* close any open context menu */
    rclick_btn   = app.mouse.button;
    right_held   = 1;
    panning      = 1;
    pan_start_mx = app.mouse.x;
    pan_start_my = app.mouse.y;
    rclick_mx    = app.mouse.x;
    rclick_my    = app.mouse.y;
    pan_start_cx = cam_x;
    pan_start_cy = cam_y;
    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* Pan drag */
  if ( panning && right_held )
  {
    cam_x = pan_start_cx - (float)( app.mouse.x - pan_start_mx ) / cam_zoom;
    cam_y = pan_start_cy - (float)( app.mouse.y - pan_start_my ) / cam_zoom;
  }

  /* Right or middle button release: stop pan, maybe open context menu */
  if ( ( app.mouse.button == 3 || app.mouse.button == 2 )
       && app.mouse.state == 0 )
  {
    /* If didn't drag, open context menu (right-click only, not middle) */
    if ( rclick_btn == 3 )
    {
      int rdx = app.mouse.x - rclick_mx;
      int rdy = app.mouse.y - rclick_my;
      if ( rdx * rdx + rdy * rdy < DRAG_THRESHOLD * DRAG_THRESHOLD )
      {
        float rwx = s2wx( (float)app.mouse.x, panel );
        float rwy = s2wy( (float)app.mouse.y, panel );

        ctx_menu_open = 1;
        ctx_menu_wx   = rwx;
        ctx_menu_wy   = rwy;
        ctx_menu_sx   = app.mouse.x;
        ctx_menu_sy   = app.mouse.y;

        /* Check if right-clicked on a node */
        ctx_menu_node = -1;
        for ( int i = g_ne_graph.num_nodes - 1; i >= 0; i-- )
        {
          NENode_t* n = &g_ne_graph.nodes[i];
          if ( rwx >= n->gx && rwx <= n->gx + NODE_W
               && rwy >= n->gy && rwy <= n->gy + NODE_H )
          {
            ctx_menu_node = i;
            break;
          }
        }
      }
    }
    right_held = 0;
    panning    = 0;
  }

  /* Left-button press: hit-test nodes */
  if ( app.mouse.button == 1 && app.mouse.state == 1 && !panning )
  {
    left_held     = 1;
    dragging_node = -1;
    click_node    = -1;
    click_mx      = app.mouse.x;
    click_my      = app.mouse.y;

    for ( int i = g_ne_graph.num_nodes - 1; i >= 0; i-- )
    {
      NENode_t* n = &g_ne_graph.nodes[i];
      if ( wmx >= n->gx && wmx <= n->gx + NODE_W
           && wmy >= n->gy && wmy <= n->gy + NODE_H )
      {
        dragging_node = i;
        click_node    = i;
        drag_ox       = wmx - n->gx;
        drag_oy       = wmy - n->gy;
        break;
      }
    }

    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* Left-drag: move node */
  if ( left_held && dragging_node >= 0 )
  {
    NENode_t* n = &g_ne_graph.nodes[dragging_node];
    float new_wmx = s2wx( (float)app.mouse.x, panel );
    float new_wmy = s2wy( (float)app.mouse.y, panel );
    n->gx = new_wmx - drag_ox;
    n->gy = new_wmy - drag_oy;
  }

  /* Left-button release */
  if ( app.mouse.button == 1 && app.mouse.state == 0 )
  {
    /* If click didn't move far, treat as select (opens modal) */
    int dx = app.mouse.x - click_mx;
    int dy = app.mouse.y - click_my;
    if ( dx * dx + dy * dy < DRAG_THRESHOLD * DRAG_THRESHOLD )
      g_ne_selected_node = click_node;

    left_held     = 0;
    dragging_node = -1;
    click_node    = -1;
  }
}

/* ---- Draw connections ---- */

static void ne_DrawConnections( aRectf_t p )
{
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
  {
    NENode_t* src = &g_ne_graph.nodes[i];

    /* Speech -> Options (blue lines) */
    for ( int o = 0; o < src->num_options; o++ )
    {
      int di = find_node( sp( src->option_keys[o] ) );
      if ( di < 0 ) continue;
      NENode_t* dst = &g_ne_graph.nodes[di];

      int x1 = (int)w2sx( src->gx + NODE_W, p );
      int y1 = (int)w2sy( src->gy + NODE_H * 0.5f, p );
      int x2 = (int)w2sx( dst->gx, p );
      int y2 = (int)w2sy( dst->gy + NODE_H * 0.5f, p );

      a_DrawLine( x1, y1, x2, y2, (aColor_t){ 80, 120, 180, 200 } );
    }

    /* Option -> Goto (yellow lines) */
    if ( slen( src->goto_key ) )
    {
      int di = find_node( sp( src->goto_key ) );
      if ( di < 0 ) continue;
      NENode_t* dst = &g_ne_graph.nodes[di];

      int x1 = (int)w2sx( src->gx + NODE_W, p );
      int y1 = (int)w2sy( src->gy + NODE_H * 0.5f, p );
      int x2 = (int)w2sx( dst->gx, p );
      int y2 = (int)w2sy( dst->gy + NODE_H * 0.5f, p );

      a_DrawLine( x1, y1, x2, y2, (aColor_t){ 180, 160, 60, 200 } );
    }
  }
}

/* ---- Node colors ---- */

static aColor_t ne_NodeColor( NENode_t* node )
{
  if ( node->is_start )
    return (aColor_t){ 35, 100, 35, 220 };       /* green: start */
  if ( slen( node->label ) )
    return (aColor_t){ 120, 110, 35, 220 };       /* gold: option */
  if ( slen( node->text ) && node->num_options > 0 )
    return (aColor_t){ 35, 60, 110, 220 };        /* blue: speech with options */
  if ( slen( node->text ) )
    return (aColor_t){ 60, 50, 90, 220 };         /* purple: speech, no options yet */
  return (aColor_t){ 50, 50, 55, 220 };           /* gray: empty/unconfigured */
}

/* ---- Draw nodes ---- */

static void ne_DrawNodes( aRectf_t p )
{
  aTextStyle_t ts = {
    .type       = FONT_CODE_PAGE_437,
    .fg         = white,
    .bg         = black,
    .align      = TEXT_ALIGN_LEFT,
    .wrap_width = 0,
    .scale      = 1.0f,
    .padding    = 0
  };

  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
  {
    NENode_t* node = &g_ne_graph.nodes[i];

    float sx = w2sx( node->gx, p );
    float sy = w2sy( node->gy, p );
    float sw = NODE_W * cam_zoom;
    float sh = NODE_H * cam_zoom;

    /* Frustum cull */
    if ( sx + sw < p.x || sx > p.x + p.w
         || sy + sh < p.y || sy > p.y + p.h )
      continue;

    aRectf_t box = { sx, sy, sw, sh };

    /* Fill */
    a_DrawFilledRect( box, ne_NodeColor( node ) );

    /* Border: white if selected, lighter version of fill otherwise */
    aColor_t border;
    if ( i == g_ne_selected_node )
      border = white;
    else
    {
      aColor_t c = ne_NodeColor( node );
      border = (aColor_t){
        (uint8_t)( c.r + 60 > 255 ? 255 : c.r + 60 ),
        (uint8_t)( c.g + 60 > 255 ? 255 : c.g + 60 ),
        (uint8_t)( c.b + 60 > 255 ? 255 : c.b + 60 ),
        255
      };
    }
    a_DrawRect( box, border );

    /* Skip text when zoomed out too far */
    if ( cam_zoom < 0.4f ) continue;

    int tx = (int)( sx + NODE_PAD * cam_zoom );
    int ty = (int)( sy + NODE_PAD * cam_zoom );
    int text_w = (int)( ( NODE_W - NODE_PAD * 2 ) * cam_zoom );

    /* Key name (bold white) */
    ts.fg = white;
    ts.scale = cam_zoom;
    ts.wrap_width = 0;
    a_DrawText( sp( node->key ), tx, ty, ts );

    /* Content text: label or dialogue text, truncated to fit node */
    const char* line2 = slen( node->label ) ? sp( node->label )
                        : slen( node->text ) ? sp( node->text )
                        : NULL;
    if ( line2 )
    {
      ts.fg = (aColor_t){ 190, 190, 190, 255 };

      /* Calculate how many chars fit in the node body */
      int glyph_w = (int)( 9 * cam_zoom );
      int chars_per_line = ( glyph_w > 0 ) ? text_w / glyph_w : 1;
      if ( chars_per_line < 1 ) chars_per_line = 1;
      int line_h = (int)( 16 * cam_zoom );
      if ( line_h < 1 ) line_h = 1;
      int avail_h = (int)( sh - ( NODE_PAD + 16 ) * cam_zoom - NODE_PAD * cam_zoom );
      int max_lines = avail_h / line_h;
      if ( max_lines < 1 ) max_lines = 1;
      int max_chars = max_lines * chars_per_line;

      int text_len = (int)strlen( line2 );
      char buf[256];
      if ( text_len <= max_chars )
      {
        ts.wrap_width = text_w;
        a_DrawText( line2, tx,
                    (int)( sy + ( NODE_PAD + 16 ) * cam_zoom ), ts );
      }
      else
      {
        int trunc = max_chars - 2;
        if ( trunc < 0 ) trunc = 0;
        if ( trunc > (int)sizeof( buf ) - 3 ) trunc = (int)sizeof( buf ) - 3;
        memcpy( buf, line2, trunc );
        buf[trunc] = '.';
        buf[trunc + 1] = '.';
        buf[trunc + 2] = '\0';
        ts.wrap_width = text_w;
        a_DrawText( buf, tx,
                    (int)( sy + ( NODE_PAD + 16 ) * cam_zoom ), ts );
      }
    }
  }
}

/* ---- Main graph draw ---- */

void ne_GraphDraw( aRectf_t panel )
{
  a_DrawFilledRect( panel, (aColor_t){ 25, 30, 35, 220 } );
  a_DrawRect( panel, (aColor_t){ 50, 60, 70, 180 } );

  if ( !g_ne_graph_loaded ) return;

  /* Header above panel */
  aTextStyle_t hdr = {
    .type       = FONT_CODE_PAGE_437,
    .fg         = (aColor_t){ 0x63, 0xc7, 0xb2, 255 },
    .bg         = black,
    .align      = TEXT_ALIGN_LEFT,
    .wrap_width = 0,
    .scale      = 1.0f,
    .padding    = 0
  };
  char title[128];
  snprintf( title, sizeof( title ), "%s - %d nodes [%.0f%%]",
            sp( g_ne_graph.display_name ),
            g_ne_graph.num_nodes, cam_zoom * 100.0f );
  a_DrawText( title, (int)panel.x + 4, (int)panel.y - 14, hdr );

  a_SetClipRect( panel );

  ne_DrawConnections( panel );
  ne_DrawNodes( panel );

  a_DisableClipRect();
}
