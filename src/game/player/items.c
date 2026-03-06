#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "items.h"
#include "maps.h"
#include "player.h"

ClassInfo_t      g_classes[3];
const char*      g_class_keys[3] = { "mercenary", "rogue", "mage" };
ConsumableInfo_t g_consumables[MAX_CONSUMABLES];
int              g_num_consumables = 0;

static aSoundEffect_t sfx_heal;
static int sfx_heal_loaded = 0;
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
      if ( access( img_path->value_string, F_OK ) != 0 )
      {
        fprintf( stderr, "FATAL: missing image '%s' for class '%s'\n",
                 img_path->value_string, g_classes[i].name );
        exit( 1 );
      }
      g_classes[i].image = a_ImageLoad( img_path->value_string );
    }
  }

  d_DUFFree( root );
}

static void ParseConsumableEntry( dDUFValue_t* entry )
{
  if ( g_num_consumables >= MAX_CONSUMABLES ) return;

  ConsumableInfo_t* c = &g_consumables[g_num_consumables];
  memset( c, 0, sizeof( ConsumableInfo_t ) );

  if ( entry->key )
    strncpy( c->key, entry->key, MAX_NAME_LENGTH - 1 );

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

  dDUFValue_t* v;
  if ( ( v = d_DUFGetObjectItem( entry, "range" ) ) )        c->range        = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "requires_los" ) ) ) c->requires_los = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "heal" ) ) )         c->heal         = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "ticks" ) ) )        c->ticks        = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "tick_damage" ) ) )  c->tick_damage  = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "place_range" ) ) )  c->place_range  = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "radius" ) ) )       c->radius       = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "duration" ) ) )     c->duration     = (int)v->value_int;
  if ( ( v = d_DUFGetObjectItem( entry, "aoe_radius" ) ) )   c->aoe_radius   = (int)v->value_int;

  dDUFValue_t* action      = d_DUFGetObjectItem( entry, "action" );
  dDUFValue_t* gives       = d_DUFGetObjectItem( entry, "gives" );
  dDUFValue_t* use_message = d_DUFGetObjectItem( entry, "use_message" );

  if ( action )      strncpy( c->action, action->value_string, MAX_NAME_LENGTH - 1 );
  if ( gives )       strncpy( c->gives, gives->value_string, MAX_NAME_LENGTH - 1 );
  if ( use_message ) strncpy( c->use_message, use_message->value_string, 255 );

  if ( img_path && strlen( img_path->value_string ) > 0 )
  {
    if ( access( img_path->value_string, F_OK ) != 0 )
    {
      fprintf( stderr, "FATAL: missing image '%s' for consumable '%s'\n",
               img_path->value_string, c->name );
      exit( 1 );
    }
    c->image = a_ImageLoad( img_path->value_string );
  }

  g_num_consumables++;
}

static void LoadConsumableDUF( const char* path )
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

  for ( dDUFValue_t* entry = root->child; entry != NULL; entry = entry->next )
    ParseConsumableEntry( entry );

  d_DUFFree( root );
}

static void LoadConsumableData( void )
{
  g_num_consumables = 0;
  LoadConsumableDUF( "resources/data/consumables.duf" );
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
    {
      if ( access( img_path->value_string, F_OK ) != 0 )
      {
        fprintf( stderr, "FATAL: missing image '%s' for openable '%s'\n",
                 img_path->value_string, o->name );
        exit( 1 );
      }
      o->image = a_ImageLoad( img_path->value_string );
    }

    g_num_openables++;
  }

  d_DUFFree( root );
}

static void LoadEquipmentDUF( const char* path )
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

  for ( dDUFValue_t* entry = root->child; entry != NULL && g_num_equipment < MAX_EQUIPMENT; entry = entry->next )
  {
    EquipmentInfo_t* e = &g_equipment[g_num_equipment];
    memset( e, 0, sizeof( EquipmentInfo_t ) );

    if ( entry->key )
      strncpy( e->key, entry->key, MAX_NAME_LENGTH - 1 );

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
    dDUFValue_t* desc     = d_DUFGetObjectItem( entry, "description" );
    dDUFValue_t* img_path = d_DUFGetObjectItem( entry, "image_path" );

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

    if ( img_path && strlen( img_path->value_string ) > 0 )
    {
      if ( access( img_path->value_string, F_OK ) != 0 )
      {
        fprintf( stderr, "FATAL: missing image '%s' for equipment '%s'\n",
                 img_path->value_string, e->name );
        exit( 1 );
      }
      e->image = a_ImageLoad( img_path->value_string );
    }

    g_num_equipment++;
  }

  d_DUFFree( root );
}

static int g_num_starters = 0;

static void LoadEquipmentData( void )
{
  g_num_equipment = 0;
  LoadEquipmentDUF( "resources/data/equipment_starters.duf" );
  g_num_starters = g_num_equipment;
  LoadEquipmentDUF( "resources/data/equipment_shop.duf" );
  LoadEquipmentDUF( "resources/data/equipment_drops.duf" );
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
  MapsLoadAll();
}

