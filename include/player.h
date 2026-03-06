#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <Archimedes.h>
#include <Daedalus.h>

#define INV_COLS         4
#define INV_ROWS         5
#define MAX_INVENTORY   32       /* array size: 4×8 absolute max */

#define INV_EMPTY        0
#define INV_CONSUMABLE   1
#define INV_EQUIPMENT    2
#define INV_MAP          3

#define EQUIP_WEAPON    0
#define EQUIP_ARMOR     1
#define EQUIP_TRINKET1  2
#define EQUIP_TRINKET2  3
#define EQUIP_SLOTS     4

typedef struct { int type; int index; } InvSlot_t;

typedef struct
{
  int active;
  int bonus_damage;
  char effect[MAX_NAME_LENGTH];
  int heal;
} ConsumableBuff_t;

typedef struct
{
  char name[MAX_NAME_LENGTH];
  int hp;
  int max_hp;
  int class_damage;                     /* immutable base from class */
  int class_defense;                    /* immutable base from class */
  dStaticTable_t* stats;                /* computed stats: "damage", "defense" */
  char consumable_type[MAX_NAME_LENGTH];
  char description[256];
  char glyph[8];
  aColor_t color;
  aImage_t* image;

  float world_x;
  float world_y;

  InvSlot_t inventory[MAX_INVENTORY];   /* 4×8 max grid */
  int max_inventory;                    /* usable slots (starts 20, grows to 32) */
  int inv_cursor;
  int selected_consumable;

  int equipment[EQUIP_SLOTS];           /* index into g_equipment[], -1 = empty */
  int equip_cursor;
  int inv_focused;                      /* 1 = inventory panel, 0 = equipment panel */
  int turns_since_hit;
  int gold;
  ConsumableBuff_t buff;

  int first_strike_active;              /* 1 = ready, 0 = used this room */
  uint32_t fs_visited;                  /* bitfield: rooms that already gave first strike */
  int scroll_echo_counter;              /* scrolls used toward next free cast */
  int dodge_counter;                    /* incoming hits toward next dodge   */
  int attack_counter;                   /* outgoing attacks toward armor break */
  int last_room_id;                     /* detect room changes */
  int root_turns;                       /* forced skip turns (spider web etc.) */
} Player_t;

void PlayerInitStats( void );
void PlayerRecalcStats( void );
int  PlayerStat( const char* key );
int  PlayerEquipEffect( const char* name );

/* Gameplay state wrappers - funnel all mutations through these */
void PlayerFullReset( int class_index );
void PlayerTakeDamage( int amount );
void PlayerHeal( int amount );
void PlayerAddGold( int amount );
int  PlayerSpendGold( int amount );
void PlayerApplyBuff( int bonus_dmg, const char* effect, int heal );
void PlayerClearBuff( void );
void PlayerSetWorldPos( float x, float y );
void PlayerResetFirstStrike( void );
void PlayerConsumeFirstStrike( void );
void PlayerTickTurnsSinceHit( void );
void PlayerSetRoom( int room_id );
void PlayerEquip( int slot, int index );

#endif
