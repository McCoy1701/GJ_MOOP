#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <Archimedes.h>

#define MAX_INVENTORY   8

#define EQUIP_WEAPON    0
#define EQUIP_ARMOR     1
#define EQUIP_TRINKET1  2
#define EQUIP_TRINKET2  3
#define EQUIP_SLOTS     4

typedef struct
{
  char name[MAX_NAME_LENGTH];
  int hp;
  int max_hp;
  int base_damage;
  int defense;
  char consumable_type[MAX_NAME_LENGTH];
  char description[256];
  aImage_t* image;

  int inventory[MAX_INVENTORY];
  int inventory_count;
  int selected_consumable;

  int equipment[EQUIP_SLOTS];   /* -1 = empty */
} Player_t;

#endif
