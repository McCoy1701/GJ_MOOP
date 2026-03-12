/*
 * npc_editor/npc_editor.c:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"

#include "editor.h"
#include "entity_editor.h"
#include "items_editor.h"
#include "world_editor.h"
#include "npc_editor.h"

static void ne_Logic( float dt );
static void ne_Draw( float dt );

/* Globals */
NEGraph_t    g_ne_graph;
int          g_ne_graph_loaded   = 0;
int          g_ne_selected_node  = -1;

NEFileEntry_t  g_ne_files[NE_MAX_NPC_FILES];
int            g_ne_num_files     = 0;
int            g_ne_selected_file = -1;

NEFolderState_t g_ne_folders[NE_MAX_FOLDERS];
int             g_ne_num_folders = 0;

static int ne_files_scanned = 0;
static int browser_scroll   = 0;

/* Modal state */
static int modal_open       = 0;
static int modal_scroll     = 0;
static int modal_prev_node  = -1;

/* Undo stack */
static NEUndoStack_t g_undo;
static int           g_undo_inited = 0;

/* Track which array field + index is being edited */
static int editing_array_idx = -1;

/* Delete confirmation */
static int   delete_confirm   = 0;
static float delete_confirm_t = 0.0f;  /* auto-cancel after timeout */
#define DELETE_CONFIRM_TIMEOUT 2.0f

/* Node-key picker state */
static int        picker_open   = 0;
static dString_t* picker_target = NULL;  /* dString to write selected key into */
static int        picker_scroll = 0;
static int        picker_is_new_option = 0;  /* 1 = we added a blank option slot */
static char       picker_filter[128];
static int        picker_filter_len = 0;

#define ROW_H    20
#define MODAL_W  620
#define MODAL_H  500
#define CLOSE_SZ  24
#define PICKER_W  300
#define PICKER_H  360
#define PICKER_ROW_H 22

/* Browser row: either a folder header or a file entry */
#define BR_FOLDER 0
#define BR_FILE   1
#define BR_MAX_ROWS (NE_MAX_FOLDERS + NE_MAX_NPC_FILES)

typedef struct
{
  int type;           /* BR_FOLDER or BR_FILE */
  int index;          /* folder index or file index */
} BrowserRow_t;

static BrowserRow_t g_browser_rows[BR_MAX_ROWS];
static int          g_browser_num_rows = 0;

/* ---- Helpers ---- */

static int mouse_in_rect( aRectf_t r )
{
  return app.mouse.x >= r.x && app.mouse.x < r.x + r.w
      && app.mouse.y >= r.y && app.mouse.y < r.y + r.h;
}

static aRectf_t ne_ModalRect( void )
{
  float mx = ( SCREEN_WIDTH  - MODAL_W ) * 0.5f;
  float my = ( SCREEN_HEIGHT - MODAL_H ) * 0.5f;
  return (aRectf_t){ mx, my, MODAL_W, MODAL_H };
}

static aRectf_t ne_CloseRect( aRectf_t mr )
{
  return (aRectf_t){ mr.x + mr.w - CLOSE_SZ - 4, mr.y + 4,
                     CLOSE_SZ, CLOSE_SZ };
}

static aTextStyle_t ne_style( void )
{
  return (aTextStyle_t){
    .type       = FONT_CODE_PAGE_437,
    .fg         = white,
    .bg         = black,
    .align      = TEXT_ALIGN_LEFT,
    .wrap_width = 0,
    .scale      = 1.0f,
    .padding    = 0
  };
}

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

/* ---- Undo helpers ---- */

static void ensure_undo_init( void )
{
  if ( !g_undo_inited )
  {
    ne_UndoInit( &g_undo );
    g_undo_inited = 1;
  }
}

static void push_undo( void )
{
  ensure_undo_init();
  ne_UndoPush( &g_undo, &g_ne_graph );
}

/* ---- Folder helpers ---- */

static void ne_BuildFolders( void )
{
  g_ne_num_folders = 0;

  for ( int i = 0; i < g_ne_num_files; i++ )
  {
    const char* fname = sp( g_ne_files[i].folder );
    if ( fname[0] == '\0' ) continue;  /* root files have no folder row */

    /* Check if folder already exists */
    int found = 0;
    for ( int f = 0; f < g_ne_num_folders; f++ )
    {
      if ( strcmp( g_ne_folders[f].name, fname ) == 0 )
      {
        found = 1;
        break;
      }
    }
    if ( !found && g_ne_num_folders < NE_MAX_FOLDERS )
    {
      strncpy( g_ne_folders[g_ne_num_folders].name, fname,
               sizeof( g_ne_folders[0].name ) - 1 );
      g_ne_folders[g_ne_num_folders].name[127] = '\0';
      g_ne_folders[g_ne_num_folders].expanded = 1;  /* start expanded */
      g_ne_num_folders++;
    }
  }
}

static int ne_FolderExpanded( const char* name )
{
  for ( int f = 0; f < g_ne_num_folders; f++ )
    if ( strcmp( g_ne_folders[f].name, name ) == 0 )
      return g_ne_folders[f].expanded;
  return 1;
}

static void ne_FolderToggle( const char* name )
{
  for ( int f = 0; f < g_ne_num_folders; f++ )
  {
    if ( strcmp( g_ne_folders[f].name, name ) == 0 )
    {
      g_ne_folders[f].expanded = !g_ne_folders[f].expanded;
      return;
    }
  }
}

static void ne_BuildBrowserRows( void )
{
  g_browser_num_rows = 0;
  const char* cur_folder = NULL;

  for ( int i = 0; i < g_ne_num_files; i++ )
  {
    const char* fname = sp( g_ne_files[i].folder );

    /* Emit folder header when folder changes (skip empty = root files) */
    if ( fname[0] != '\0'
         && ( cur_folder == NULL || strcmp( cur_folder, fname ) != 0 ) )
    {
      cur_folder = fname;

      /* Find folder index */
      for ( int f = 0; f < g_ne_num_folders; f++ )
      {
        if ( strcmp( g_ne_folders[f].name, fname ) == 0 )
        {
          g_browser_rows[g_browser_num_rows].type  = BR_FOLDER;
          g_browser_rows[g_browser_num_rows].index = f;
          g_browser_num_rows++;
          break;
        }
      }
    }

    /* Skip files in collapsed folders */
    if ( fname[0] != '\0' && !ne_FolderExpanded( fname ) )
      continue;

    g_browser_rows[g_browser_num_rows].type  = BR_FILE;
    g_browser_rows[g_browser_num_rows].index = i;
    g_browser_num_rows++;
  }
}

