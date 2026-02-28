#ifndef __SOUND_MANAGER_H__
#define __SOUND_MANAGER_H__

#include <Archimedes.h>

#define MUSIC_FADE_MS  2000

void SoundManagerInit( void );
void SoundManagerUpdate( float dt );
void SoundManagerCrossfadeToGame( void );
void SoundManagerPlayMenu( void );
void SoundManagerPlayGame( void );
void SoundManagerPlayFootstep( void );
void SoundManagerStop( void );

#endif
