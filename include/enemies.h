#ifndef __ENEMIES_H__
#define __ENEMIES_H__

#include <Archimedes.h>

#define MAX_ENEMY_TYPES  16
#define MAX_ENEMIES      32

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
  aColor_t color;
  aImage_t* image;
} EnemyType_t;

typedef struct
{
  int   type_idx;
  int   row, col;
  float world_x, world_y;
  int   hp;
  int   alive;
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

void EnemyRatTick( Enemy_t* e, int player_row, int player_col,
                   int (*walkable)(int,int),
                   Enemy_t* all, int count );

/* enemies.c — enemy turn management (owns its own tween manager) */
#include "world.h"

void EnemiesSetWorld( World_t* w );
void EnemiesUpdate( float dt );
int  EnemiesTurning( void );

/* Start an enemy turn: AI moves + attack. Call once per player action. */
void EnemiesStartTurn( Enemy_t* list, int count,
                       int player_row, int player_col,
                       int (*walkable)(int,int) );

/* rendering.c — draw all alive enemies */
#include "game_viewport.h"

void EnemiesDrawAll( aRectf_t vp_rect, GameCamera_t* cam,
                     Enemy_t* list, int count,
                     World_t* world, int gfx_mode );

#endif