/* ---- Init / Destroy ---- */

void e_NPCEditorInit( void )
{
  app.delegate.logic = ne_Logic;
  app.delegate.draw  = ne_Draw;

  app.g_viewport = (aRectf_t){0};

  a_WidgetsInit( "resources/widgets/editor/npc.auf" );
  app.active_widget = a_GetWidget( "tab_bar" );

  aContainerWidget_t* tab_container = a_GetContainerFromWidget( "tab_bar" );
  for ( int i = 0; i < tab_container->num_components; i++ )
  {
    aWidget_t* current = &tab_container->components[i];

    if ( strcmp( current->name, "world" ) == 0 )
      current->action = e_WorldEditorInit;

    if ( strcmp( current->name, "item" ) == 0 )
      current->action = e_ItemEditorInit;

    if ( strcmp( current->name, "entity" ) == 0 )
      current->action = e_EntityEditorInit;

    if ( strcmp( current->name, "npc" ) == 0 )
      current->action = e_NPCEditorInit;
  }

  if ( !ne_files_scanned )
  {
    ne_ScanNPCFiles( "../resources/data/npcs" );
    ne_BuildFolders();
    ne_files_scanned = 1;
  }

  browser_scroll  = 0;
  modal_open      = 0;
  modal_scroll    = 0;
  modal_prev_node = -1;
  editing_array_idx = -1;
  ne_FieldClear();
  ensure_undo_init();
}

void e_NPCEditorDestroy( void )
{
  ne_FieldClear();

  if ( g_undo_inited )
  {
    ne_UndoDestroy( &g_undo );
    g_undo_inited = 0;
  }

  if ( g_ne_graph_loaded )
  {
    ne_GraphDestroy( &g_ne_graph );
    g_ne_graph_loaded = 0;
  }

  for ( int i = 0; i < g_ne_num_files; i++ )
    ne_FileEntryDestroy( &g_ne_files[i] );

  g_ne_selected_node = -1;
  g_ne_selected_file = -1;
  g_ne_num_files     = 0;
  ne_files_scanned   = 0;
}

/* ---- Picker helpers ---- */

static void open_picker( dString_t* target, int is_new_option )
{
  picker_open       = 1;
  picker_target     = target;
  picker_scroll     = 0;
  picker_is_new_option = is_new_option;
  picker_filter[0]  = '\0';
  picker_filter_len = 0;
  SDL_StartTextInput();
}

static void close_picker( int cancelled )
{
  /* If we added a blank option slot and user cancelled, remove it */
  if ( cancelled && picker_is_new_option && g_ne_selected_node >= 0 )
  {
    NENode_t* sel = &g_ne_graph.nodes[g_ne_selected_node];
    if ( sel->num_options > 0 )
    {
      d_StringSet( sel->option_keys[sel->num_options - 1], "" );
      sel->num_options--;
    }
  }
  picker_open          = 0;
  picker_target        = NULL;
  picker_is_new_option = 0;
  SDL_StopTextInput();
}

/* ---- Close modal helper ---- */

static void close_modal( void )
{
  if ( picker_open )
    close_picker( 1 );
  if ( ne_FieldIsActive() )
  {
    push_undo();
    ne_FieldCommit();
    g_ne_graph.dirty = 1;
  }
  modal_open         = 0;
  g_ne_selected_node = -1;
  modal_prev_node    = -1;
  editing_array_idx  = -1;
  delete_confirm     = 0;
}

/* ---- Add / Delete nodes ---- */

static void add_node_at( float wx, float wy )
{
  if ( !g_ne_graph_loaded ) return;
  if ( g_ne_graph.num_nodes >= NE_MAX_NODES ) return;

  push_undo();

  NENode_t* node = &g_ne_graph.nodes[g_ne_graph.num_nodes];
  ne_NodeInit( node );

  char key[64];
  snprintf( key, sizeof( key ), "node_%d", g_ne_graph.num_nodes );
  d_StringSet( node->key, key );

  node->gx = wx;
  node->gy = wy;

  g_ne_graph.num_nodes++;
  g_ne_graph.dirty = 1;

  /* Select and open modal for new node, activate key field for naming */
  g_ne_selected_node = g_ne_graph.num_nodes - 1;
  modal_prev_node    = g_ne_selected_node;
  modal_open         = 1;
  modal_scroll       = 0;
  ne_FieldClear();
  ne_FieldActivate( node->key );
}

static void delete_selected_node( void )
{
  if ( g_ne_selected_node < 0
       || g_ne_selected_node >= g_ne_graph.num_nodes )
    return;

  push_undo();

  int idx = g_ne_selected_node;

  /* Clean up references to this node from other nodes */
  const char* del_key = sp( g_ne_graph.nodes[idx].key );
  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
  {
    if ( i == idx ) continue;
    NENode_t* n = &g_ne_graph.nodes[i];

    /* Remove from option_keys */
    for ( int o = 0; o < n->num_options; o++ )
    {
      if ( strcmp( sp( n->option_keys[o] ), del_key ) == 0 )
      {
        /* Shift remaining options down */
        for ( int k = o; k < n->num_options - 1; k++ )
          d_StringSet( n->option_keys[k],
                       sp( n->option_keys[k + 1] ) );
        d_StringSet( n->option_keys[n->num_options - 1], "" );
        n->num_options--;
        o--;
      }
    }

    /* Clear goto_key if it points to deleted node */
    if ( strcmp( sp( n->goto_key ), del_key ) == 0 )
      d_StringSet( n->goto_key, "" );
  }

  /* Destroy the node */
  ne_NodeDestroy( &g_ne_graph.nodes[idx] );

  /* Shift remaining nodes down */
  for ( int i = idx; i < g_ne_graph.num_nodes - 1; i++ )
    g_ne_graph.nodes[i] = g_ne_graph.nodes[i + 1];

  /* Zero the last slot (pointers were moved, not copied) */
  memset( &g_ne_graph.nodes[g_ne_graph.num_nodes - 1], 0, sizeof( NENode_t ) );

  g_ne_graph.num_nodes--;
  g_ne_graph.dirty = 1;

  close_modal();
}

