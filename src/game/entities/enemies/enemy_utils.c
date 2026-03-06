#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "enemies.h"

EnemyType_t g_enemy_types[MAX_ENEMY_TYPES];
int         g_num_enemy_types = 0;

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

static void EnemiesLoadFile( const char* path )
{
  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( path, &root );

  if ( err != NULL )
  {
    printf( "DUF parse error at %d:%d - %s\n",
            err->line, err->column, d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return;
  }

  for ( dDUFValue_t* entry = root->child;
        entry != NULL && g_num_enemy_types < MAX_ENEMY_TYPES;
        entry = entry->next )
  {
    EnemyType_t* t = &g_enemy_types[g_num_enemy_types];
    memset( t, 0, sizeof( EnemyType_t ) );

    if ( entry->key )
      strncpy( t->key, entry->key, MAX_NAME_LENGTH - 1 );

    dDUFValue_t* name     = d_DUFGetObjectItem( entry, "name" );
    dDUFValue_t* glyph    = d_DUFGetObjectItem( entry, "glyph" );
    dDUFValue_t* hp       = d_DUFGetObjectItem( entry, "hp" );
    dDUFValue_t* dmg      = d_DUFGetObjectItem( entry, "damage" );
    dDUFValue_t* def      = d_DUFGetObjectItem( entry, "defense" );
    dDUFValue_t* ai       = d_DUFGetObjectItem( entry, "ai" );
    dDUFValue_t* desc     = d_DUFGetObjectItem( entry, "description" );
    dDUFValue_t* range     = d_DUFGetObjectItem( entry, "range" );
    dDUFValue_t* drop_item = d_DUFGetObjectItem( entry, "drop_item" );
    dDUFValue_t* gold_drop = d_DUFGetObjectItem( entry, "gold_drop" );
    dDUFValue_t* color     = d_DUFGetObjectItem( entry, "color" );
    dDUFValue_t* img_path  = d_DUFGetObjectItem( entry, "image_path" );

    if ( name )      strncpy( t->name, name->value_string, MAX_NAME_LENGTH - 1 );
    if ( glyph )     strncpy( t->glyph, glyph->value_string, 7 );
    if ( hp )        t->hp      = (int)hp->value_int;
    if ( dmg )       t->damage  = (int)dmg->value_int;
    if ( def )       t->defense = (int)def->value_int;
    if ( ai )        strncpy( t->ai, ai->value_string, MAX_NAME_LENGTH - 1 );
    if ( desc )      strncpy( t->description, desc->value_string, 255 );
    if ( range )     t->range = (int)range->value_int;

    dDUFValue_t* sight = d_DUFGetObjectItem( entry, "sight_range" );
    t->sight_range = sight ? (int)sight->value_int : 6;
    if ( drop_item )
      strncpy( t->drop_item, drop_item->value_string, MAX_NAME_LENGTH - 1 );
    if ( gold_drop ) t->gold_drop = (int)gold_drop->value_int;

    dDUFValue_t* on_death_v      = d_DUFGetObjectItem( entry, "on_death" );
    dDUFValue_t* pool_duration_v = d_DUFGetObjectItem( entry, "pool_duration" );
    dDUFValue_t* pool_damage_v   = d_DUFGetObjectItem( entry, "pool_damage" );
    if ( on_death_v )      strncpy( t->on_death, on_death_v->value_string, MAX_NAME_LENGTH - 1 );
    if ( pool_duration_v ) t->pool_duration = (int)pool_duration_v->value_int;
    if ( pool_damage_v )   t->pool_damage   = (int)pool_damage_v->value_int;

    t->color = ParseDUFColor( color );

    if ( img_path && strlen( img_path->value_string ) > 0 )
    {
      if ( access( img_path->value_string, F_OK ) != 0 )
      {
        fprintf( stderr, "FATAL: missing image '%s' for enemy '%s'\n",
                 img_path->value_string, t->name );
        exit( 1 );
      }
      t->image = a_ImageLoad( img_path->value_string );
    }

    g_num_enemy_types++;
  }

  d_DUFFree( root );
}

static void enemies_scan_dir( const char* dirpath )
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

    struct stat st;
    if ( stat( path, &st ) == 0 && S_ISDIR( st.st_mode ) )
    {
      enemies_scan_dir( path );
      continue;
    }

    int len = (int)strlen( name );
    if ( len < 5 || strcmp( name + len - 4, ".duf" ) != 0 ) continue;

    EnemiesLoadFile( path );
  }

  closedir( dir );
}

void EnemiesLoadTypes( void )
{
  memset( g_enemy_types, 0, sizeof( g_enemy_types ) );
  g_num_enemy_types = 0;
  enemies_scan_dir( "resources/data/enemies" );
  printf( "Loaded %d enemy types.\n", g_num_enemy_types );
}

int EnemyTypeByKey( const char* key )
{
  for ( int i = 0; i < g_num_enemy_types; i++ )
  {
    if ( strcmp( g_enemy_types[i].key, key ) == 0 )
      return i;
  }
  return -1;
}

void EnemiesInit( Enemy_t* list, int* count )
{
  memset( list, 0, sizeof( Enemy_t ) * MAX_ENEMIES );
  *count = 0;
}

Enemy_t* EnemySpawn( Enemy_t* list, int* count,
                     int type_idx, int row, int col,
                     int tile_w, int tile_h )
{
  if ( *count >= MAX_ENEMIES || type_idx < 0 || type_idx >= g_num_enemy_types )
    return NULL;

  Enemy_t* e = &list[*count];
  e->type_idx = type_idx;
  e->row      = row;
  e->col      = col;
  e->world_x  = row * tile_w + tile_w / 2.0f;
  e->world_y  = col * tile_h + tile_h / 2.0f;
  e->hp             = g_enemy_types[type_idx].hp;
  e->alive          = 1;
  e->turns_since_hit = 99;
  e->chase_turns     = 0;
  e->last_seen_row   = -1;
  e->last_seen_col   = -1;
  e->ai_state        = 0;
  e->ai_dir_row      = 0;
  e->ai_dir_col      = 0;
  ( *count )++;
  return e;
}

Enemy_t* EnemyAt( Enemy_t* list, int count, int row, int col )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( list[i].alive && list[i].row == row && list[i].col == col )
      return &list[i];
  }
  return NULL;
}

Enemy_t* EnemyMobileAt( Enemy_t* list, int count, int row, int col )
{
  for ( int i = 0; i < count; i++ )
  {
    if ( list[i].alive && list[i].row == row && list[i].col == col
         && strcmp( g_enemy_types[list[i].type_idx].ai, "static" ) != 0 )
      return &list[i];
  }
  return NULL;
}

int EnemiesInCombat( Enemy_t* list, int count )
{
  for ( int i = 0; i < count; i++ )
    if ( list[i].alive && list[i].chase_turns > 0 ) return 1;
  return 0;
}
