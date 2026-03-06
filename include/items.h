#ifndef __ITEMS_H__
#define __ITEMS_H__

#include <Archimedes.h>

#define MAX_CONSUMABLES   48
#define MAX_DOORS         8

#define FILTERED_CONSUMABLE 0
#define FILTERED_OPENABLE   1
#define FILTERED_EQUIPMENT  2

typedef struct
{
  char name[MAX_NAME_LENGTH];
  int hp;
  int base_damage;
  int defense;
  char consumable_type[MAX_NAME_LENGTH];
  char description[256];
  char glyph[8];
  aColor_t color;
  aImage_t* image;
} ClassInfo_t;

typedef struct
{
  char key[MAX_NAME_LENGTH];
  char name[MAX_NAME_LENGTH];
  char type[MAX_NAME_LENGTH];
  char glyph[8];
  aColor_t color;
  int bonus_damage;
  char effect[MAX_NAME_LENGTH];
  int range;
  int requires_los;
  int heal;
  int ticks;
  int tick_damage;
  int place_range;
  int radius;
  int duration;
  int aoe_radius;
  char description[256];
  char action[MAX_NAME_LENGTH];      /* UI label: "Lure", "Eat", etc. (empty = "Use") */
  char gives[MAX_NAME_LENGTH];       /* consumable key to add on use (empty = none) */
  char use_message[256];             /* custom console message on use (empty = none) */
  aImage_t* image;
} ConsumableInfo_t;

typedef struct
{
  char name[MAX_NAME_LENGTH];
  char kind[MAX_NAME_LENGTH];
  char glyph[8];
  aColor_t color;
  char required_type[MAX_NAME_LENGTH];
  char description[256];
  aImage_t* image;
} OpenableInfo_t;

#define MAX_EQUIPMENT     32

typedef struct
{
  char key[MAX_NAME_LENGTH];
  char name[MAX_NAME_LENGTH];
  char kind[MAX_NAME_LENGTH];       /* "weapon" / "armor" / "trinket" */
  char slot[MAX_NAME_LENGTH];
  char glyph[8];
  aColor_t color;
  int damage;
  int defense;
  char effect[MAX_NAME_LENGTH];
  int effect_value;
  char class_name[MAX_NAME_LENGTH]; /* which class gets this as starter */
  char description[256];
  aImage_t* image;
} EquipmentInfo_t;

typedef struct { int type; int index; } FilteredItem_t;

extern ClassInfo_t      g_classes[3];
extern const char*      g_class_keys[3];
extern ConsumableInfo_t g_consumables[MAX_CONSUMABLES];
extern int              g_num_consumables;
extern OpenableInfo_t   g_openables[MAX_DOORS];
extern int              g_num_openables;
extern EquipmentInfo_t  g_equipment[MAX_EQUIPMENT];
extern int              g_num_equipment;

void ItemsLoadAll( void );
int  ItemsBuildFiltered( int class_idx, FilteredItem_t* out, int max_out, int include_universal );

int  EquipSlotForKind( const char* kind );
int  EquipSlotForTrinket( int equip_idx );
void EquipStarterGear( const char* class_key );
int  InventoryAdd( int item_type, int index );
void InventoryRemove( int slot );

int  ConsumableByKey( const char* key );
int  EquipmentByKey( const char* key );

const char* PlayerClassKey( void );
int         EquipmentClassMatch( int eq_idx );

#endif