/* ---- Logic ---- */

static void ne_Logic( float dt )
{
  a_DoInput();

  /* Ctrl+Z / Ctrl+Y — undo/redo (always available) */
  SDL_Keymod mod = SDL_GetModState();
  if ( mod & KMOD_CTRL )
  {
    if ( app.keyboard[SDL_SCANCODE_Z] == 1 )
    {
      app.keyboard[SDL_SCANCODE_Z] = 0;
      ne_FieldCancel();
      if ( ne_Undo( &g_undo, &g_ne_graph ) )
      {
        /* Validate selected node index */
        if ( g_ne_selected_node >= g_ne_graph.num_nodes )
          g_ne_selected_node = -1;
        modal_prev_node = g_ne_selected_node;
      }
      return;
    }
    if ( app.keyboard[SDL_SCANCODE_Y] == 1 )
    {
      app.keyboard[SDL_SCANCODE_Y] = 0;
      ne_FieldCancel();
      if ( ne_Redo( &g_undo, &g_ne_graph ) )
      {
        if ( g_ne_selected_node >= g_ne_graph.num_nodes )
          g_ne_selected_node = -1;
        modal_prev_node = g_ne_selected_node;
      }
      return;
    }
  }

  /* Escape */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;

    if ( picker_open )
    {
      close_picker( 1 );
      return;
    }

    if ( ne_FieldIsActive() )
    {
      ne_FieldCancel();
      return;
    }

    if ( modal_open )
    {
      close_modal();
      return;
    }

    e_NPCEditorDestroy();
    EditorInit();
    return;
  }

  /* ---- Picker input ---- */
  if ( picker_open )
  {
    /* Text input for filter */
    if ( app.input_text[0] != '\0' )
    {
      int ch_len = (int)strlen( app.input_text );
      if ( picker_filter_len + ch_len < (int)sizeof( picker_filter ) - 1 )
      {
        memcpy( picker_filter + picker_filter_len, app.input_text, ch_len );
        picker_filter_len += ch_len;
        picker_filter[picker_filter_len] = '\0';
      }
      memset( app.input_text, 0, sizeof( app.input_text ) );
    }
    if ( app.keyboard[SDL_SCANCODE_BACKSPACE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_BACKSPACE] = 0;
      if ( picker_filter_len > 0 )
        picker_filter[--picker_filter_len] = '\0';
    }

    /* Picker click handling is in draw pass (like fields) */
    /* Scroll is handled in modal scroll section */
    return;
  }

  /* Return commits active field */
  if ( ne_FieldIsActive() && app.keyboard[SDL_SCANCODE_RETURN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    push_undo();
    ne_FieldCommit();
    g_ne_graph.dirty   = 1;
    editing_array_idx  = -1;
    return;
  }

  /* Tab commits and moves to next field (future enhancement) */
  if ( ne_FieldIsActive() && app.keyboard[SDL_SCANCODE_TAB] == 1 )
  {
    app.keyboard[SDL_SCANCODE_TAB] = 0;
    push_undo();
    ne_FieldCommit();
    g_ne_graph.dirty   = 1;
    editing_array_idx  = -1;
    return;
  }

  /* Field text input processing */
  if ( ne_FieldIsActive() )
  {
    ne_FieldLogic( dt );
    return;
  }

  /* Open modal when graph selects a new node */
  if ( g_ne_selected_node >= 0 && g_ne_selected_node != modal_prev_node )
  {
    modal_open      = 1;
    modal_scroll    = 0;
    modal_prev_node = g_ne_selected_node;
    editing_array_idx = -1;
    ne_GraphClearInput();
    ne_FieldClear();
  }

  /* Modal input */
  if ( modal_open )
  {
    aRectf_t mr = ne_ModalRect();
    aRectf_t cr = ne_CloseRect( mr );

    /* Scroll inside modal */
    if ( mouse_in_rect( mr ) && app.mouse.wheel != 0 )
    {
      modal_scroll -= app.mouse.wheel * 20;
      if ( modal_scroll < 0 ) modal_scroll = 0;
      app.mouse.wheel = 0;
    }

    /* Delete confirmation timeout */
    if ( delete_confirm )
    {
      delete_confirm_t += dt;
      if ( delete_confirm_t >= DELETE_CONFIRM_TIMEOUT )
        delete_confirm = 0;
    }

    /* Delete key — two-press confirm */
    if ( app.keyboard[SDL_SCANCODE_DELETE] == 1 && !ne_FieldIsActive() )
    {
      app.keyboard[SDL_SCANCODE_DELETE] = 0;
      if ( delete_confirm )
      {
        delete_confirm = 0;
        delete_selected_node();
        return;
      }
      else
      {
        delete_confirm   = 1;
        delete_confirm_t = 0.0f;
      }
    }

    /* Click close button or outside modal */
    if ( app.mouse.button == 1 && app.mouse.state == 1 )
    {
      if ( mouse_in_rect( cr ) || !mouse_in_rect( mr ) )
      {
        close_modal();
        app.mouse.button = 0; app.mouse.state = 0;
        return;
      }
      /* Don't consume click — draw pass detects field clicks */
    }

    /* Skip a_DoWidget() while modal is open so clicks reach field draw */
    return;
  }

  /* Add node: N key when no modal */
  if ( app.keyboard[SDL_SCANCODE_N] == 1 && g_ne_graph_loaded )
  {
    app.keyboard[SDL_SCANCODE_N] = 0;
    add_node_at( ne_GraphCamX(), ne_GraphCamY() );
    a_DoWidget();
    return;
  }

  aContainerWidget_t* bp = a_GetContainerFromWidget( "npc_browser" );
  aContainerWidget_t* np = a_GetContainerFromWidget( "npc_nodelist" );

  ne_BuildBrowserRows();
  int browser_max_rows = (int)bp->rect.h / ROW_H;

  /* Browser scroll + click */
  if ( mouse_in_rect( bp->rect ) )
  {
    if ( app.mouse.wheel != 0 )
    {
      browser_scroll -= app.mouse.wheel;
      int max_s = g_browser_num_rows - browser_max_rows;
      if ( max_s < 0 ) max_s = 0;
      if ( browser_scroll < 0 ) browser_scroll = 0;
      if ( browser_scroll > max_s ) browser_scroll = max_s;
      app.mouse.wheel = 0;
    }

    if ( app.mouse.button == 1 && app.mouse.state == 1 )
    {
      int row_idx = browser_scroll
                    + (int)( app.mouse.y - bp->rect.y ) / ROW_H;
      if ( row_idx >= 0 && row_idx < g_browser_num_rows )
      {
        BrowserRow_t* row = &g_browser_rows[row_idx];

        if ( row->type == BR_FOLDER )
        {
          /* Toggle folder expand/collapse */
          ne_FolderToggle( g_ne_folders[row->index].name );
          ne_BuildBrowserRows();
          /* Clamp scroll after collapse */
          int max_s = g_browser_num_rows - browser_max_rows;
          if ( max_s < 0 ) max_s = 0;
          if ( browser_scroll > max_s ) browser_scroll = max_s;
        }
        else if ( row->index != g_ne_selected_file )
        {
          int clicked = row->index;
          g_ne_selected_file = clicked;
          g_ne_selected_node = -1;
          modal_prev_node    = -1;
          modal_open         = 0;
          modal_scroll       = 0;
          ne_FieldClear();

          /* Reset undo stack on file change */
          if ( g_undo_inited )
            ne_UndoDestroy( &g_undo );
          ne_UndoInit( &g_undo );
          g_undo_inited = 1;

          ne_GraphCtxMenuClose();
          ne_LoadNPCFile( d_StringPeek( g_ne_files[clicked].filename ),
                          d_StringPeek( g_ne_files[clicked].stem ) );
        }
      }
      app.mouse.button = 0; app.mouse.state = 0;
    }
  }

  /* Context menu click handling (before graph logic consumes the click) */
  if ( ne_GraphCtxMenuOpen() && !modal_open
       && app.mouse.button == 1 && app.mouse.state == 1 )
  {
    int cx = ne_GraphCtxMenuSX();
    int cy = ne_GraphCtxMenuSY();
    int cw = 180;
    int node_idx = ne_GraphCtxMenuNode();
    int is_option_node = 0;
    if ( node_idx >= 0 )
      is_option_node = slen( g_ne_graph.nodes[node_idx].label );
    int on_node = node_idx >= 0 && !is_option_node;
    int ch = on_node ? 48 : 26;
    aRectf_t menu_rect = { (float)cx, (float)cy, (float)cw, (float)ch };

    if ( mouse_in_rect( menu_rect ) )
    {
      /* Which item was clicked? */
      int item = ( app.mouse.y - cy ) / 24;
      app.mouse.button = 0; app.mouse.state = 0;

      if ( on_node )
      {
        /* Right-clicked on a speech/start node — item 0 = "Add Option", item 1 = "New Node" */
        if ( item == 0 )
        {
          /* Create a new option node linked from this node */
          int src_idx = ne_GraphCtxMenuNode();
          NENode_t* src = &g_ne_graph.nodes[src_idx];

          if ( src->num_options < NE_MAX_OPTIONS
               && g_ne_graph.num_nodes < NE_MAX_NODES )
          {
            push_undo();

            /* Create new node */
            NENode_t* nn = &g_ne_graph.nodes[g_ne_graph.num_nodes];
            ne_NodeInit( nn );
            char key[64];
            snprintf( key, sizeof( key ), "opt_%d", g_ne_graph.num_nodes );
            d_StringSet( nn->key, key );
            nn->gx = src->gx + 260.0f;
            nn->gy = src->gy + (float)src->num_options * 100.0f;
            g_ne_graph.num_nodes++;

            /* Link it as option on the source node */
            d_StringSet( src->option_keys[src->num_options], key );
            src->num_options++;
            g_ne_graph.dirty = 1;

            /* Open modal for the new node */
            ne_GraphCtxMenuClose();
            g_ne_selected_node = g_ne_graph.num_nodes - 1;
            modal_prev_node    = g_ne_selected_node;
            modal_open         = 1;
            modal_scroll       = 0;
            ne_FieldClear();
            ne_FieldActivate( nn->key );
            return;
          }
        }
        else
        {
          /* "New Node" at this position */
          float wx = ne_GraphCtxMenuWX();
          float wy = ne_GraphCtxMenuWY();
          ne_GraphCtxMenuClose();
          add_node_at( wx, wy );
          return;
        }
      }
      else
      {
        /* Right-clicked on empty space — only "New Node" */
        float wx = ne_GraphCtxMenuWX();
        float wy = ne_GraphCtxMenuWY();
        ne_GraphCtxMenuClose();
        add_node_at( wx, wy );
        return;
      }
    }
    else
    {
      /* Clicked outside menu — close it */
      ne_GraphCtxMenuClose();
      /* Don't consume click — let graph logic handle it */
    }
  }

  /* Graph canvas input */
  if ( g_ne_graph_loaded && mouse_in_rect( np->rect ) )
    ne_GraphLogic( np->rect );

  a_DoWidget();
}

