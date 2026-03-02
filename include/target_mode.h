#ifndef __TARGET_MODE_H__
#define __TARGET_MODE_H__

#include "world.h"
#include "game_viewport.h"
#include "console.h"
#include "enemies.h"

void TargetModeInit( World_t* w, Console_t* con, GameCamera_t* cam,
                     aSoundEffect_t* move, aSoundEffect_t* click );

/* Enter targeting mode for a consumable. inv_slot = inventory slot to remove on success. */
void TargetModeEnter( int consumable_idx, int inv_slot );
void TargetModeExit( void );
int  TargetModeActive( void );

/* Process input. Returns 1 if input was consumed.
   Sets confirmed flag internally - caller checks TargetModeConfirmed(). */
int  TargetModeLogic( Enemy_t* enemies, int num_enemies );

/* Returns 1 if target was just confirmed. Fills out row/col/consumable_idx/inv_slot. */
int  TargetModeConfirmed( int* row, int* col, int* consumable_idx, int* inv_slot );

/* Draw range highlights and cursor. */
void TargetModeDraw( aRectf_t vp_rect, GameCamera_t* cam, World_t* world );

#endif
