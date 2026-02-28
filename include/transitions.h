#ifndef __TRANSITIONS_H__
#define __TRANSITIONS_H__

/* === Outro (class_select → game_scene) === */

void  TransitionOutroStart( void );
int   TransitionOutroUpdate( float dt );  /* returns 1 if outro active */
void  TransitionOutroSkip( void );
int   TransitionOutroActive( void );
int   TransitionOutroDone( void );        /* 1 once outro finishes (read-once) */

/* Class select reads these during outro */
float TransitionGetOutroAlpha( void );    /* UI fade: 1 → 0 */
float TransitionGetOutroVPAlpha( void );  /* game viewport box: 0 → 1 */
float TransitionGetOutroCharOY( void );   /* character bounce offset */
float TransitionGetOutroDropT( void );    /* drop interpolation: 0 → 1 */
int   TransitionGetOutroFlipped( void );  /* 1 after the "turn" */

/* === Intro (game_scene UI slide-in) === */

void  TransitionIntroStart( void );
int   TransitionIntroUpdate( float dt );  /* returns 1 if intro active */
void  TransitionIntroSkip( void );
int   TransitionIntroActive( void );

/* Game scene reads these during intro */
float TransitionGetViewportAlpha( void );
float TransitionGetViewportH( void );
float TransitionGetTopBarOY( void );
float TransitionGetRightOX( void );
float TransitionGetConsoleOY( void );
float TransitionGetUIAlpha( void );
int   TransitionShowLabels( void );

#endif