/* ---- Draw browser panel ---- */

static void ne_DrawBrowserPanel( void )
{
  aContainerWidget_t* bp = a_GetContainerFromWidget( "npc_browser" );
  aRectf_t r = bp->rect;

  a_DrawFilledRect( r, (aColor_t){ 30, 30, 40, 220 } );
  a_DrawRect( r, (aColor_t){ 60, 60, 80, 180 } );

  aTextStyle_t hdr = ne_style();
  hdr.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
  a_DrawText( "NPC Files", (int)r.x + 4, (int)r.y - 14, hdr );

  ne_BuildBrowserRows();
  a_SetClipRect( r );

  aTextStyle_t style = ne_style();
  int max_rows = (int)r.h / ROW_H;
  int y = (int)r.y + 2;

  for ( int i = browser_scroll;
        i < g_browser_num_rows && ( i - browser_scroll ) < max_rows;
        i++ )
  {
    BrowserRow_t* row = &g_browser_rows[i];

    if ( row->type == BR_FOLDER )
    {
      /* Folder header row */
      NEFolderState_t* fs = &g_ne_folders[row->index];
      a_DrawFilledRect( (aRectf_t){ r.x, (float)y, r.w, ROW_H },
                        (aColor_t){ 40, 40, 55, 220 } );
      style.fg = (aColor_t){ 0xde, 0x9e, 0x41, 230 };
      const char* arrow = fs->expanded ? "v " : "> ";
      char folder_label[140];
      snprintf( folder_label, sizeof( folder_label ), "%s%s/",
                arrow, fs->name );
      a_DrawText( folder_label, (int)r.x + 4, y + 2, style );
    }
    else
    {
      /* File entry row */
      int file_idx = row->index;
      int indent = ( sp( g_ne_files[file_idx].folder )[0] != '\0' ) ? 14 : 4;

      if ( file_idx == g_ne_selected_file )
      {
        a_DrawFilledRect( (aRectf_t){ r.x, (float)y, r.w, ROW_H },
                          (aColor_t){ 60, 60, 100, 200 } );
        style.fg = (aColor_t){ 255, 255, 100, 255 };
      }
      else
      {
        style.fg = white;
      }

      const char* label = slen( g_ne_files[file_idx].display_name )
                          ? sp( g_ne_files[file_idx].display_name )
                          : sp( g_ne_files[file_idx].stem );
      a_DrawText( label, (int)r.x + indent, y + 2, style );
    }

    y += ROW_H;
  }

  a_DisableClipRect();
}

