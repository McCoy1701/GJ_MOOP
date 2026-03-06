#ifndef __COMBAT_VFX_H__
#define __COMBAT_VFX_H__

#include <Archimedes.h>
#include "game_viewport.h"

void CombatVFXInit( void );
void CombatVFXUpdate( float dt );

/* Draw floating damage numbers - call after entities, before clip disable */
void CombatVFXDraw( aRectf_t vp_rect, GameCamera_t* cam );

/* Spawn a floating damage number at world position */
void CombatVFXSpawnNumber( float wx, float wy, int amount, aColor_t color );
void CombatVFXSpawnNumberScaled( float wx, float wy, int amount, aColor_t color, float scale );

/* Spawn floating text at world position (speech bubbles, etc.) */
void CombatVFXSpawnText( float wx, float wy, const char* text, aColor_t color );

/* Draw a health bar above an entity - only shows when hp < max_hp */
void CombatVFXDrawHealthBar( aRectf_t vp_rect, GameCamera_t* cam,
                              float wx, float wy,
                              int hp, int max_hp );

#endif
