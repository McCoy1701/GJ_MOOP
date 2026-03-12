/*
 * npc_editor/ne_Browse.c:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "npc_editor.h"

/* ---- Init / Destroy helpers ---- */

void ne_NodeInit( NENode_t* node )
{
  memset( node, 0, sizeof( NENode_t ) );
  node->key              = d_StringInit();
  node->text             = d_StringInit();
  node->label            = d_StringInit();
  node->goto_key         = d_StringInit();
  node->color_name       = d_StringInit();
  node->require_class    = d_StringInit();
  node->require_flag_min = d_StringInit();
  node->require_item     = d_StringInit();
  node->set_flag         = d_StringInit();
  node->incr_flag        = d_StringInit();
  node->clear_flag       = d_StringInit();
  node->give_item        = d_StringInit();
  node->take_item        = d_StringInit();
  node->set_lore         = d_StringInit();
  node->action           = d_StringInit();

  for ( int i = 0; i < NE_MAX_OPTIONS; i++ )
    node->option_keys[i] = d_StringInit();
  for ( int i = 0; i < NE_MAX_CONDITIONS; i++ )
  {
    node->require_flag[i]     = d_StringInit();
    node->require_not_flag[i] = d_StringInit();
    node->require_lore[i]     = d_StringInit();
    node->require_not_lore[i] = d_StringInit();
  }
}

void ne_NodeDestroy( NENode_t* node )
{
  d_StringDestroy( node->key );
  d_StringDestroy( node->text );
  d_StringDestroy( node->label );
  d_StringDestroy( node->goto_key );
  d_StringDestroy( node->color_name );
  d_StringDestroy( node->require_class );
  d_StringDestroy( node->require_flag_min );
  d_StringDestroy( node->require_item );
  d_StringDestroy( node->set_flag );
  d_StringDestroy( node->incr_flag );
  d_StringDestroy( node->clear_flag );
  d_StringDestroy( node->give_item );
  d_StringDestroy( node->take_item );
  d_StringDestroy( node->set_lore );
  d_StringDestroy( node->action );

  for ( int i = 0; i < NE_MAX_OPTIONS; i++ )
    d_StringDestroy( node->option_keys[i] );
  for ( int i = 0; i < NE_MAX_CONDITIONS; i++ )
  {
    d_StringDestroy( node->require_flag[i] );
    d_StringDestroy( node->require_not_flag[i] );
    d_StringDestroy( node->require_lore[i] );
    d_StringDestroy( node->require_not_lore[i] );
  }
}

void ne_GraphInit( NEGraph_t* graph )
{
  memset( graph, 0, sizeof( NEGraph_t ) );
  graph->npc_key      = d_StringInit();
  graph->display_name = d_StringInit();
  graph->glyph        = d_StringInit();
  graph->description  = d_StringInit();
  graph->image_path   = d_StringInit();
  graph->combat_bark  = d_StringInit();
  graph->idle_bark    = d_StringInit();
  graph->idle_log     = d_StringInit();
  graph->action_label = d_StringInit();
}

void ne_GraphDestroy( NEGraph_t* graph )
{
  for ( int i = 0; i < graph->num_nodes; i++ )
    ne_NodeDestroy( &graph->nodes[i] );

  d_StringDestroy( graph->npc_key );
  d_StringDestroy( graph->display_name );
  d_StringDestroy( graph->glyph );
  d_StringDestroy( graph->description );
  d_StringDestroy( graph->image_path );
  d_StringDestroy( graph->combat_bark );
  d_StringDestroy( graph->idle_bark );
  d_StringDestroy( graph->idle_log );
  d_StringDestroy( graph->action_label );
}

void ne_FileEntryInit( NEFileEntry_t* fe )
{
  memset( fe, 0, sizeof( NEFileEntry_t ) );
  fe->filename     = d_StringInit();
  fe->stem         = d_StringInit();
  fe->display_name = d_StringInit();
  fe->folder       = d_StringInit();
}

void ne_FileEntryDestroy( NEFileEntry_t* fe )
{
  d_StringDestroy( fe->filename );
  d_StringDestroy( fe->stem );
  d_StringDestroy( fe->display_name );
  d_StringDestroy( fe->folder );
}

/* ---- Helper: copy a DUF string value into a dString_t ---- */