/* ---- Draw node-key picker overlay ---- */

static void ne_DrawPicker( void )
{
  if ( !picker_open ) return;

  float px = ( SCREEN_WIDTH  - PICKER_W ) * 0.5f;
  float py = ( SCREEN_HEIGHT - PICKER_H ) * 0.5f;
  aRectf_t pr = { px, py, PICKER_W, PICKER_H };

  /* Background */
  a_DrawFilledRect( (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                    (aColor_t){ 0, 0, 0, 100 } );
  a_DrawFilledRect( pr, (aColor_t){ 25, 30, 45, 245 } );
  a_DrawRect( pr, (aColor_t){ 80, 140, 220, 255 } );

  aTextStyle_t ts = ne_style();

  /* Title */
  ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
  a_DrawText( "Select Node Key", (int)px + 10, (int)py + 8, ts );

  /* Filter box */
  int fx = (int)px + 10;
  int fy = (int)py + 28;
  int fw = PICKER_W - 20;
  aRectf_t filter_rect = { (float)fx - 2, (float)fy - 2,
                            (float)fw + 4, 20.0f };
  a_DrawFilledRect( filter_rect, (aColor_t){ 15, 20, 30, 255 } );
  a_DrawRect( filter_rect, (aColor_t){ 60, 100, 160, 200 } );

  ts.fg = white;
  if ( picker_filter_len > 0 )
    a_DrawText( picker_filter, fx, fy, ts );
  else
  {
    ts.fg = (aColor_t){ 80, 80, 100, 255 };
    a_DrawText( "type to filter...", fx, fy, ts );
  }

  /* List area */
  int ly = fy + 24;
  int lh = PICKER_H - ( ly - (int)py ) - 10;
  aRectf_t list_rect = { (float)fx, (float)ly, (float)fw, (float)lh };
  a_SetClipRect( list_rect );

  /* Scroll */
  if ( mouse_in_rect( pr ) && app.mouse.wheel != 0 )
  {
    picker_scroll -= app.mouse.wheel * PICKER_ROW_H;
    if ( picker_scroll < 0 ) picker_scroll = 0;
    app.mouse.wheel = 0;
  }

  /* Build filtered list and draw */
  int row_y = ly - picker_scroll;
  int visible_count = 0;

  /* "[+ New Node]" entry at top */
  {
    aRectf_t row_rect = { (float)fx, (float)row_y, (float)fw, (float)PICKER_ROW_H };
    if ( row_y + PICKER_ROW_H > ly && row_y < ly + lh )
    {
      int hovered = mouse_in_rect( row_rect );
      if ( hovered )
      {
        a_DrawFilledRect( row_rect, (aColor_t){ 40, 80, 40, 200 } );
        if ( app.mouse.button == 1 && app.mouse.state == 1 )
        {
          /* Create a new node and link it, stay on current modal */
          app.mouse.button = 0; app.mouse.state = 0;
          a_DisableClipRect();

          push_undo();

          if ( g_ne_graph.num_nodes < NE_MAX_NODES )
          {
            NENode_t* nn = &g_ne_graph.nodes[g_ne_graph.num_nodes];
            ne_NodeInit( nn );
            char key[64];
            snprintf( key, sizeof( key ), "node_%d", g_ne_graph.num_nodes );
            d_StringSet( nn->key, key );
            nn->gx = ne_GraphCamX() + 120.0f;
            nn->gy = ne_GraphCamY() + (float)g_ne_graph.num_nodes * 30.0f;
            g_ne_graph.num_nodes++;
            g_ne_graph.dirty = 1;

            /* Link the new node key into the picker target */
            if ( picker_target )
              d_StringSet( picker_target, key );

            close_picker( 0 );
            /* Stay on the current node's modal — don't switch */
          }
          return;
        }
      }
      ts.fg = (aColor_t){ 100, 220, 100, 255 };
      a_DrawText( "[+ Create New Node]", fx + 4, row_y + 3, ts );
    }
    row_y += PICKER_ROW_H;
    visible_count++;
  }

  for ( int i = 0; i < g_ne_graph.num_nodes; i++ )
  {
    /* Skip current node */
    if ( i == g_ne_selected_node ) continue;

    const char* key = sp( g_ne_graph.nodes[i].key );
    if ( !key || !key[0] ) continue;

    /* Apply filter */
    if ( picker_filter_len > 0 )
    {
      /* Case-insensitive substring match */
      int match = 0;
      int klen = (int)strlen( key );
      for ( int k = 0; k <= klen - picker_filter_len; k++ )
      {
        int ok = 1;
        for ( int f = 0; f < picker_filter_len; f++ )
        {
          char a = key[k + f];
          char b = picker_filter[f];
          if ( a >= 'A' && a <= 'Z' ) a += 32;
          if ( b >= 'A' && b <= 'Z' ) b += 32;
          if ( a != b ) { ok = 0; break; }
        }
        if ( ok ) { match = 1; break; }
      }
      if ( !match ) continue;
    }

    aRectf_t row_rect = { (float)fx, (float)row_y, (float)fw, (float)PICKER_ROW_H };

    if ( row_y + PICKER_ROW_H > ly && row_y < ly + lh )
    {
      int hovered = mouse_in_rect( row_rect );
      if ( hovered )
        a_DrawFilledRect( row_rect, (aColor_t){ 40, 50, 80, 200 } );

      /* Node key */
      ts.fg = white;
      a_DrawText( key, fx + 4, row_y + 3, ts );

      /* Preview: label or text snippet */
      const char* preview = slen( g_ne_graph.nodes[i].label )
                            ? sp( g_ne_graph.nodes[i].label )
                            : slen( g_ne_graph.nodes[i].text )
                            ? sp( g_ne_graph.nodes[i].text )
                            : NULL;
      if ( preview )
      {
        /* Show first ~30 chars of preview after the key */
        int kpx = (int)strlen( key ) * 9 + 12;
        if ( kpx < 140 ) kpx = 140;
        ts.fg = (aColor_t){ 120, 120, 140, 255 };
        char prev_buf[36];
        snprintf( prev_buf, sizeof( prev_buf ), "%.32s%s",
                  preview, (int)strlen( preview ) > 32 ? ".." : "" );
        a_DrawText( prev_buf, fx + kpx, row_y + 3, ts );
      }

      /* Click to select */
      if ( hovered && app.mouse.button == 1 && app.mouse.state == 1 )
      {
        app.mouse.button = 0; app.mouse.state = 0;
        a_DisableClipRect();

        push_undo();
        if ( picker_target )
          d_StringSet( picker_target, key );
        g_ne_graph.dirty = 1;
        close_picker( 0 );
        return;
      }
    }

    row_y += PICKER_ROW_H;
    visible_count++;
  }

  /* Clamp scroll */
  int max_scroll = visible_count * PICKER_ROW_H - lh;
  if ( max_scroll < 0 ) max_scroll = 0;
  if ( picker_scroll > max_scroll ) picker_scroll = max_scroll;

  a_DisableClipRect();

  /* Cancel button */
  aRectf_t cancel_btn = { px + PICKER_W - 70, py + 4, 60, 20 };
  int cancel_hover = mouse_in_rect( cancel_btn );
  a_DrawFilledRect( cancel_btn, cancel_hover
                    ? (aColor_t){ 140, 40, 40, 220 }
                    : (aColor_t){ 80, 30, 30, 200 } );
  a_DrawRect( cancel_btn, (aColor_t){ 180, 60, 60, 200 } );
  ts.fg = (aColor_t){ 255, 140, 140, 255 };
  a_DrawText( "Cancel", (int)cancel_btn.x + 6, (int)cancel_btn.y + 3, ts );

  if ( cancel_hover && app.mouse.button == 1 && app.mouse.state == 1 )
  {
    app.mouse.button = 0; app.mouse.state = 0;
    close_picker( 1 );
  }
}

/* ---- Draw editable modal ---- */

static void ne_DrawEditModal( void )
{
  if ( !modal_open ) return;
  if ( g_ne_selected_node < 0
       || g_ne_selected_node >= g_ne_graph.num_nodes )
    return;

  /* Dim background */
  a_DrawFilledRect( (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                    (aColor_t){ 0, 0, 0, 140 } );

  aRectf_t r  = ne_ModalRect();
  aRectf_t cr = ne_CloseRect( r );

  /* Modal box */
  a_DrawFilledRect( r, (aColor_t){ 20, 25, 35, 240 } );
  a_DrawRect( r, (aColor_t){ 100, 110, 130, 255 } );

  /* Close button */
  a_DrawFilledRect( cr, (aColor_t){ 140, 40, 40, 220 } );
  a_DrawRect( cr, (aColor_t){ 200, 80, 80, 255 } );
  aTextStyle_t xts = ne_style();
  xts.fg = white;
  a_DrawText( "X", (int)cr.x + 7, (int)cr.y + 4, xts );

  /* Delete button (left of close) — shows CONFIRM? after first click */
  int dw = delete_confirm ? 90 : 52;
  aRectf_t del_btn = { cr.x - (float)dw - 8, cr.y, (float)dw, (float)CLOSE_SZ };
  if ( delete_confirm )
  {
    a_DrawFilledRect( del_btn, (aColor_t){ 180, 40, 40, 240 } );
    a_DrawRect( del_btn, (aColor_t){ 255, 80, 80, 255 } );
    xts.fg = (aColor_t){ 255, 255, 100, 255 };
    a_DrawText( "CONFIRM?", (int)del_btn.x + 8, (int)del_btn.y + 4, xts );
  }
  else
  {
    a_DrawFilledRect( del_btn, (aColor_t){ 100, 30, 30, 220 } );
    a_DrawRect( del_btn, (aColor_t){ 180, 60, 60, 255 } );
    xts.fg = (aColor_t){ 255, 120, 120, 255 };
    a_DrawText( "DEL", (int)del_btn.x + 12, (int)del_btn.y + 4, xts );
  }

  /* Handle delete button click */
  if ( app.mouse.button == 1 && app.mouse.state == 1
       && mouse_in_rect( del_btn ) )
  {
    app.mouse.button = 0; app.mouse.state = 0;
    if ( delete_confirm )
    {
      delete_confirm = 0;
      delete_selected_node();
      return;
    }
    else
    {
      delete_confirm   = 1;
      delete_confirm_t = 0.0f;
    }
  }

  /* Dirty indicator */
  if ( g_ne_graph.dirty )
  {
    xts.fg = (aColor_t){ 255, 100, 100, 255 };
    a_DrawText( "*", (int)r.x + 8, (int)r.y + 6, xts );
  }

  a_SetClipRect( r );

  NENode_t* sel = &g_ne_graph.nodes[g_ne_selected_node];

  int dx = (int)r.x + 10;
  int dy = (int)r.y + 34 - modal_scroll;
  int fw = (int)r.w - 24;

  int clicked = 0;

  /* -- Key -- */
  clicked = 0;
  dy = ne_DrawFieldRow( "key:", sp( sel->key ),
                        ne_FieldIsTarget( sel->key ), dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->key );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* -- Start toggle -- */
  clicked = 0;
  dy = ne_DrawToggleRow( "start:", sel->is_start, dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    sel->is_start = !sel->is_start;
    g_ne_graph.dirty = 1;
    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* -- Priority -- */
  if ( sel->is_start )
  {
    char prio_str[32];
    snprintf( prio_str, sizeof( prio_str ), "%d", sel->priority );
    clicked = 0;
    dy = ne_DrawFieldRow( "priority:", prio_str,
                          ne_FieldIsIntTarget( &sel->priority ),
                          dx, dy, fw, &clicked );
    if ( clicked )
    {
      push_undo();
      ne_FieldActivateInt( &sel->priority );
      app.mouse.button = 0; app.mouse.state = 0;
    }
  }

  /* -- Text -- */
  dy += 4;
  dy = ne_DrawSectionHeader( "Speech", dx, dy );
  clicked = 0;
  dy = ne_DrawFieldRow( "text:", sp( sel->text ),
                        ne_FieldIsTarget( sel->text ), dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->text );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* -- Options array -- */
  {
    int click_idx = -1, click_add = 0, click_remove = -1;
    dy = ne_DrawArrayField( "options:", sel->option_keys,
                            sel->num_options, NE_MAX_OPTIONS,
                            dx, dy, fw, -1,
                            &click_idx, &click_add, &click_remove );

    if ( click_add && sel->num_options < NE_MAX_OPTIONS )
    {
      /* Add blank slot and open picker to choose a node key */
      d_StringSet( sel->option_keys[sel->num_options], "" );
      sel->num_options++;
      g_ne_graph.dirty = 1;
      a_DisableClipRect();
      open_picker( sel->option_keys[sel->num_options - 1], 1 );
      app.mouse.button = 0; app.mouse.state = 0;
      return;
    }
    if ( click_remove >= 0 && click_remove < sel->num_options )
    {
      push_undo();
      for ( int k = click_remove; k < sel->num_options - 1; k++ )
        d_StringSet( sel->option_keys[k], sp( sel->option_keys[k + 1] ) );
      d_StringSet( sel->option_keys[sel->num_options - 1], "" );
      sel->num_options--;
      g_ne_graph.dirty = 1;
      app.mouse.button = 0; app.mouse.state = 0;
    }
    if ( click_idx >= 0 )
    {
      /* Open picker to change existing option key */
      a_DisableClipRect();
      open_picker( sel->option_keys[click_idx], 0 );
      app.mouse.button = 0; app.mouse.state = 0;
      return;
    }
  }

  /* -- Option fields -- */
  dy += 4;
  dy = ne_DrawSectionHeader( "Option", dx, dy );

  clicked = 0;
  dy = ne_DrawFieldRow( "label:", sp( sel->label ),
                        ne_FieldIsTarget( sel->label ), dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->label );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  clicked = 0;
  dy = ne_DrawFieldRow( "goto:", sp( sel->goto_key ),
                        0, dx, dy, fw, &clicked );
  if ( clicked )
  {
    /* Open picker to choose goto target */
    a_DisableClipRect();
    open_picker( sel->goto_key, 0 );
    app.mouse.button = 0; app.mouse.state = 0;
    return;
  }

  clicked = 0;
  dy = ne_DrawFieldRow( "color:", sp( sel->color_name ),
                        ne_FieldIsTarget( sel->color_name ), dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->color_name );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* -- Conditions -- */
  dy += 4;
  dy = ne_DrawSectionHeader( "Conditions", dx, dy );

  clicked = 0;
  dy = ne_DrawFieldRow( "require_class:", sp( sel->require_class ),
                        ne_FieldIsTarget( sel->require_class ),
                        dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->require_class );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  clicked = 0;
  dy = ne_DrawFieldRow( "require_item:", sp( sel->require_item ),
                        ne_FieldIsTarget( sel->require_item ),
                        dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->require_item );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  clicked = 0;
  dy = ne_DrawFieldRow( "require_flag_min:", sp( sel->require_flag_min ),
                        ne_FieldIsTarget( sel->require_flag_min ),
                        dx, dy, fw, &clicked );
  if ( clicked )
  {
    push_undo();
    ne_FieldActivate( sel->require_flag_min );
    app.mouse.button = 0; app.mouse.state = 0;
  }

  /* require_flag array */
  {
    int ci = -1, ca = 0, cr2 = -1;
    dy = ne_DrawArrayField( "require_flag:", sel->require_flag,
                            sel->num_require_flag, NE_MAX_CONDITIONS,
                            dx, dy, fw, -1, &ci, &ca, &cr2 );
    if ( ca && sel->num_require_flag < NE_MAX_CONDITIONS )
    {
      push_undo();
      sel->num_require_flag++;
      ne_FieldActivate( sel->require_flag[sel->num_require_flag - 1] );
      g_ne_graph.dirty = 1;
      app.mouse.button = 0; app.mouse.state = 0;
    }
    if ( cr2 >= 0 && cr2 < sel->num_require_flag )
    {
      push_undo();
      for ( int k = cr2; k < sel->num_require_flag - 1; k++ )
        d_StringSet( sel->require_flag[k], sp( sel->require_flag[k + 1] ) );
      d_StringSet( sel->require_flag[sel->num_require_flag - 1], "" );
      sel->num_require_flag--;
      g_ne_graph.dirty = 1;
      app.mouse.button = 0; app.mouse.state = 0;
    }
    if ( ci >= 0 )
    {
      push_undo();
      ne_FieldActivate( sel->require_flag[ci] );
      app.mouse.button = 0; app.mouse.state = 0;
    }
  }

  /* require_not_flag array */
  {
    int ci = -1, ca = 0, cr2 = -1;
    dy = ne_DrawArrayField( "require_not_flag:", sel->require_not_flag,
                            sel->num_require_not_flag, NE_MAX_CONDITIONS,
                            dx, dy, fw, -1, &ci, &ca, &cr2 );
    if ( ca && sel->num_require_not_flag < NE_MAX_CONDITIONS )
    {
      push_undo();
      sel->num_require_not_flag++;
      ne_FieldActivate( sel->require_not_flag[sel->num_require_not_flag - 1] );
      g_ne_graph.dirty = 1;
      app.mouse.button = 0; app.mouse.state = 0;
    }
    if ( cr2 >= 0 && cr2 < sel->num_require_not_flag )
    {
      push_undo();
      for ( int k = cr2; k < sel->num_require_not_flag - 1; k++ )
        d_StringSet( sel->require_not_flag[k], sp( sel->require_not_flag[k + 1] ) );
      d_StringSet( sel->require_not_flag[sel->num_require_not_flag - 1], "" );
      sel->num_require_not_flag--;
      g_ne_graph.dirty = 1;
      app.mouse.button = 0; app.mouse.state = 0;
    }
    if ( ci >= 0 )
    {
      push_undo();
      ne_FieldActivate( sel->require_not_flag[ci] );
      app.mouse.button = 0; app.mouse.state = 0;
    }
  }

  /* require_gold_min */
  {
    char gm[32];
    snprintf( gm, sizeof( gm ), "%d", sel->require_gold_min );
    clicked = 0;
    dy = ne_DrawFieldRow( "require_gold_min:", gm,
                          ne_FieldIsIntTarget( &sel->require_gold_min ),
                          dx, dy, fw, &clicked );
    if ( clicked )
    {
      push_undo();
      ne_FieldActivateInt( &sel->require_gold_min );
      app.mouse.button = 0; app.mouse.state = 0;
    }
  }

  /* -- Actions -- */
  dy += 4;
  dy = ne_DrawSectionHeader( "Actions", dx, dy );

  #define ACTION_FIELD( lbl, field ) do { \
    clicked = 0; \
    dy = ne_DrawFieldRow( lbl, sp( sel->field ), \
                          ne_FieldIsTarget( sel->field ), \
                          dx, dy, fw, &clicked ); \
    if ( clicked ) { \
      push_undo(); \
      ne_FieldActivate( sel->field ); \
      app.mouse.button = 0; app.mouse.state = 0; \
    } \
  } while(0)

  ACTION_FIELD( "set_flag:",   set_flag );
  ACTION_FIELD( "incr_flag:",  incr_flag );
  ACTION_FIELD( "clear_flag:", clear_flag );
  ACTION_FIELD( "give_item:",  give_item );
  ACTION_FIELD( "take_item:",  take_item );
  ACTION_FIELD( "set_lore:",   set_lore );
  ACTION_FIELD( "action:",     action );

  #undef ACTION_FIELD

  /* give_gold */
  {
    char gg[32];
    snprintf( gg, sizeof( gg ), "%d", sel->give_gold );
    clicked = 0;
    dy = ne_DrawFieldRow( "give_gold:", gg,
                          ne_FieldIsIntTarget( &sel->give_gold ),
                          dx, dy, fw, &clicked );
    if ( clicked )
    {
      push_undo();
      ne_FieldActivateInt( &sel->give_gold );
      app.mouse.button = 0; app.mouse.state = 0;
    }
  }

  a_DisableClipRect();
}

/* ---- Main draw ---- */

static void ne_Draw( float dt )
{
  (void)dt;

  a_DrawWidgets();

  aContainerWidget_t* np = a_GetContainerFromWidget( "npc_nodelist" );

  ne_DrawBrowserPanel();
  ne_GraphDraw( np->rect );

  /* Right-click context menu on graph canvas (draw only, clicks handled in logic) */
  if ( ne_GraphCtxMenuOpen() && !modal_open )
  {
    int cx = ne_GraphCtxMenuSX();
    int cy = ne_GraphCtxMenuSY();
    int cw = 180;
    int node_idx = ne_GraphCtxMenuNode();
    int is_option_node = 0;
    if ( node_idx >= 0 )
      is_option_node = slen( g_ne_graph.nodes[node_idx].label );
    int on_node = node_idx >= 0 && !is_option_node;
    int ch = on_node ? 48 : 26;

    aRectf_t menu_bg = { (float)cx, (float)cy, (float)cw, (float)ch };
    a_DrawFilledRect( menu_bg, (aColor_t){ 30, 35, 50, 245 } );
    a_DrawRect( menu_bg, (aColor_t){ 80, 100, 140, 255 } );

    aTextStyle_t cts = ne_style();
    int iy = cy + 2;

    if ( on_node )
    {
      /* Item 0: Add Option */
      aRectf_t r0 = { (float)cx + 2, (float)iy, (float)cw - 4, 22.0f };
      if ( mouse_in_rect( r0 ) )
        a_DrawFilledRect( r0, (aColor_t){ 50, 70, 110, 200 } );
      cts.fg = (aColor_t){ 100, 200, 255, 255 };
      a_DrawText( "+ Add Option Node", cx + 8, iy + 4, cts );
      iy += 22;
    }

    /* Item: Create New Node */
    aRectf_t r1 = { (float)cx + 2, (float)iy, (float)cw - 4, 22.0f };
    if ( mouse_in_rect( r1 ) )
      a_DrawFilledRect( r1, (aColor_t){ 50, 70, 110, 200 } );
    cts.fg = (aColor_t){ 100, 220, 100, 255 };
    a_DrawText( "+ Create New Node", cx + 8, iy + 4, cts );
  }

  ne_DrawEditModal();
  ne_DrawPicker();
}
