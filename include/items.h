#ifndef __ITEMS_H__
#define __ITEMS_H__

#include <Archimedes.h>

#define MAX_CONSUMABLES   16
#define MAX_DOORS         8

#define FILTERED_CONSUMABLE 0
#define FILTERED_OPENABLE   1

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
  char name[MAX_NAME_LENGTH];
  char type[MAX_NAME_LENGTH];
  char glyph[8];
  aColor_t color;
  int bonus_damage;
  char effect[MAX_NAME_LENGTH];
  char description[256];
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

#define MAX_EQUIPMENT     16

typedef struct
{
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
int  ItemsBuildFiltered( int class_idx, FilteredItem_t* out, int max_out );

int  EquipSlotForKind( const char* kind );
void EquipStarterGear( const char* class_key );
int  InventoryAdd( int item_type, int index );
void InventoryRemove( int slot );

#endif