static void copy_dstr( dString_t* dst, dDUFValue_t* node )
{
  if ( node && node->value_string )
    d_StringSet( dst, node->value_string );
}

/* ---- Helper: parse a string array from DUF into dString_t[] ---- */

static int parse_str_array( dDUFValue_t* arr,
                             dString_t* out[], int max )
{
  if ( !arr || arr->type != D_DUF_ARRAY ) return 0;
  int n = 0;
  for ( dDUFValue_t* ch = arr->child; ch && n < max; ch = ch->next )
  {
    if ( ch->value_string )
      d_StringSet( out[n], ch->value_string );
    n++;
  }
  return n;
}

/* ---- Scan a directory (recursively) for .duf files ---- */

static const char* g_scan_basedir;

static void scan_dir( const char* dirpath )
{
  DIR* dir = opendir( dirpath );
  if ( !dir )
  {
    printf( "NPC Editor: could not open dir '%s'\n", dirpath );
    return;
  }
  printf( "NPC Editor: scanning '%s'\n", dirpath );

  struct dirent* ent;
  while ( ( ent = readdir( dir ) ) != NULL )
  {
    const char* name = ent->d_name;
    if ( name[0] == '.' ) continue;

    char path[512];
    snprintf( path, sizeof( path ), "%s/%s", dirpath, name );

    struct stat st;
    if ( stat( path, &st ) == 0 && S_ISDIR( st.st_mode ) )
    {
      scan_dir( path );
      continue;
    }

    int len = (int)strlen( name );
    if ( len < 5 || strcmp( name + len - 4, ".duf" ) != 0 ) continue;
    if ( g_ne_num_files >= NE_MAX_NPC_FILES ) continue;

    NEFileEntry_t* fe = &g_ne_files[g_ne_num_files];
    ne_FileEntryInit( fe );

    d_StringSet( fe->filename, path );

    /* Stem = filename without .duf extension */
    char stem_buf[256];
    int slen = len - 4;
    if ( slen >= (int)sizeof( stem_buf ) ) slen = (int)sizeof( stem_buf ) - 1;
    memcpy( stem_buf, name, slen );
    stem_buf[slen] = '\0';
    d_StringSet( fe->stem, stem_buf );

    /* Extract relative folder name from basedir */
    int baselen = (int)strlen( g_scan_basedir );
    if ( (int)strlen( dirpath ) > baselen + 1 )
      d_StringSet( fe->folder, dirpath + baselen + 1 );
    else
      d_StringSet( fe->folder, "" );

    /* Quick parse to grab display name */
    dDUFValue_t* root = NULL;
    dDUFError_t* err = d_DUFParseFile( path, &root );
    if ( err )
    {
      d_DUFErrorFree( err );
      /* still add file, just no display name */
      g_ne_num_files++;
      continue;
    }

    for ( dDUFValue_t* entry = root->child; entry; entry = entry->next )
    {
      if ( entry->key && strcmp( entry->key, "npc" ) == 0 )
      {
        dDUFValue_t* nm = d_DUFGetObjectItem( entry, "name" );
        if ( nm && nm->value_string )
          d_StringSet( fe->display_name, nm->value_string );
        break;
      }
    }

    d_DUFFree( root );
    g_ne_num_files++;
  }

  closedir( dir );
}

/* Compare file entries: folder first (root last), then stem within folder */
static int cmp_file_entry( const void* a, const void* b )
{
  const NEFileEntry_t* fa = (const NEFileEntry_t*)a;
  const NEFileEntry_t* fb = (const NEFileEntry_t*)b;

  const char* da = d_StringPeek( fa->folder );
  const char* db = d_StringPeek( fb->folder );
  int la = d_StringGetLength( fa->folder );
  int lb = d_StringGetLength( fb->folder );

  /* Root files (empty folder) sort last */
  if ( la == 0 && lb > 0 ) return 1;
  if ( la > 0 && lb == 0 ) return -1;

  int fc = strcmp( da, db );
  if ( fc != 0 ) return fc;

  /* Within same folder, sort by stem */
  return strcmp( d_StringPeek( fa->stem ), d_StringPeek( fb->stem ) );
}

