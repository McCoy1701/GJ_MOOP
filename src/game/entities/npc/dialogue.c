#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "defines.h"
#include "dialogue.h"
#include "items.h"
#include "lore.h"
#include "bank.h"
#include "npc_relocate.h"

extern Player_t player;

/* ---- NPC type registry ---- */

NPCType_t g_npc_types[MAX_NPC_TYPES];
int       g_num_npc_types = 0;

/* ---- Flag system ---- */

typedef struct { dString_t* name; int value; } Flag_t;

static Flag_t g_flags[MAX_FLAGS];
static int    g_num_flags = 0;

static int flag_find( const char* name )
{
  for ( int i = 0; i < g_num_flags; i++ )
    if ( d_StringCompareToCString( g_flags[i].name, name ) == 0 ) return i;
  return -1;
}

void FlagsInit( void )
{
  for ( int i = 0; i < g_num_flags; i++ )
    d_StringDestroy( g_flags[i].name );
  memset( g_flags, 0, sizeof( g_flags ) );
  g_num_flags = 0;
}

int FlagGet( const char* name )
{
  int i = flag_find( name );
  return ( i >= 0 ) ? g_flags[i].value : 0;
}

void FlagSet( const char* name, int value )
{
  int i = flag_find( name );
  if ( i >= 0 ) { g_flags[i].value = value; return; }
  if ( g_num_flags >= MAX_FLAGS ) return;
  g_flags[g_num_flags].name = d_StringInit();
  d_StringSet( g_flags[g_num_flags].name, name );
  g_flags[g_num_flags].value = value;
  g_num_flags++;
}

void FlagIncr( const char* name )
{
  FlagSet( name, FlagGet( name ) + 1 );
}

void FlagClear( const char* name )
{
  int i = flag_find( name );
  if ( i < 0 ) return;
  d_StringDestroy( g_flags[i].name );
  g_flags[i] = g_flags[g_num_flags - 1];
  memset( &g_flags[g_num_flags - 1], 0, sizeof( Flag_t ) );
  g_num_flags--;
}

/* ---- Init / destroy helpers ---- */

void DialogueEntryInit( DialogueEntry_t* de )
{
  memset( de, 0, sizeof( DialogueEntry_t ) );
  de->key              = d_StringInit();
  de->text             = d_StringInit();
  de->label            = d_StringInit();
  de->goto_key         = d_StringInit();
  de->require_class    = d_StringInit();
  de->require_flag_min = d_StringInit();
  de->require_item     = d_StringInit();
  de->set_flag         = d_StringInit();
  de->incr_flag        = d_StringInit();
  de->clear_flag       = d_StringInit();
  de->give_item        = d_StringInit();
  de->take_item        = d_StringInit();
  de->set_lore         = d_StringInit();
  de->action           = d_StringInit();

  for ( int i = 0; i < MAX_NODE_OPTIONS; i++ )
    de->option_keys[i] = d_StringInit();
  for ( int i = 0; i < MAX_CONDITIONS; i++ )
  {
    de->require_flag[i]     = d_StringInit();
    de->require_not_flag[i] = d_StringInit();
    de->require_lore[i]     = d_StringInit();
  }
}

void DialogueEntryDestroy( DialogueEntry_t* de )
{
  d_StringDestroy( de->key );
  d_StringDestroy( de->text );
  d_StringDestroy( de->label );
  d_StringDestroy( de->goto_key );
  d_StringDestroy( de->require_class );
  d_StringDestroy( de->require_flag_min );
  d_StringDestroy( de->require_item );
  d_StringDestroy( de->set_flag );
  d_StringDestroy( de->incr_flag );
  d_StringDestroy( de->clear_flag );
  d_StringDestroy( de->give_item );
  d_StringDestroy( de->take_item );
  d_StringDestroy( de->set_lore );
  d_StringDestroy( de->action );

  for ( int i = 0; i < MAX_NODE_OPTIONS; i++ )
    d_StringDestroy( de->option_keys[i] );
  for ( int i = 0; i < MAX_CONDITIONS; i++ )
  {
    d_StringDestroy( de->require_flag[i] );
    d_StringDestroy( de->require_not_flag[i] );
    d_StringDestroy( de->require_lore[i] );
  }
}