int ItemsBuildFiltered( int class_idx, FilteredItem_t* out, int max_out, int include_universal )
{
  int count = 0;
  if ( class_idx < 0 || class_idx > 2 ) return 0;

  const char* ctype = g_classes[class_idx].consumable_type;
  const char* ckey  = g_class_keys[class_idx];

  /* Add matching consumables */
  for ( int i = 0; i < g_num_consumables && count < max_out; i++ )
  {
    int match = strncmp( g_consumables[i].type, ctype, strlen( g_consumables[i].type ) ) == 0;
    if ( !match && include_universal )
      match = strcmp( g_consumables[i].type, "potion" ) == 0 ||
              strcmp( g_consumables[i].type, "quest" ) == 0;
    if ( match )
    {
      out[count].type = FILTERED_CONSUMABLE;
      out[count].index = i;
      count++;
    }
  }

  /* Add matching trinkets (skip starters - shown separately) */
  for ( int i = g_num_starters; i < g_num_equipment && count < max_out; i++ )
  {
    if ( strcmp( g_equipment[i].kind, "trinket" ) == 0 &&
         strcmp( g_equipment[i].class_name, ckey ) == 0 )
    {
      out[count].type = FILTERED_EQUIPMENT;
      out[count].index = i;
      count++;
    }
  }

  return count;
}

extern Player_t player;

void PlayerInitStats( void )
{
  const char* k1 = "damage";
  const char* k2 = "defense";
  const void* keys[]   = { &k1, &k2 };
  int zero = 0;
  const void* values[] = { &zero, &zero };

  player.stats = d_InitStaticTable(
    sizeof( const char* ), sizeof( int ),
    d_HashString, d_CompareString,
    4, keys, values, 2
  );
}

void PlayerRecalcStats( void )
{
  int total_dmg = player.class_damage;
  int total_def = player.class_defense;

  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    if ( player.equipment[i] < 0 ) continue;
    total_dmg += g_equipment[ player.equipment[i] ].damage;
    total_def += g_equipment[ player.equipment[i] ].defense;
  }

  const char* k_dmg = "damage";
  const char* k_def = "defense";
  d_StaticTableSet( player.stats, &k_dmg, &total_dmg );
  d_StaticTableSet( player.stats, &k_def, &total_def );
}

int PlayerStat( const char* key )
{
  int* v = (int*)d_StaticTableGet( player.stats, &key );
  return v ? *v : 0;
}

int PlayerEquipEffect( const char* name )
{
  int total = 0;
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    if ( player.equipment[i] < 0 ) continue;
    EquipmentInfo_t* eq = &g_equipment[player.equipment[i]];
    if ( strcmp( eq->effect, name ) == 0 )
      total += eq->effect_value;
  }
  return total;
}

int PlayerEquipEffectMin( const char* name )
{
  int best = 0;
  int count = 0;
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    if ( player.equipment[i] < 0 ) continue;
    EquipmentInfo_t* eq = &g_equipment[player.equipment[i]];
    if ( strcmp( eq->effect, name ) == 0 )
    {
      if ( count == 0 || eq->effect_value < best )
        best = eq->effect_value;
      count++;
    }
  }
  if ( count >= 2 ) best /= 2;
  return best > 0 ? best : 0;
}

int EquipSlotForKind( const char* kind )
{
  if ( strcmp( kind, "weapon" ) == 0 ) return EQUIP_WEAPON;
  if ( strcmp( kind, "armor" ) == 0 )  return EQUIP_ARMOR;
  if ( strcmp( kind, "trinket" ) == 0 )
  {
    if ( player.equipment[EQUIP_TRINKET1] == -1 ) return EQUIP_TRINKET1;
    if ( player.equipment[EQUIP_TRINKET2] == -1 ) return EQUIP_TRINKET2;
    return EQUIP_TRINKET2;
  }
  return -1;
}

int EquipSlotForTrinket( int equip_idx )
{
  const char* new_eff = g_equipment[equip_idx].effect;

  /* Block if the other trinket slot already has the same effect */
  if ( player.equipment[EQUIP_TRINKET1] >= 0
       && strcmp( g_equipment[player.equipment[EQUIP_TRINKET1]].effect, new_eff ) == 0 )
    return EQUIP_TRINKET1;  /* swap into same slot */
  if ( player.equipment[EQUIP_TRINKET2] >= 0
       && strcmp( g_equipment[player.equipment[EQUIP_TRINKET2]].effect, new_eff ) == 0 )
    return EQUIP_TRINKET2;  /* swap into same slot */

  /* First empty slot, or default to slot 2 */
  if ( player.equipment[EQUIP_TRINKET1] == -1 ) return EQUIP_TRINKET1;
  if ( player.equipment[EQUIP_TRINKET2] == -1 ) return EQUIP_TRINKET2;
  return EQUIP_TRINKET2;
}

