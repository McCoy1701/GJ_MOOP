#ifndef __ENEMIES_H__
#define __ENEMIES_H__

#include <Archimedes.h>

#define MAX_ENEMY_TYPES  32
#define MAX_ENEMIES      64

typedef struct
{
  char     key[MAX_NAME_LENGTH];
  char     name[MAX_NAME_LENGTH];
  char     glyph[8];
  int      hp;
  int      damage;
  int      defense;
  char     ai[MAX_NAME_LENGTH];
  char     description[256];
  int      range;
  int      sight_range;                 /* aggro range (Manhattan), default 6 */
  char     drop_item[MAX_NAME_LENGTH];  /* DUF key of consumable to drop on death */
  int      gold_drop;                  /* gold dropped on death (0 = none) */
  char     on_death[MAX_NAME_LENGTH];  /* death hazard, e.g. "poison_pool" */
  int      pool_duration;
  int      pool_damage;
  aColor_t color;
  aImage_t* image;
} EnemyType_t;

typedef struct Enemy_t
{
  int   type_idx;
  int   row, col;
  float world_x, world_y;
  int   hp;
  int   alive;
  int   facing_left;
  int   turns_since_hit;
  int   chase_turns;
  int   last_seen_row;
  int   last_seen_col;
  int   ai_state;
  int   ai_dir_row;
  int   ai_dir_col;
  /* Status effects */
  int   poison_ticks;
  int   poison_dmg;
  int   burn_ticks;
  int   burn_dmg;
  int   stun_turns;
  int   root_turns;
} Enemy_t;

extern EnemyType_t g_enemy_types[MAX_ENEMY_TYPES];
extern int         g_num_enemy_types;

void     EnemiesLoadTypes( void );
int      EnemyTypeByKey( const char* key );
void     EnemiesInit( Enemy_t* list, int* count );
Enemy_t* EnemySpawn( Enemy_t* list, int* count,
                     int type_idx, int row, int col,
                     int tile_w, int tile_h );
Enemy_t* EnemyAt( Enemy_t* list, int count, int row, int col );
int      EnemiesInCombat( Enemy_t* list, int count );

void EnemyBasicAITick( Enemy_t* e, int player_row, int player_col,
                   int (*walkable)(int,int),
                   Enemy_t* all, int count );

void EnemySkeletonTick( Enemy_t* e, int player_row, int player_col,
                        int (*walkable)(int,int),
                        Enemy_t* all, int count );

void EnemyShamanTick( Enemy_t* e, int player_row, int player_col,
                      int (*walkable)(int,int),
                      Enemy_t* all, int count );

/* enemies.c - enemy turn management (owns its own tween manager) */
#include "world.h"

void EnemiesSetWorld( World_t* w );
void EnemiesSetNPCs( void* npcs, int* num_npcs );
void EnemiesSetList( Enemy_t* list, int* count );
int  EnemyBlockedByNPC( int row, int col );
int  EnemyGridW( void );
int  EnemyGridH( void );

/* Shaman totem spawn helper (uses stored list/count) */
int  EnemyShamanSpawnTotem( int row, int col, int (*walkable)(int,int),
                            Enemy_t* all, int count );
void EnemiesUpdate( float dt );
int  EnemiesTurning( void );

/* Start an enemy turn: AI moves + attack. Call once per player action. */
void EnemiesStartTurn( Enemy_t* list, int count,
                       int player_row, int player_col,
                       int (*walkable)(int,int) );

/* rendering.c - draw all alive enemies */
#include "game_viewport.h"

void EnemiesDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                     Enemy_t* list, int count,
                     World_t* world, int gfx_mode );

void EnemiesDrawTelegraph( aRectf_t vp_rect, GameCamera_t* cam,
                           Enemy_t* list, int count,
                           World_t* world );

/* Arrow projectile VFX */
void EnemyProjectileInit( void );
void EnemyProjectileUpdate( float dt );
void EnemyProjectileSpawn( float sx, float sy, float ex, float ey,
                           int dir_row, int dir_col );
void EnemyProjectileDraw( aRectf_t vp_rect, GameCamera_t* cam );
int  EnemyProjectileInFlight( void );

#endif