void NPCTypeInit( NPCType_t* npc )
{
  memset( npc, 0, sizeof( NPCType_t ) );
  npc->key         = d_StringInit();
  npc->name        = d_StringInit();
  npc->glyph       = d_StringInit();
  npc->description = d_StringInit();
  npc->combat_bark = d_StringInit();
  npc->idle_bark    = d_StringInit();
  npc->idle_log     = d_StringInit();
  npc->action_label = d_StringInit();
  d_StringSet( npc->action_label, "Talk" );
}

void NPCTypeDestroy( NPCType_t* npc )
{
  for ( int i = 0; i < npc->num_entries; i++ )
    DialogueEntryDestroy( &npc->entries[i] );

  d_StringDestroy( npc->key );
  d_StringDestroy( npc->name );
  d_StringDestroy( npc->glyph );
  d_StringDestroy( npc->description );
  d_StringDestroy( npc->combat_bark );
  d_StringDestroy( npc->idle_bark );
  d_StringDestroy( npc->idle_log );
  d_StringDestroy( npc->action_label );
}

void DialogueDestroyAll( void )
{
  for ( int i = 0; i < g_num_npc_types; i++ )
    NPCTypeDestroy( &g_npc_types[i] );
  memset( g_npc_types, 0, sizeof( g_npc_types ) );
  g_num_npc_types = 0;
}

/* ---- DUF color parsing (same as enemies/items) ---- */

static aColor_t ParseDUFColor( dDUFValue_t* color_node )
{
  aColor_t c = { 255, 255, 255, 255 };
  if ( color_node == NULL || color_node->type != D_DUF_ARRAY ) return c;

  dDUFValue_t* ch = color_node->child;
  if ( ch ) { c.r = (uint8_t)ch->value_int; ch = ch->next; }
  if ( ch ) { c.g = (uint8_t)ch->value_int; ch = ch->next; }
  if ( ch ) { c.b = (uint8_t)ch->value_int; ch = ch->next; }
  if ( ch ) { c.a = (uint8_t)ch->value_int; }
  return c;
}

/* ---- Parse DUF string arrays (for options list) ---- */

