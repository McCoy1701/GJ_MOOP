#ifndef __LOOK_MODE_H__
#define __LOOK_MODE_H__

#include "world.h"
#include "game_viewport.h"
#include "console.h"

void LookModeInit( World_t* w, Console_t* con,
                   aSoundEffect_t* move, aSoundEffect_t* click );

/* Process look mode input.  Returns 1 if input was consumed. */
int  LookModeLogic( int mouse_moved );

/* Draw the look mode cursor. */
void LookModeDraw( aRectf_t vp_rect, GameCamera_t* cam );

int  LookModeActive( void );
void LookModeEnter( int start_row, int start_col );
void LookModeExit( void );
void LookModeGetCursor( int* row, int* col );

#endif
