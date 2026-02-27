#include <stdio.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "items.h"
#include "player.h"

ClassInfo_t      g_classes[3];
const char*      g_class_keys[3] = { "mercenary", "rogue", "mage" };
ConsumableInfo_t g_consumables[MAX_CONSUMABLES];
int              g_num_consumables = 0;
OpenableInfo_t   g_openables[MAX_DOORS];
int              g_num_openables = 0;
EquipmentInfo_t  g_equipment[MAX_EQUIPMENT];
int              g_num_equipment = 0;

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

static void LoadCharacterData( void )
{
  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( "resources/data/characters.duf", &root );

  if ( err != NULL )
  {
    printf( "DUF parse error at %d:%d - %s\n",
            err->line, err->column, d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return;
  }

  for ( int i = 0; i < 3; i++ )
  {
    dDUFValue_t* entry = d_DUFGetObjectItem( root, g_class_keys[i] );
    if ( entry == NULL ) continue;

    dDUFValue_t* name     = d_DUFGetObjectItem( entry, "name" );
    dDUFValue_t* hp       = d_DUFGetObjectItem( entry, "hp" );
    dDUFValue_t* dmg      = d_DUFGetObjectItem( entry, "base_damage" );
    dDUFValue_t* def      = d_DUFGetObjectItem( entry, "defense" );
    dDUFValue_t* ctype    = d_DUFGetObjectItem( entry, "consumable_type" );
    dDUFValue_t* desc     = d_DUFGetObjectItem( entry, "description" );
    dDUFValue_t* img_path = d_DUFGetObjectItem( entry, "image_path" );
    dDUFValue_t* glyph    = d_DUFGetObjectItem( entry, "glyph" );
    dDUFValue_t* color    = d_DUFGetObjectItem( entry, "color" );

    if ( name )  strncpy( g_classes[i].name, name->value_string, MAX_NAME_LENGTH - 1 );
    if ( hp )    g_classes[i].hp = (int)hp->value_int;
    if ( dmg )   g_classes[i].base_damage = (int)dmg->value_int;
    if ( def )   g_classes[i].defense = (int)def->value_int;
    if ( ctype ) strncpy( g_classes[i].consumable_type, ctype->value_string, MAX_NAME_LENGTH - 1 );
    if ( desc )  strncpy( g_classes[i].description, desc->value_string, 255 );
    if ( glyph ) strncpy( g_classes[i].glyph, glyph->value_string, 7 );
    g_classes[i].color = ParseDUFColor( color );

    if ( img_path && strlen( img_path->value_string ) > 0 )
    {
      g_classes[i].image = a_ImageLoad( img_path->value_string );
    }
  }

  d_DUFFree( root );
}

static void LoadConsumableData( void )
{
  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( "resources/data/consumables.duf", &root );

  if ( err != NULL )
  {
    printf( "DUF parse error at %d:%d - %s\n",
            err->line, err->column, d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return;
  }

  g_num_consumables = 0;
  for ( dDUFValue_t* entry = root->child; entry != NULL && g_num_consumables < MAX_CONSUMABLES; entry = entry->next )
  {
    ConsumableInfo_t* c = &g_consumables[g_num_consumables];
    memset( c, 0, sizeof( ConsumableInfo_t ) );

    dDUFValue_t* name     = d_DUFGetObjectItem( entry, "name" );
    dDUFValue_t* type     = d_DUFGetObjectItem( entry, "type" );
    dDUFValue_t* glyph    = d_DUFGetObjectItem( entry, "glyph" );
    dDUFValue_t* color    = d_DUFGetObjectItem( entry, "color" );
    dDUFValue_t* bdmg     = d_DUFGetObjectItem( entry, "bonus_damage" );
    dDUFValue_t* effect   = d_DUFGetObjectItem( entry, "effect" );
    dDUFValue_t* desc     = d_DUFGetObjectItem( entry, "description" );
    dDUFValue_t* img_path = d_DUFGetObjectItem( entry, "image_path" );

    if ( name )   strncpy( c->name, name->value_string, MAX_NAME_LENGTH - 1 );
    if ( type )   strncpy( c->type, type->value_string, MAX_NAME_LENGTH - 1 );
    if ( glyph )  strncpy( c->glyph, glyph->value_string, 7 );
    if ( effect ) strncpy( c->effect, effect->value_string, MAX_NAME_LENGTH - 1 );
    if ( desc )   strncpy( c->description, desc->value_string, 255 );
    if ( bdmg )   c->bonus_damage = (int)bdmg->value_int;
    c->color = ParseDUFColor( color );

    if ( img_path && strlen( img_path->value_string ) > 0 )
      c->image = a_ImageLoad( img_path->value_string );

    g_num_consumables++;
  }

  d_DUFFree( root );
}

static void LoadOpenableData( void )
{
  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( "resources/data/openables.duf", &root );

  if ( err != NULL )
  {
    printf( "DUF parse error at %d:%d - %s\n",
            err->line, err->column, d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return;
  }

  g_num_openables = 0;
  for ( dDUFValue_t* entry = root->child; entry != NULL && g_num_openables < MAX_DOORS; entry = entry->next )
  {
    OpenableInfo_t* o = &g_openables[g_num_openables];
    memset( o, 0, sizeof( OpenableInfo_t ) );

    dDUFValue_t* name     = d_DUFGetObjectItem( entry, "name" );
    dDUFValue_t* kind     = d_DUFGetObjectItem( entry, "kind" );
    dDUFValue_t* glyph    = d_DUFGetObjectItem( entry, "glyph" );
    dDUFValue_t* color    = d_DUFGetObjectItem( entry, "color" );
    dDUFValue_t* rtype    = d_DUFGetObjectItem( entry, "required_type" );
    dDUFValue_t* desc     = d_DUFGetObjectItem( entry, "description" );
    dDUFValue_t* img_path = d_DUFGetObjectItem( entry, "image_path" );

    if ( name )  strncpy( o->name, name->value_string, MAX_NAME_LENGTH - 1 );
    if ( kind )  strncpy( o->kind, kind->value_string, MAX_NAME_LENGTH - 1 );
    if ( glyph ) strncpy( o->glyph, glyph->value_string, 7 );
    if ( rtype ) strncpy( o->required_type, rtype->value_string, MAX_NAME_LENGTH - 1 );
    if ( desc )  strncpy( o->description, desc->value_string, 255 );
    o->color = ParseDUFColor( color );

    if ( img_path && strlen( img_path->value_string ) > 0 )
      o->image = a_ImageLoad( img_path->value_string );

    g_num_openables++;
  }

  d_DUFFree( root );
}

static void LoadEquipmentData( void )
{
  dDUFValue_t* root = NULL;
  dDUFError_t* err = d_DUFParseFile( "resources/data/equipment_starters.duf", &root );

  if ( err != NULL )
  {
    printf( "DUF parse error at %d:%d - %s\n",
            err->line, err->column, d_StringPeek( err->message ) );
    d_DUFErrorFree( err );
    return;
  }

  g_num_equipment = 0;
  for ( dDUFValue_t* entry = root->child; entry != NULL && g_num_equipment < MAX_EQUIPMENT; entry = entry->next )
  {
    EquipmentInfo_t* e = &g_equipment[g_num_equipment];
    memset( e, 0, sizeof( EquipmentInfo_t ) );

    dDUFValue_t* name    = d_DUFGetObjectItem( entry, "name" );
    dDUFValue_t* kind    = d_DUFGetObjectItem( entry, "kind" );
    dDUFValue_t* slot    = d_DUFGetObjectItem( entry, "slot" );
    dDUFValue_t* glyph   = d_DUFGetObjectItem( entry, "glyph" );
    dDUFValue_t* color   = d_DUFGetObjectItem( entry, "color" );
    dDUFValue_t* dmg     = d_DUFGetObjectItem( entry, "damage" );
    dDUFValue_t* def     = d_DUFGetObjectItem( entry, "defense" );
    dDUFValue_t* effect  = d_DUFGetObjectItem( entry, "effect" );
    dDUFValue_t* effval  = d_DUFGetObjectItem( entry, "effect_value" );
    dDUFValue_t* cls     = d_DUFGetObjectItem( entry, "class" );
    dDUFValue_t* desc    = d_DUFGetObjectItem( entry, "description" );

    if ( name )   strncpy( e->name, name->value_string, MAX_NAME_LENGTH - 1 );
    if ( kind )   strncpy( e->kind, kind->value_string, MAX_NAME_LENGTH - 1 );
    if ( slot )   strncpy( e->slot, slot->value_string, MAX_NAME_LENGTH - 1 );
    if ( glyph )  strncpy( e->glyph, glyph->value_string, 7 );
    if ( effect ) strncpy( e->effect, effect->value_string, MAX_NAME_LENGTH - 1 );
    if ( cls )    strncpy( e->class_name, cls->value_string, MAX_NAME_LENGTH - 1 );
    if ( desc )   strncpy( e->description, desc->value_string, 255 );
    if ( dmg )    e->damage = (int)dmg->value_int;
    if ( def )    e->defense = (int)def->value_int;
    if ( effval ) e->effect_value = (int)effval->value_int;
    e->color = ParseDUFColor( color );

    g_num_equipment++;
  }

  d_DUFFree( root );
}

void ItemsLoadAll( void )
{
  memset( g_classes, 0, sizeof( g_classes ) );
  memset( g_consumables, 0, sizeof( g_consumables ) );
  memset( g_openables, 0, sizeof( g_openables ) );
  memset( g_equipment, 0, sizeof( g_equipment ) );
  g_num_consumables = 0;
  g_num_openables = 0;
  g_num_equipment = 0;

  LoadCharacterData();
  LoadConsumableData();
  LoadOpenableData();
  LoadEquipmentData();
}

int ItemsBuildFiltered( int class_idx, FilteredItem_t* out, int max_out )
{
  int count = 0;
  if ( class_idx < 0 || class_idx > 2 ) return 0;

  const char* ctype = g_classes[class_idx].consumable_type;
  const char* ckey  = g_class_keys[class_idx];

  /* Add matching consumables */
  for ( int i = 0; i < g_num_consumables && count < max_out; i++ )
  {
    if ( strncmp( g_consumables[i].type, ctype, strlen( g_consumables[i].type ) ) == 0 )
    {
      out[count].type = FILTERED_CONSUMABLE;
      out[count].index = i;
      count++;
    }
  }

  /* Add matching openables */
  for ( int i = 0; i < g_num_openables && count < max_out; i++ )
  {
    if ( strcmp( g_openables[i].required_type, ckey ) == 0 ||
         strcmp( g_openables[i].required_type, "any" ) == 0 )
    {
      out[count].type = FILTERED_OPENABLE;
      out[count].index = i;
      count++;
    }
  }

  return count;
}

extern Player_t player;

int EquipSlotForKind( const char* kind )
{
  if ( strcmp( kind, "weapon" ) == 0 ) return EQUIP_WEAPON;
  if ( strcmp( kind, "armor" ) == 0 )  return EQUIP_ARMOR;
  if ( strcmp( kind, "trinket" ) == 0 )
  {
    if ( player.equipment[EQUIP_TRINKET1] == -1 ) return EQUIP_TRINKET1;
    return EQUIP_TRINKET2;
  }
  return -1;
}

void EquipStarterGear( const char* class_key )
{
  for ( int i = 0; i < g_num_equipment; i++ )
  {
    if ( strcmp( g_equipment[i].class_name, class_key ) != 0 ) continue;

    int slot = EquipSlotForKind( g_equipment[i].slot );
    if ( slot >= 0 )
      player.equipment[slot] = i;
  }
}

int InventoryAdd( int item_type, int index )
{
  for ( int i = 0; i < MAX_INVENTORY; i++ )
  {
    if ( player.inventory[i].type == INV_EMPTY )
    {
      player.inventory[i].type = item_type;
      player.inventory[i].index = index;
      return i;
    }
  }
  return -1;
}

void InventoryRemove( int slot )
{
  if ( slot < 0 || slot >= MAX_INVENTORY ) return;
  player.inventory[slot].type = INV_EMPTY;
  player.inventory[slot].index = 0;
}
