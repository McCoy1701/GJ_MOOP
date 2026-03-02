#ifndef __DUNGEON_H__
#define __DUNGEON_H__

#include "world.h"
#include "game_viewport.h"
#include "enemies.h"
#include "npc.h"
#include "ground_items.h"

#define DUNGEON_W 100
#define DUNGEON_H 50

extern int g_current_floor;

/* Map format */
int  DungeonCharToRoomId( char c );
char DungeonRoomIdToChar( int id );
int  DungeonSaveMap( const char* path, const char** rows, int width, int height );

/* Builder */
void DungeonBuild( World_t* world );
void DungeonPlayerStart( float* wx, float* wy );

/* Spawner */
void DungeonSpawn( NPC_t* npcs, int* num_npcs,
                   Enemy_t* enemies, int* num_enemies,
                   GroundItem_t* items, int* num_items,
                   World_t* world );

/* Handler */
void DungeonHandlerInit( World_t* world );
void DungeonDrawProps( aRectf_t vp_rect, GameCamera_t* cam,
                       World_t* world, int gfx_mode );

#endif