static int ParseStringArray( dDUFValue_t* arr,
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

/* ---- Helper: copy DUF value into dString_t ---- */

static void copy_dstr( dString_t* dst, dDUFValue_t* node )
{
  if ( node && node->value_string )
    d_StringSet( dst, node->value_string );
}

/* ---- Load one NPC dialogue file ---- */

static void DialogueLoadFile( const char* path, const char* stem )
{
  if ( g_num_npc_types >= MAX_NPC_TYPES ) return;

  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( path, &root );

  if ( err != NULL )
  {
    printf( "DUF parse error in %s at %d:%d - %s\n",
            path, err->line, err->column, d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return;
  }

  NPCType_t* npc = &g_npc_types[g_num_npc_types];
  NPCTypeInit( npc );
  d_StringSet( npc->key, stem );
  d_StringSet( npc->combat_bark, "Can't talk right now!" );

  for ( dDUFValue_t* entry = root->child; entry != NULL; entry = entry->next )
  {
    if ( !entry->key ) continue;

    /* @npc block - metadata */
    if ( strcmp( entry->key, "npc" ) == 0 )
    {
      copy_dstr( npc->name,  d_DUFGetObjectItem( entry, "name" ) );
      copy_dstr( npc->glyph, d_DUFGetObjectItem( entry, "glyph" ) );
      npc->color = ParseDUFColor( d_DUFGetObjectItem( entry, "color" ) );
      copy_dstr( npc->description, d_DUFGetObjectItem( entry, "description" ) );
      copy_dstr( npc->combat_bark, d_DUFGetObjectItem( entry, "combat_bark" ) );
      copy_dstr( npc->idle_bark,    d_DUFGetObjectItem( entry, "idle_bark" ) );
      copy_dstr( npc->idle_log,    d_DUFGetObjectItem( entry, "idle_log" ) );
      copy_dstr( npc->action_label, d_DUFGetObjectItem( entry, "action" ) );

      dDUFValue_t* idle_cd = d_DUFGetObjectItem( entry, "idle_cooldown" );
      if ( idle_cd ) npc->idle_cooldown = (int)idle_cd->value_int;

      dDUFValue_t* combat_v = d_DUFGetObjectItem( entry, "combat" );
      if ( combat_v ) npc->combat = (int)combat_v->value_int;

      dDUFValue_t* damage_v = d_DUFGetObjectItem( entry, "damage" );
      if ( damage_v ) npc->damage = (int)damage_v->value_int;

      dDUFValue_t* img_path = d_DUFGetObjectItem( entry, "image_path" );
      if ( img_path && img_path->value_string && strlen( img_path->value_string ) > 0 )
      {
        struct stat img_st;
        if ( stat( img_path->value_string, &img_st ) == 0 )
          npc->image = a_ImageLoad( img_path->value_string );
        else
          printf( "NPC '%s': image not found: %s\n", stem, img_path->value_string );
      }

      continue;
    }

    /* Dialogue entry - speech or option node */
    if ( npc->num_entries >= MAX_DIALOGUE_NODES )
    {
      printf( "DIALOGUE: '%s' exceeds %d nodes, '%s' dropped!\n",
              d_StringPeek( npc->key ), MAX_DIALOGUE_NODES, entry->key );
      continue;
    }

    DialogueEntry_t* de = &npc->entries[npc->num_entries];
    DialogueEntryInit( de );
    d_StringSet( de->key, entry->key );

    /* Speech node fields */
    copy_dstr( de->text, d_DUFGetObjectItem( entry, "text" ) );
    de->num_options = ParseStringArray(
      d_DUFGetObjectItem( entry, "options" ),
      de->option_keys, MAX_NODE_OPTIONS );

    /* Option node fields */
    copy_dstr( de->label,    d_DUFGetObjectItem( entry, "label" ) );
    copy_dstr( de->goto_key, d_DUFGetObjectItem( entry, "goto" ) );

    /* Label color - named preset or RGBA array (alpha 0 = not set) */
    {
      dDUFValue_t* col = d_DUFGetObjectItem( entry, "color" );
      if ( col && col->value_string )
      {
        if      ( strcmp( col->value_string, "quest" )    == 0 )
          de->label_color = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
        else if ( strcmp( col->value_string, "complete" ) == 0 )
          de->label_color = (aColor_t){ 0x75, 0xa7, 0x43, 255 };
        else if ( strcmp( col->value_string, "danger" )   == 0 )
          de->label_color = (aColor_t){ 0xa5, 0x30, 0x30, 255 };
        else if ( strcmp( col->value_string, "lore" )     == 0 )
          de->label_color = (aColor_t){ 0x63, 0xc7, 0xb2, 255 };
      }
      else if ( col && col->type == D_DUF_ARRAY )
        de->label_color = ParseDUFColor( col );
    }

    /* Start / priority */
    dDUFValue_t* start = d_DUFGetObjectItem( entry, "start" );
    if ( start ) de->is_start = 1;
    dDUFValue_t* prio = d_DUFGetObjectItem( entry, "priority" );
    if ( prio ) de->priority = (int)prio->value_int;

    /* Conditions - iterate children to collect ALL matching keys */
    copy_dstr( de->require_class,    d_DUFGetObjectItem( entry, "require_class" ) );
    copy_dstr( de->require_flag_min, d_DUFGetObjectItem( entry, "require_flag_min" ) );
    copy_dstr( de->require_item,     d_DUFGetObjectItem( entry, "require_item" ) );

    de->num_require_flag = 0;
    de->num_require_not_flag = 0;
    de->num_require_lore = 0;
    for ( dDUFValue_t* ch = entry->child; ch; ch = ch->next )
    {
      if ( !ch->key ) continue;
      if ( strcmp( ch->key, "require_flag" ) == 0 )
      {
        if ( ch->value_string && de->num_require_flag < MAX_CONDITIONS )
          d_StringSet( de->require_flag[de->num_require_flag++],
                       ch->value_string );
        else if ( ch->type == D_DUF_ARRAY )
          for ( dDUFValue_t* a = ch->child; a && de->num_require_flag < MAX_CONDITIONS; a = a->next )
            if ( a->value_string )
              d_StringSet( de->require_flag[de->num_require_flag++],
                           a->value_string );
      }
      if ( strcmp( ch->key, "require_not_flag" ) == 0 )
      {
        if ( ch->value_string && de->num_require_not_flag < MAX_CONDITIONS )
          d_StringSet( de->require_not_flag[de->num_require_not_flag++],
                       ch->value_string );
        else if ( ch->type == D_DUF_ARRAY )
          for ( dDUFValue_t* a = ch->child; a && de->num_require_not_flag < MAX_CONDITIONS; a = a->next )
            if ( a->value_string )
              d_StringSet( de->require_not_flag[de->num_require_not_flag++],
                           a->value_string );
      }
      if ( strcmp( ch->key, "require_lore" ) == 0 )
      {
        if ( ch->value_string && de->num_require_lore < MAX_CONDITIONS )
          d_StringSet( de->require_lore[de->num_require_lore++],
                       ch->value_string );
        else if ( ch->type == D_DUF_ARRAY )
          for ( dDUFValue_t* a = ch->child; a && de->num_require_lore < MAX_CONDITIONS; a = a->next )
            if ( a->value_string )
              d_StringSet( de->require_lore[de->num_require_lore++],
                           a->value_string );
      }
    }

    /* Actions */
    copy_dstr( de->set_flag,   d_DUFGetObjectItem( entry, "set_flag" ) );
    copy_dstr( de->incr_flag,  d_DUFGetObjectItem( entry, "incr_flag" ) );
    copy_dstr( de->clear_flag, d_DUFGetObjectItem( entry, "clear_flag" ) );
    copy_dstr( de->give_item,  d_DUFGetObjectItem( entry, "give_item" ) );
    copy_dstr( de->take_item,  d_DUFGetObjectItem( entry, "take_item" ) );
    copy_dstr( de->set_lore,   d_DUFGetObjectItem( entry, "set_lore" ) );
    copy_dstr( de->action,    d_DUFGetObjectItem( entry, "action" ) );

    { dDUFValue_t* v = d_DUFGetObjectItem( entry, "give_gold" );
      if ( v ) de->give_gold = (int)v->value_int; }
    { dDUFValue_t* v = d_DUFGetObjectItem( entry, "require_gold_min" );
      if ( v ) de->require_gold_min = (int)v->value_int; }

    npc->num_entries++;
  }

  d_DUFFree( root );
  g_num_npc_types++;
}

/* ---- Scan a directory for .duf files, recurse into subdirs ---- */

static void dialogue_scan_dir( const char* dirpath )
{
  DIR* dir = opendir( dirpath );
  if ( !dir ) return;

  struct dirent* ent;
  while ( ( ent = readdir( dir ) ) != NULL )
  {
    const char* name = ent->d_name;
    if ( name[0] == '.' ) continue;

    char path[512];
    snprintf( path, sizeof( path ), "%s/%s", dirpath, name );

    /* Recurse into subdirectories */
    struct stat st;
    if ( stat( path, &st ) == 0 && S_ISDIR( st.st_mode ) )
    {
      dialogue_scan_dir( path );
      continue;
    }

    int len = (int)strlen( name );
    if ( len < 5 || strcmp( name + len - 4, ".duf" ) != 0 ) continue;

    /* Extract stem (filename without .duf) */
    char stem[MAX_NAME_LENGTH] = { 0 };
    int slen = len - 4;
    if ( slen >= MAX_NAME_LENGTH ) slen = MAX_NAME_LENGTH - 1;
    strncpy( stem, name, slen );

    DialogueLoadFile( path, stem );
  }

  closedir( dir );
}

void DialogueLoadAll( void )
{
  DialogueDestroyAll();
  dialogue_scan_dir( "resources/data/npcs" );
  printf( "Loaded %d NPC dialogue files.\n", g_num_npc_types );
}

int NPCTypeByKey( const char* key )
{
  for ( int i = 0; i < g_num_npc_types; i++ )
    if ( d_StringCompareToCString( g_npc_types[i].key, key ) == 0 ) return i;
  return -1;
}

/* ---- Dialogue runtime ---- */

static int  dlg_active   = 0;
static int  dlg_npc_type = -1;
static int  dlg_node_idx = -1;     /* current speech node index in entries[] */

/* Filtered option indices visible to the player */
static int  dlg_visible_opts[MAX_NODE_OPTIONS];
static int  dlg_num_visible = 0;

/* Speech text override (set by actions, cleared each selection) */
static char dlg_text_override[256] = { 0 };

void DialogueOverrideText( const char* text )
{
  strncpy( dlg_text_override, text, sizeof( dlg_text_override ) - 1 );
  dlg_text_override[sizeof( dlg_text_override ) - 1] = '\0';
}

static DialogueEntry_t* find_entry( NPCType_t* npc, const char* key )
{
  for ( int i = 0; i < npc->num_entries; i++ )
    if ( d_StringCompareToCString( npc->entries[i].key, key ) == 0 )
      return &npc->entries[i];
  return NULL;
}

static int find_entry_idx( NPCType_t* npc, const char* key )
{
  for ( int i = 0; i < npc->num_entries; i++ )
    if ( d_StringCompareToCString( npc->entries[i].key, key ) == 0 ) return i;
  return -1;
}

/* ---- Condition checking ---- */

static int has_item( const char* name )
{
  for ( int i = 0; i < MAX_INVENTORY; i++ )
  {
    if ( player.inventory[i].type == INV_CONSUMABLE )
    {
      if ( strcmp( g_consumables[player.inventory[i].index].name, name ) == 0 )
        return 1;
    }
  }
  return 0;
}

static int check_conditions( DialogueEntry_t* de )
{
  /* require_class */
  if ( d_StringGetLength( de->require_class ) > 0 )
  {
    if ( strcasecmp( player.name, d_StringPeek( de->require_class ) ) != 0 )
      return 0;
  }

  /* require_flag: ALL listed flags must be > 0 */
  for ( int i = 0; i < de->num_require_flag; i++ )
  {
    if ( d_StringGetLength( de->require_flag[i] ) > 0
         && FlagGet( d_StringPeek( de->require_flag[i] ) ) <= 0 )
      return 0;
  }

  /* require_flag_min: "key:value" - flag >= value */
  if ( d_StringGetLength( de->require_flag_min ) > 0 )
  {
    char buf[256];
    strncpy( buf, d_StringPeek( de->require_flag_min ), sizeof( buf ) - 1 );
    buf[sizeof( buf ) - 1] = '\0';
    char* colon = strchr( buf, ':' );
    if ( colon )
    {
      *colon = '\0';
      int min_val = atoi( colon + 1 );
      if ( FlagGet( buf ) < min_val ) return 0;
    }
  }

  /* require_not_flag: ALL listed flags must be 0 or missing */
  for ( int i = 0; i < de->num_require_not_flag; i++ )
  {
    if ( d_StringGetLength( de->require_not_flag[i] ) > 0
         && FlagGet( d_StringPeek( de->require_not_flag[i] ) ) > 0 )
      return 0;
  }

  /* require_lore: ALL listed lore keys must be discovered */
  for ( int i = 0; i < de->num_require_lore; i++ )
  {
    if ( d_StringGetLength( de->require_lore[i] ) > 0
         && !LoreIsDiscovered( d_StringPeek( de->require_lore[i] ) ) )
      return 0;
  }

  /* require_item */
  if ( d_StringGetLength( de->require_item ) > 0 )
  {
    if ( !has_item( d_StringPeek( de->require_item ) ) ) return 0;
  }

  /* require_gold_min */
  if ( de->require_gold_min > 0 && player.gold < de->require_gold_min )
    return 0;

  return 1;
}

/* ---- Dispatch custom action string ---- */

static void DialogueDispatchAction( const char* a )
{
  if ( strcmp( a, "bank_deposit" ) == 0 )   { BankDeposit( 10 ); }
  if ( strcmp( a, "bank_withdraw" ) == 0 )  { BankWithdraw( 10 ); }
  if ( strcmp( a, "bank_check" ) == 0 )     { BankCheck(); }
  if ( strcmp( a, "bank_greeting" ) == 0 )  { BankGreeting(); }

  /* relocate:ROW,COL - fade-to-black NPC teleport */
  if ( strncmp( a, "relocate:", 9 ) == 0 )
  {
    int row = 0, col = 0;
    if ( sscanf( a + 9, "%d,%d", &row, &col ) == 2 )
    {
      NPCRelocate( dlg_npc_type, row, col, 1 );
      DialogueEnd();
    }
  }
}

/* ---- Execute actions on a node ---- */

static void execute_actions( DialogueEntry_t* de )
{
  if ( d_StringGetLength( de->set_flag ) > 0 )
  {
    char buf[256];
    strncpy( buf, d_StringPeek( de->set_flag ), sizeof( buf ) - 1 );
    buf[sizeof( buf ) - 1] = '\0';
    char* colon = strchr( buf, ':' );
    if ( colon )
    {
      *colon = '\0';
      FlagSet( buf, atoi( colon + 1 ) );
    }
    else
      FlagSet( buf, 1 );
  }

  if ( d_StringGetLength( de->incr_flag ) > 0 )
    FlagIncr( d_StringPeek( de->incr_flag ) );

  if ( d_StringGetLength( de->clear_flag ) > 0 )
    FlagClear( d_StringPeek( de->clear_flag ) );

  if ( d_StringGetLength( de->give_item ) > 0 )
  {
    for ( int i = 0; i < g_num_consumables; i++ )
    {
      if ( strcmp( g_consumables[i].name, d_StringPeek( de->give_item ) ) == 0 )
      { InventoryAdd( INV_CONSUMABLE, i ); break; }
    }
  }

  if ( d_StringGetLength( de->take_item ) > 0 )
  {
    for ( int i = 0; i < MAX_INVENTORY; i++ )
    {
      if ( player.inventory[i].type == INV_CONSUMABLE &&
           strcmp( g_consumables[player.inventory[i].index].name,
                   d_StringPeek( de->take_item ) ) == 0 )
      { InventoryRemove( i ); break; }
    }
  }

  if ( d_StringGetLength( de->set_lore ) > 0 )
    LoreUnlock( d_StringPeek( de->set_lore ) );

  if ( de->give_gold > 0 )
    PlayerAddGold( de->give_gold );

  if ( de->action && d_StringGetLength( de->action ) > 0 )
    DialogueDispatchAction( d_StringPeek( de->action ) );
}

/* ---- Build filtered options for current speech node ---- */

static void build_visible_options( void )
{
  dlg_num_visible = 0;
  if ( dlg_npc_type < 0 || dlg_node_idx < 0 ) return;

  NPCType_t* npc = &g_npc_types[dlg_npc_type];
  DialogueEntry_t* speech = &npc->entries[dlg_node_idx];

  for ( int i = 0; i < speech->num_options && dlg_num_visible < MAX_NODE_OPTIONS; i++ )
  {
    DialogueEntry_t* opt = find_entry( npc, d_StringPeek( speech->option_keys[i] ) );
    if ( !opt )
    {
      printf( "DIALOGUE: option '%s' not found in '%s'\n",
              d_StringPeek( speech->option_keys[i] ), d_StringPeek( npc->key ) );
      continue;
    }
    if ( !check_conditions( opt ) ) continue;
    dlg_visible_opts[dlg_num_visible++] = (int)( opt - npc->entries );
  }
}

/* ---- Public runtime API ---- */

void DialogueStart( int npc_type_idx )
{
  dlg_text_override[0] = '\0';
  if ( npc_type_idx < 0 || npc_type_idx >= g_num_npc_types ) return;

  NPCType_t* npc = &g_npc_types[npc_type_idx];
  dlg_npc_type = npc_type_idx;

  /* Find best start node: highest priority among passing conditions */
  int best_idx = -1;
  int best_pri = -1;

  for ( int i = 0; i < npc->num_entries; i++ )
  {
    DialogueEntry_t* de = &npc->entries[i];
    if ( !de->is_start ) continue;
    if ( !check_conditions( de ) ) continue;
    if ( de->priority > best_pri )
    {
      best_pri = de->priority;
      best_idx = i;
    }
  }

  if ( best_idx < 0 ) { dlg_active = 0; return; }

  dlg_node_idx = best_idx;
  dlg_active = 1;
  execute_actions( &npc->entries[dlg_node_idx] );
  build_visible_options();
}

void DialogueSelectOption( int index )
{
  dlg_text_override[0] = '\0';
  if ( !dlg_active || dlg_npc_type < 0 ) return;
  if ( index < 0 || index >= dlg_num_visible ) return;

  NPCType_t* npc = &g_npc_types[dlg_npc_type];
  DialogueEntry_t* opt = &npc->entries[dlg_visible_opts[index]];

  execute_actions( opt );

  /* Follow goto */
  if ( d_StringCompareToCString( opt->goto_key, "end" ) == 0 )
  { DialogueEnd(); return; }

  int next = find_entry_idx( npc, d_StringPeek( opt->goto_key ) );
  if ( next < 0 )
  {
    printf( "DIALOGUE: goto '%s' not found in '%s' (from option '%s')\n",
            d_StringPeek( opt->goto_key ), d_StringPeek( npc->key ),
            d_StringPeek( opt->key ) );
    DialogueEnd();
    return;
  }

  dlg_node_idx = next;
  execute_actions( &npc->entries[dlg_node_idx] );
  build_visible_options();

  /* If no options left, auto-close */
  if ( dlg_num_visible == 0 )
    DialogueEnd();
}

void DialogueEnd( void )
{
  dlg_active = 0;
  dlg_npc_type = -1;
  dlg_node_idx = -1;
  dlg_num_visible = 0;
}

int DialogueActive( void ) { return dlg_active; }

/* ---- State getters for UI ---- */

const char* DialogueGetNPCName( void )
{
  if ( dlg_npc_type < 0 ) return "";
  return d_StringPeek( g_npc_types[dlg_npc_type].name );
}

aColor_t DialogueGetNPCColor( void )
{
  if ( dlg_npc_type < 0 ) return (aColor_t){ 255, 255, 255, 255 };
  return g_npc_types[dlg_npc_type].color;
}

const char* DialogueGetText( void )
{
  if ( dlg_text_override[0] ) return dlg_text_override;
  if ( dlg_npc_type < 0 || dlg_node_idx < 0 ) return "";
  return d_StringPeek( g_npc_types[dlg_npc_type].entries[dlg_node_idx].text );
}

int DialogueGetOptionCount( void ) { return dlg_num_visible; }

const char* DialogueGetOptionLabel( int index )
{
  if ( index < 0 || index >= dlg_num_visible ) return "";
  NPCType_t* npc = &g_npc_types[dlg_npc_type];
  return d_StringPeek( npc->entries[dlg_visible_opts[index]].label );
}

aColor_t DialogueGetOptionColor( int index )
{
  aColor_t def = { 0, 0, 0, 0 };   /* alpha 0 = no override */
  if ( index < 0 || index >= dlg_num_visible ) return def;

  NPCType_t* npc = &g_npc_types[dlg_npc_type];
  DialogueEntry_t* opt = &npc->entries[dlg_visible_opts[index]];

  /* 1. Manual color from DUF */
  if ( opt->label_color.a > 0 ) return opt->label_color;

  /* 2. Auto-detect from actions */
  const char* sf = d_StringPeek( opt->set_flag );
  if ( sf[0] && strncmp( sf, "quest_", 6 ) == 0 )
    return (aColor_t){ 0xde, 0x9e, 0x41, 255 };   /* quest start - gold */

  const char* cf = d_StringPeek( opt->clear_flag );
  if ( cf[0] && strncmp( cf, "quest_", 6 ) == 0 )
    return (aColor_t){ 0x75, 0xa7, 0x43, 255 };   /* quest complete - green */

  return def;
}

aImage_t* DialogueGetNPCImage( void )
{
  if ( dlg_npc_type < 0 ) return NULL;
  return g_npc_types[dlg_npc_type].image;
}

const char* DialogueGetNPCGlyph( void )
{
  if ( dlg_npc_type < 0 ) return "";
  return d_StringPeek( g_npc_types[dlg_npc_type].glyph );
}
