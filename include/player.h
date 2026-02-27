#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <Archimedes.h>

#define INV_COLS         4
#define INV_ROWS         5
#define MAX_INVENTORY   (INV_COLS * INV_ROWS)

#define INV_EMPTY        0
#define INV_CONSUMABLE   1
#define INV_EQUIPMENT    2

#define EQUIP_WEAPON    0
#define EQUIP_ARMOR     1
#define EQUIP_TRINKET1  2
#define EQUIP_TRINKET2  3
#define EQUIP_SLOTS     4

typedef struct { int type; int index; } InvSlot_t;

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

  float world_x;
  float world_y;

  InvSlot_t inventory[MAX_INVENTORY];   /* 4x5 grid */
  int inv_cursor;
  int selected_consumable;

  int equipment[EQUIP_SLOTS];           /* index into g_equipment[], -1 = empty */
  int equip_cursor;
  int inv_focused;                      /* 1 = inventory panel, 0 = equipment panel */
} Player_t;

#endif
