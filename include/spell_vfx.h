#ifndef __SPELL_VFX_H__
#define __SPELL_VFX_H__

#include <Archimedes.h>
#include "world.h"
#include "game_viewport.h"

void SpellVFXInit( World_t* w );
void SpellVFXUpdate( float dt );
void SpellVFXDraw( aRectf_t vp_rect, GameCamera_t* cam );
int  SpellVFXActive( void );

/* Spawn functions - called from game_events.c */
void SpellVFXSpark( float px, float py, float tx, float ty );
void SpellVFXFrost( float px, float py, float tx, float ty );
void SpellVFXFireball( float px, float py, int tgt_r, int tgt_c, int radius );
void SpellVFXSwap( float p1x, float p1y, float p2x, float p2y );

/* Camera shake offsets - add to draw_cam alongside CombatShakeOX/OY */
float SpellVFXShakeOX( void );
float SpellVFXShakeOY( void );

/* Screen flash overlay - draw after CombatFlashAlpha */
float    SpellVFXFlashAlpha( void );
aColor_t SpellVFXFlashColor( void );

#endif