void EquipStarterGear( const char* class_key )
{
  for ( int i = 0; i < g_num_starters; i++ )
  {
    if ( strcmp( g_equipment[i].class_name, class_key ) != 0 ) continue;

    int slot = EquipSlotForKind( g_equipment[i].slot );
    if ( slot >= 0 )
      player.equipment[slot] = i;
  }
}

int InventoryAdd( int item_type, int index )
{
  for ( int i = 0; i < player.max_inventory; i++ )
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

int ConsumableByKey( const char* key )
{
  for ( int i = 0; i < g_num_consumables; i++ )
    if ( strcmp( g_consumables[i].key, key ) == 0 ) return i;
  return -1;
}

int EquipmentByKey( const char* key )
{
  for ( int i = 0; i < g_num_equipment; i++ )
    if ( strcmp( g_equipment[i].key, key ) == 0 ) return i;
  return -1;
}

const char* PlayerClassKey( void )
{
  for ( int i = 0; i < 3; i++ )
  {
    if ( strcmp( player.name, g_classes[i].name ) == 0 )
      return g_class_keys[i];
  }
  return "";
}

int EquipmentClassMatch( int eq_idx )
{
  if ( eq_idx < 0 || eq_idx >= g_num_equipment ) return 0;
  if ( g_equipment[eq_idx].class_name[0] == '\0' ) return 1;
  return strcmp( g_equipment[eq_idx].class_name, PlayerClassKey() ) == 0;
}

/* ---- Player state wrappers ---- */

void PlayerFullReset( int class_index )
{
  ClassInfo_t* c = &g_classes[class_index];
  strncpy( player.name, c->name, MAX_NAME_LENGTH - 1 );
  player.hp = c->hp;
  player.max_hp = c->hp;
  player.turns_since_hit = 99;
  player.class_damage = c->base_damage;
  player.class_defense = c->defense;
  strncpy( player.consumable_type, c->consumable_type, MAX_NAME_LENGTH - 1 );
  strncpy( player.description, c->description, 255 );
  strncpy( player.glyph, c->glyph, 7 );
  player.color = c->color;
  player.image = c->image;
  player.world_x = 64.0f;
  player.world_y = 64.0f;
  memset( player.inventory, 0, sizeof( player.inventory ) );
  player.max_inventory = 20;  /* base capacity */
  memset( &player.buff, 0, sizeof( ConsumableBuff_t ) );
  player.inv_cursor = 0;
  player.selected_consumable = 0;
  player.equip_cursor = 0;
  player.inv_focused = 1;
  player.gold = 0;
  player.first_strike_active = 1;
  player.fs_visited = 0;
  player.scroll_echo_counter = 0;
  player.dodge_counter = 0;
  player.attack_counter = 0;
  player.root_turns = 0;
  player.max_health_ups = 0;
  player.last_room_id = -1;
  for ( int i = 0; i < EQUIP_SLOTS; i++ )
    player.equipment[i] = -1;
}

void PlayerTakeDamage( int amount )
{
  player.hp -= amount;
  player.turns_since_hit = 0;
  if ( player.hp <= 0 ) player.hp = 0;
}

void PlayerHeal( int amount )
{
  if ( !sfx_heal_loaded )
  {
    a_AudioLoadSound( "resources/soundeffects/heal.wav", &sfx_heal );
    sfx_heal_loaded = 1;
  }
  player.hp += amount;
  if ( player.hp > player.max_hp ) player.hp = player.max_hp;
  a_AudioPlaySound( &sfx_heal, NULL );
}

void PlayerAddGold( int amount )
{
  player.gold += amount;
}

int PlayerSpendGold( int amount )
{
  if ( player.gold < amount ) return 0;
  player.gold -= amount;
  return 1;
}

void PlayerApplyBuff( int bonus_dmg, const char* effect, int heal )
{
  player.buff.active = 1;
  player.buff.bonus_damage = bonus_dmg;
  strncpy( player.buff.effect, effect, MAX_NAME_LENGTH - 1 );
  player.buff.heal = heal;
}

void PlayerClearBuff( void )
{
  memset( &player.buff, 0, sizeof( ConsumableBuff_t ) );
}

void PlayerSetWorldPos( float x, float y )
{
  player.world_x = x;
  player.world_y = y;
}

void PlayerResetFirstStrike( void )
{
  player.first_strike_active = 1;
}

void PlayerConsumeFirstStrike( void )
{
  player.first_strike_active = 0;
}

void PlayerTickTurnsSinceHit( void )
{
  player.turns_since_hit++;
}

void PlayerSetRoom( int room_id )
{
  player.last_room_id = room_id;
  if ( room_id >= 0 && room_id < 32 && !( player.fs_visited & ( 1u << room_id ) ) )
  {
    player.fs_visited |= ( 1u << room_id );
    player.first_strike_active = 1;
  }
}

void PlayerEquip( int slot, int index )
{
  if ( slot >= 0 && slot < EQUIP_SLOTS )
    player.equipment[slot] = index;
}