void ne_ScanNPCFiles( const char* basedir )
{
  g_ne_num_files = 0;
  memset( g_ne_files, 0, sizeof( g_ne_files ) );
  g_scan_basedir = basedir;
  scan_dir( basedir );

  /* Sort by folder then stem */
  if ( g_ne_num_files > 1 )
    qsort( g_ne_files, g_ne_num_files, sizeof( NEFileEntry_t ),
           cmp_file_entry );

  printf( "NPC Editor: found %d NPC files.\n", g_ne_num_files );
}

/* ---- Load a full NPC file into g_ne_graph ---- */

int ne_LoadNPCFile( const char* path, const char* stem )
{
  if ( g_ne_graph_loaded )
    ne_GraphDestroy( &g_ne_graph );

  ne_GraphInit( &g_ne_graph );
  g_ne_graph_loaded  = 0;
  g_ne_selected_node = -1;

  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( path, &root );
  if ( err )
  {
    printf( "NPC Editor: parse error in %s at %d:%d - %s\n",
            path, err->line, err->column,
            d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return 0;
  }

  d_StringSet( g_ne_graph.npc_key, stem );

  for ( dDUFValue_t* entry = root->child; entry; entry = entry->next )
  {
    if ( !entry->key ) continue;

    /* @npc block - metadata */
    if ( strcmp( entry->key, "npc" ) == 0 )
    {
      copy_dstr( g_ne_graph.display_name,
                 d_DUFGetObjectItem( entry, "name" ) );
      copy_dstr( g_ne_graph.glyph,
                 d_DUFGetObjectItem( entry, "glyph" ) );
      copy_dstr( g_ne_graph.description,
                 d_DUFGetObjectItem( entry, "description" ) );
      copy_dstr( g_ne_graph.image_path,
                 d_DUFGetObjectItem( entry, "image_path" ) );
      copy_dstr( g_ne_graph.combat_bark,
                 d_DUFGetObjectItem( entry, "combat_bark" ) );
      copy_dstr( g_ne_graph.idle_bark,
                 d_DUFGetObjectItem( entry, "idle_bark" ) );
      copy_dstr( g_ne_graph.idle_log,
                 d_DUFGetObjectItem( entry, "idle_log" ) );
      copy_dstr( g_ne_graph.action_label,
                 d_DUFGetObjectItem( entry, "action" ) );

      /* Color array */
      dDUFValue_t* col = d_DUFGetObjectItem( entry, "color" );
      if ( col && col->type == D_DUF_ARRAY )
      {
        dDUFValue_t* ch = col->child;
        if ( ch ) { g_ne_graph.color[0] = (uint8_t)ch->value_int; ch = ch->next; }
        if ( ch ) { g_ne_graph.color[1] = (uint8_t)ch->value_int; ch = ch->next; }
        if ( ch ) { g_ne_graph.color[2] = (uint8_t)ch->value_int; ch = ch->next; }
        if ( ch ) { g_ne_graph.color[3] = (uint8_t)ch->value_int; }
      }

      dDUFValue_t* v;
      v = d_DUFGetObjectItem( entry, "idle_cooldown" );
      if ( v ) g_ne_graph.idle_cooldown = (int)v->value_int;
      v = d_DUFGetObjectItem( entry, "combat" );
      if ( v ) g_ne_graph.combat = (int)v->value_int;
      v = d_DUFGetObjectItem( entry, "damage" );
      if ( v ) g_ne_graph.damage = (int)v->value_int;
      v = d_DUFGetObjectItem( entry, "no_face" );
      if ( v ) g_ne_graph.no_face = (int)v->value_int;
      v = d_DUFGetObjectItem( entry, "no_shadow" );
      if ( v ) g_ne_graph.no_shadow = (int)v->value_int;

      continue;
    }

    /* Dialogue nodes */
    if ( g_ne_graph.num_nodes >= NE_MAX_NODES )
    {
      printf( "NPC Editor: '%s' exceeds %d nodes, skipping '%s'\n",
              stem, NE_MAX_NODES, entry->key );
      continue;
    }

    NENode_t* node = &g_ne_graph.nodes[g_ne_graph.num_nodes];
    ne_NodeInit( node );

    d_StringSet( node->key, entry->key );

    /* Speech fields */
    copy_dstr( node->text, d_DUFGetObjectItem( entry, "text" ) );
    node->num_options = parse_str_array(
      d_DUFGetObjectItem( entry, "options" ),
      node->option_keys, NE_MAX_OPTIONS );

    /* Option fields */
    copy_dstr( node->label,    d_DUFGetObjectItem( entry, "label" ) );
    copy_dstr( node->goto_key, d_DUFGetObjectItem( entry, "goto" ) );

    /* Color name */
    dDUFValue_t* color_v = d_DUFGetObjectItem( entry, "color" );
    if ( color_v && color_v->value_string )
      d_StringSet( node->color_name, color_v->value_string );

    /* Start / priority */
    dDUFValue_t* start_v = d_DUFGetObjectItem( entry, "start" );
    if ( start_v ) node->is_start = 1;
    dDUFValue_t* prio_v = d_DUFGetObjectItem( entry, "priority" );
    if ( prio_v ) node->priority = (int)prio_v->value_int;

    /* Single-value conditions */
    copy_dstr( node->require_class,
               d_DUFGetObjectItem( entry, "require_class" ) );
    copy_dstr( node->require_flag_min,
               d_DUFGetObjectItem( entry, "require_flag_min" ) );
    copy_dstr( node->require_item,
               d_DUFGetObjectItem( entry, "require_item" ) );

    /* Multi-value conditions (iterate children for duplicates) */
    for ( dDUFValue_t* ch = entry->child; ch; ch = ch->next )
    {
      if ( !ch->key ) continue;

      if ( strcmp( ch->key, "require_flag" ) == 0 )
      {
        if ( ch->value_string && node->num_require_flag < NE_MAX_CONDITIONS )
        {
          d_StringSet( node->require_flag[node->num_require_flag],
                       ch->value_string );
          node->num_require_flag++;
        }
      }
      if ( strcmp( ch->key, "require_not_flag" ) == 0 )
      {
        if ( ch->value_string
             && node->num_require_not_flag < NE_MAX_CONDITIONS )
        {
          d_StringSet( node->require_not_flag[node->num_require_not_flag],
                       ch->value_string );
          node->num_require_not_flag++;
        }
      }
      if ( strcmp( ch->key, "require_lore" ) == 0 )
      {
        if ( ch->value_string && node->num_require_lore < NE_MAX_CONDITIONS )
        {
          d_StringSet( node->require_lore[node->num_require_lore],
                       ch->value_string );
          node->num_require_lore++;
        }
      }
      if ( strcmp( ch->key, "require_not_lore" ) == 0 )
      {
        if ( ch->value_string
             && node->num_require_not_lore < NE_MAX_CONDITIONS )
        {
          d_StringSet( node->require_not_lore[node->num_require_not_lore],
                       ch->value_string );
          node->num_require_not_lore++;
        }
      }
    }

    /* Actions */
    copy_dstr( node->set_flag,   d_DUFGetObjectItem( entry, "set_flag" ) );
    copy_dstr( node->incr_flag,  d_DUFGetObjectItem( entry, "incr_flag" ) );
    copy_dstr( node->clear_flag, d_DUFGetObjectItem( entry, "clear_flag" ) );
    copy_dstr( node->give_item,  d_DUFGetObjectItem( entry, "give_item" ) );
    copy_dstr( node->take_item,  d_DUFGetObjectItem( entry, "take_item" ) );
    copy_dstr( node->set_lore,   d_DUFGetObjectItem( entry, "set_lore" ) );
    copy_dstr( node->action,     d_DUFGetObjectItem( entry, "action" ) );

    dDUFValue_t* gg = d_DUFGetObjectItem( entry, "give_gold" );
    if ( gg ) node->give_gold = (int)gg->value_int;

    dDUFValue_t* rgm = d_DUFGetObjectItem( entry, "require_gold_min" );
    if ( rgm ) node->require_gold_min = (int)rgm->value_int;

    g_ne_graph.num_nodes++;
  }

  d_DUFFree( root );
  g_ne_graph_loaded = 1;
  g_ne_graph.dirty  = 0;

  /* Auto-layout if no positions saved */
  ne_AutoLayout();
  ne_GraphReset();

  printf( "NPC Editor: loaded '%s' (%s) - %d nodes\n",
          d_StringPeek( g_ne_graph.display_name ), stem,
          g_ne_graph.num_nodes );

  return 1;
}
